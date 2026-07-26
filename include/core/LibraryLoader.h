// include/core/LibraryLoader.h
#pragma once

#include <vector>
#include <string>

namespace core {

	class ServiceManager;

	// Платформо-независимый тип хендла динамической библиотеки
	using LibHandle = void*;

	struct LoadedLibrary {
		LibHandle lib = nullptr;
		std::string path;
	};

	class LibraryLoader {
	public:
		LibraryLoader() = default;
		~LibraryLoader();

		// Найти все DLL/so/dylib в папке и загрузить их
		int discoverAndLoad(const std::string& directory);

		// Загрузить конкретную библиотеку по пути
		bool loadLibrary(const std::string& path);

		// Вызвать во всех загруженных DLL функцию инициализации
		bool initializeAll(ServiceManager& services);

		// Выгрузить все библиотеки из памяти
		void unloadAll();

	private:
		std::vector<LoadedLibrary> libraries_;
	};

} // namespace core
