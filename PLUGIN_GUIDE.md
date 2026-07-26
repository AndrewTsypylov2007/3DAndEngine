# Как писать плагин для 3DAndEngine Core (кратко)

Файл содержит минимальную и понятную инструкцию, как создать плагин‑библиотеку, собрать и протестировать её с ядром.

1) Контракт плагина
- Экспортируемые C-функции (обязательные):
  - CreatePlugin() -> core::IPlugin*  // создаёт экземпляр
  - DestroyPlugin(core::IPlugin*)     // удаляет экземпляр
  - GetPluginManifest() -> const char* // возвращает JSON-манифест (nul-terminated)

2) Минимальный skeleton (plugin.cpp)
```cpp
#include "core/IPlugin.h"
#include "core/ServiceManager.h"
#include <iostream>

namespace core {
class MyPlugin : public IPlugin {
public:
  bool initialize(ServiceManager &services) override {
	std::cerr << "[MyPlugin] initialize\n";
	return true; // вернуть false при ошибке
  }
  void shutdown() override {
	std::cerr << "[MyPlugin] shutdown\n";
  }
};
}

extern "C" {
  core::IPlugin* CreatePlugin() { return new core::MyPlugin(); }
  void DestroyPlugin(core::IPlugin* p) { delete p; }
  const char* GetPluginManifest() {
	return R"({"name":"my_plugin","version":"0.0.1","core_version_required":"0.0.1","enabled":true})";
  }
}
```

3) Пример CMakeLists.txt для плагина (plugins/my_plugin/CMakeLists.txt)
```cmake
add_library(my_plugin SHARED plugin.cpp)
target_include_directories(my_plugin PRIVATE ${CMAKE_SOURCE_DIR}/include)

# Копируем плагин рядом с engine_core после сборки
add_custom_command(TARGET my_plugin POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:my_plugin> $<TARGET_FILE_DIR:engine_core>
  COMMENT "Copy my_plugin to engine_core output directory")
```

4) Манифест (GetPluginManifest) — поля
- name (string)
- version (semver string)
- core_version_required (semver string)
- enabled (bool) — если false, ядро пропустит плагин при discover

5) Сборка и тест
- Сборка всего проекта (core + плагин):
  cmake -S . -B build
  cmake --build build --config Debug
- После сборки плагин окажется рядом с engine_core (post-build copy); запустите engine_core и смотрите логи — должно появиться [MyPlugin] initialize.

6) Советы
- Собирайте плагин теми же настройками ABI/компилятора и vcpkg triplet, что и core.
- initialize должен возвращать false при ошибке — тогда Core выполнит откат (rollback).
- shutdown должен безопасно освобождать ресурсы и корректно работать при повторных вызовах.
- Для регистрации сервисов используйте services.registerService<T, Deps...>(shared_ptr<T>) внутри initialize.

7) Дополнительно
- Для отключения плагина без удаления файла поставьте "enabled": false в manifest.
- Чтобы сделать плагин с зависимостями (glm, assimp и т.д.), добавьте find_package / target_link_libraries в CMakeLists плагина и используйте vcpkg.

Готовый шаблон можно скопировать в plugins/new_plugin и подставить своё имя/логику.
