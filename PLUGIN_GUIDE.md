# Developer Guide: Authoring Plugins for 3DAndEngine Core (ABI 0x00040000)

Плагин — это динамическая библиотека (`.dll` / `.so` / `.dylib`), которую движок находит и загружает сам.
Ядро и плагин общаются только через C-ABI структуру `core::PluginInterface` и `core::EngineContext`
(см. `include/core/PluginContract.h`).

---

## 1. Контракт плагина

Библиотека должна экспортировать **одну** функцию с C-линковкой:

```cpp
extern "C" core::PluginInterface* GetPluginAPI();
```

Проще всего сделать это макросом `DECLARE_ENGINE_PLUGIN(instance)`, который возвращает указатель на глобальный объект `PluginInterface`.

| Поле `PluginInterface` | Назначение |
| :--- | :--- |
| `name`, `version` | Строки для логов (`version` — версия самого плагина). |
| `abi_version` | Обязательно `core::ENGINE_ABI_VERSION`. При несовпадении плагин **отклоняется**. |
| `priority` | Порядок запуска: чем меньше число, тем раньше плагин вызывается. |
| `on_load(EngineContext*)` | Вызывается один раз при регистрации. Может быть `nullptr`. |
| `on_fixed_update(float fixed_dt)` | Детерминированная логика с фиксированным шагом (по умолчанию 1/60 с). Может быть `nullptr`. |
| `on_update(float delta_time)` | Покадровая логика. **Обязателен**: без него загрузчик отклонит плагин. |
| `on_render(float alpha)` | Отрисовка; `alpha` — коэффициент интерполяции между физическими шагами. Может быть `nullptr`. |
| `on_unload()` | Вызывается при завершении работы. Может быть `nullptr`. |

> **Важно про `EngineContext`.** Указатель, который приходит в `on_load`, указывает на временный объект и становится недействительным после возврата из функции.
> Не сохраняйте сам указатель — сохраните **копию** структуры (`g_ctx = *ctx;`). Она состоит из указателей на подсистемы и безопасна для копирования.

Через `EngineContext` доступны: `ecs` (`EcsRegistry`), `event_bus`, `job_system`, а также таблицы функций
`input`, `audio`, `renderer`, `assets`, `logger` (они могут быть `nullptr`, если соответствующий сервис не зарегистрирован),
и вспомогательные `getSystem<T>()` / `registerSystem<T>()` / `getFrameData<T>()` / `setFrameData<T>()`.

---

## 2. Минимальный рабочий плагин (`my_plugin.cpp`)

Этот пример проверен: он собирается без предупреждений (`-Wall -Wextra -Wpedantic -Werror`), загружается движком и вызывает `on_load` / `on_unload`.

```cpp
#include "core/PluginContract.h"
#include "core/EcsRegistry.h"
#include "core/EventBus.h"

#include <cstdio>

namespace {

struct Position { float x = 0.0f; float y = 0.0f; };
struct Velocity { float vx = 0.0f; float vy = 0.0f; };

// ВАЖНО: ядро передает в on_load указатель на временный объект, который живет только
// на время вызова. Храните КОПИЮ структуры (она состоит из указателей и безопасна для копирования).
core::EngineContext g_ctx{};
core::Entity g_player{};

void on_load(core::EngineContext* ctx) {
    g_ctx = *ctx;
    g_player = ctx->ecs->createEntity();
    ctx->ecs->addComponent<Position>(g_player, Position{});
    ctx->ecs->addComponent<Velocity>(g_player, Velocity{ 10.0f, 0.0f });
    std::printf("[MyPlugin] Loaded, player entity created\n");
}

// Детерминированная логика: вызывается с фиксированным шагом (по умолчанию 1/60 с)
void on_fixed_update(float fixed_dt) {
    auto* velocities = g_ctx.ecs->view<Velocity>();
    if (!velocities) return;

    velocities->each([&](core::Entity entity, Velocity& vel) {
        if (auto* pos = g_ctx.ecs->getComponent<Position>(entity)) {
            pos->x += vel.vx * fixed_dt;
            pos->y += vel.vy * fixed_dt;
        }
    });
}

// Обязательный колбэк: без него загрузчик отклонит плагин
void on_update(float /*delta_time*/) {}

void on_render(float /*alpha*/) {}

void on_unload() {
    if (auto* pos = g_ctx.ecs->getComponent<Position>(g_player)) {
        std::printf("[MyPlugin] Unloading, player x = %.2f\n", pos->x);
    }
    g_ctx = core::EngineContext{};
}

core::PluginInterface g_plugin = {
    "MyPlugin",                  // name
    "1.0.0",                     // version
    core::ENGINE_ABI_VERSION,    // abi_version (должна совпадать с ядром)
    100,                         // priority (меньше = раньше)
    on_load,
    on_fixed_update,
    on_update,
    on_render,
    on_unload
};

} // namespace

DECLARE_ENGINE_PLUGIN(g_plugin)
```

---

## 3. Сборка

### Вручную (Linux, GCC / Clang)
```bash
g++ -std=c++20 -fPIC -shared -fvisibility=hidden -I path/to/3DAndEngine/include my_plugin.cpp -o libmy_plugin.so
```

### Через CMake (отдельный проект плагина)
```cmake
cmake_minimum_required(VERSION 3.20)
project(MyPlugin LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(my_plugin SHARED my_plugin.cpp)
target_include_directories(my_plugin PRIVATE path/to/3DAndEngine/include)
```

Для MSVC используйте те же флаги, что и у ядра: `/std:c++20 /permissive-`.

---

## 4. Установка и обнаружение плагинов

Движок ищет плагины **рекурсивно** в папке `plugins` рядом с исполняемым файлом
(`<build>/bin/plugins` при сборке через CMake). Если её нет, используется `./plugins` в текущей рабочей директории.

1. Скопируйте собранную библиотеку в `plugins/` (подпапки допускаются).
2. Запустите движок: `3DAndEngine` (или `3DAndEngine --ci` для headless-прогона на 30 кадров).
3. В логе должны появиться строки:
   ```
   [PluginManager] Верифицирован модуль: [MyPlugin] v1.0.0 (Приоритет: 100)
   [MyPlugin] Loaded, player entity created
   ```

Загрузчик пропускает файл и пишет предупреждение, если: нет экспорта `GetPluginAPI`, функция вернула `nullptr`,
`abi_version` не совпадает с ядром или не задан `on_update`.

---

## 5. Рекомендации

*   **Единый тулчейн.** Собирайте плагин тем же компилятором, с теми же флагами и стандартом C++20, что и ядро. Хотя интерфейс плагина — это C-структура, `EcsRegistry`, `EventBus` и `JobSystem` — это C++-классы из заголовков, и их раскладка в памяти должна совпадать.
*   **Обрабатывайте ошибки внутри плагина.** Ядро оборачивает вызовы колбэков в `try/catch` и пишет ошибку в лог, но не стоит полагаться на это как на штатный механизм.
*   **Общее состояние держите в `EngineContext`** (ECS, шина событий, `registerSystem` / `setFrameData`), а не в глобальных переменных, на которые рассчитывают другие плагины: модули не должны знать друг о друге.
*   **Порядок вызова** определяется `priority` (меньше — раньше). Порядок плагинов с одинаковым приоритетом не гарантируется.
*   **Остановка движка.** Событие с идентификатором `"engine/exit"` или `"app/quit"` останавливает главный цикл: `g_ctx.event_bus->dispatchImmediate(core::hash_str("app/quit"));`

---
*Версия ABI меняется при несовместимых изменениях `PluginContract.h`; плагины со старым ABI нужно пересобирать.*
