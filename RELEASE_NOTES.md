Release v0.0.1
================

Дата релиза: 2026-07-23

Кратко
-----
Первая публичная сборка ядра 3DAndEngine Core. Включает:

- Базовую инфраструктуру ядра (Application, ServiceManager, EventBus).
- Плагинную систему: рекурсивный discover (plugins/**), manifest JSON с полем `enabled`, SemVer проверка совместимости, безопасная инициализация плагинов и откат при ошибках.
- LoggerService (spdlog) и ConfigService (nlohmann::json).
- TaskScheduler (пул потоков) как сервис.
- Unit-тесты и CI (GitHub Actions) с vcpkg, кэшированием, мультиплатформенной сборкой (Linux/Windows/macOS), публикацией артефактов и автоматической упаковкой релизов.

Подробности
---------
- Плагинный контракт: экспорт CreatePlugin / DestroyPlugin / GetPluginManifest. Manifest должен содержать поля name, version, core_version_required и опционально enabled (boolean).
- ServiceManager поддерживает фазы init/start/postStart/stop и обнаруживает циклы зависимостей.
- TaskScheduler поддерживает submit/enqueue и безопасный graceful shutdown.

Как выпустить релиз
-------------------
1. Обновить версию core (в PluginLoader.cpp уже установлен currentCoreVersionStr = "0.0.1").
2. Закоммитить изменения и создать тег:
   - git add .
   - git commit -m "Release v0.0.1"
   - git tag -a v0.0.1 -m "v0.0.1"
   - git push origin main --follow-tags
3. Создать релиз на GitHub (через UI или gh CLI). CI автоматически упакует релизные артефакты и прикрепит zip к релизу.

Известные ограничения и следующие шаги
--------------------------------------
- Нет полного набора интеграционных тестов для плагинов; рекомендуется добавить их.
- Не реализованы механизм подписи плагинов и sandboxing — планируется добавить.
- Рекомендуется провести кроссплатформенное тестирование бинарников и собрать релизные артефакты для Windows/macOS отдельно.

Спасибо за использование 3DAndEngine Core — добро пожаловать в сообщество!
