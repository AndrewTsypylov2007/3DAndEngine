# 3DAndEngine — Core

Подробная документация по проекту, архитектуре ядра и инструкции по сборке, тестированию и публикации релизов.

Цель проекта
---
3DAndEngine core — минимальное модульное ядро для 3D-движка. Ядро предоставляет инфраструктуру (жизненный цикл приложения, менеджер сервисов, шину событий, планировщик задач) и систему плагинов (динамические библиотеки). Ядро не содержит реализаций графики/физики/игровой логики — эти функции реализуются в плагинах.

Ключевые характеристики
---
- Модульная архитектура: ядро не зависит от конкретных подсистем, они подключаются как плагины.
- Плагинная система: автодискавери, manifest (JSON), проверка версии (SemVer), безопасная инициализация и откат.
- ServiceManager: фазы init/start/postStart/stop, топологическая сортировка по зависимостям и детекция циклов.
- TaskScheduler: пул потоков для фоновых задач, graceful shutdown.
- CI: GitHub Actions (Ubuntu/Windows/macOS) с vcpkg, кэшированием и публикацией артефактов.

Быстрый старт
---
Требования: CMake >= 3.14, компилятор C++17, рекомендован vcpkg для зависимостей.

Сборка (рекомендуется через vcpkg):

PowerShell / bash:

cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake -DBUILD_TESTS=ON
cmake --build build --config Debug
ctest --test-dir build --output-on-failure -C Debug

Запуск:
- Положите плагины в папку `plugins/` или убедитесь, что они находятся рядом с `engine_core` (CMake копирует `sample_plugin` автоматически).
- Запустите исполняемый `engine_core`.

Структура репозитория и описание файлов
---
- CMakeLists.txt — сборка core как статической библиотеки и exe; опция `BUILD_TESTS` управляет тестами.
- vcpkg.json — манифест зависимостей (fmt, spdlog, nlohmann-json и др.).
- include/core/ — публичные заголовки ядра:
  - IPlugin.h — контракт плагина: интерфейс IPlugin, C-типовые фабрики CreatePlugin/DestroyPlugin и GetPluginManifest.
  - IService.h — интерфейс сервисов (init/start/postStart/stop).
  - ServiceManager.h — регистрация сервисов, получение сервисов и lifecycle (startAll/stopAll) с зависимостями.
  - EventBus.h — простая шина событий (subscribe/publish/unsubscribe).
  - PluginLoader.h — API загрузчика плагинов: loadPlugin, discoverAndLoad, initializeAll, unloadAll.
  - TaskScheduler.h — пул потоков как сервис (submit/enqueue и lifecycle).
  - LoggerService.h, ConfigService.h — примеры инфраструктурных сервисов (spdlog, nlohmann::json).

- src/core/ — реализации соответствующих модулей:
  - Application.cpp — жизненный цикл приложения: регистрация сервисов, опциональное автодискавери плагинов, запуск lifecycle.
  - PluginLoader.cpp — кроссплатформенная загрузка библиотек (LoadLibrary/dlopen), чтение манифеста, SemVer-валидация, Create/Destroy, initializeAll + rollback.
  - ServiceManager.cpp — топологическая сортировка, init/start/postStart/stop и детекция циклов.
  - EventBus.cpp — реализация шины событий.
  - TaskScheduler.cpp — пул потоков, workerLoop, graceful shutdown.
  - LoggerService.cpp / ConfigService.cpp — реализации сервисов.

- plugins/sample_plugin/ — пример плагина:
  - sample_plugin.cpp — экспортирует CreatePlugin, DestroyPlugin, GetPluginManifest (JSON);
  - CMakeLists.txt — сборка shared lib и копирование в output.

- tests/ — простые unit тесты и локальный тест-раннер (опционально собираются с BUILD_TESTS=ON).

- .github/workflows/ci.yml — CI: сборка на Ubuntu/Windows/macOS, bootstrap vcpkg, кэширование, публикация артефактов и релизов.

Детальное описание API и поведения
---
IPlugin (include/core/IPlugin.h)
- Методы:
  - virtual bool initialize(ServiceManager &services) — инициализация плагина (регистрирует свои сервисы, подписки и т.д.). Возвращает true при успехе.
  - virtual void shutdown() — корректное освобождение ресурсов.
- Экспортируемые функции (C linkage):
  - CreatePlugin() -> IPlugin* — фабрика.
  - DestroyPlugin(IPlugin*) — удаление.
  - GetPluginManifest() -> const char* — JSON-манифест плагина (nul-terminated).

Plugin manifest (JSON)
- Обязательные / рекомендуемые поля:
  - name — строка
  - version — строка (semver)
  - core_version_required — минимальная версия core (semver)
  - enabled — boolean (если false, плагин будет пропущен)

Пример:
```json
{"name":"sample_plugin","version":"0.1.0","core_version_required":"0.1.0","enabled":true}
```

PluginLoader — поведение
- discoverAndLoad(directory) — рекурсивный скан каталога; читает manifest (GetPluginManifest) предварительно, пропускает plugins с enabled=false.
- loadPlugin(path) — открывает библиотеку, читает manifest, проверяет SemVer совм., ищет CreatePlugin/DestroyPlugin, вызывает CreatePlugin и сохраняет LoadedPlugin.
- initializeAll(services) — вызывает initialize() у всех загруженных плагинов; при ошибке выполняет rollback: shutdown уже инициализированных, DestroyPlugin, выгрузка библиотек и очистка списка.

ServiceManager — lifecycle
- registerService<T, Deps...>(shared_ptr<T>) — регистрирует сервис и его зависимости (по типам).
- getService<T>() — возвращает shared_ptr<T> если зарегистрирован.
- startAll() — топологическая сортировка по deps_, затем для каждого сервиса в порядке: init(serviceManager) -> start(); после запуска всех вызывается postStart() для каждого.
- stopAll() — вызывает stop() в обратном порядке.
- При обнаружении циклической зависимости выбрасывается std::runtime_error с описанием цикла.

TaskScheduler
- Реализован как IService; запускается в start(), останавливается в stop() (graceful, идемпотентный).
- submit(f,args...) -> future; enqueue(fn) для void задач.

ConfigService / LoggerService
- ConfigService читает config.json (если есть) и предоставляет nlohmann::json объект.
- LoggerService обёртка над spdlog; создаётся при старте, если разрешён в конфиге.

Конфигурация (config.json)
- Пример:
```json
{
  "core": {
	"enable_logger": true,
	"enable_plugin_discovery": true,
	"enable_scheduler": true,
	"scheduler_threads": 8
  }
}
```
- Core сначала пытается загрузить config.json, затем решает, регистрировать ли опциональные сервисы (logger, scheduler) и выполнять discover.

Рекомендации для разработки плагинов
- Собирать с теми же настройками ABI (vcpkg, triplet).
- Указывать core_version_required.
- В initialize() раскрывать только внутренние ресурсы, регистрировать сервисы через services.registerService<...>().
- Использовать LoggerService для логов.

CI и релизы
- CI соберёт проект на Ubuntu/Windows/macOS, установит зависимости через vcpkg, выполнит тесты, загрузит артефакты.
- При создании релиза workflow соберёт Release-артефакт (zip) и прикрепит его к релизу.

Команды (PowerShell)
- cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=path\to\vcpkg\scripts\buildsystems\vcpkg.cmake -DBUILD_TESTS=ON
- cmake --build build --config Debug
- ctest --test-dir build --output-on-failure -C Debug

Лицензия
- Добавьте LICENSE (например MIT) в корень — я могу добавить шаблон ниже.

Если хотите — добавлю README ещё более детально (разбор каждого .cpp/.h с отмечанием публичных методов) или с диаграммой последовательности. Скажите, что предпочитаете.
