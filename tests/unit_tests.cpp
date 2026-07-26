// tests/unit_tests.cpp
#include <iostream>

// Оставляем только объявление живых тестов менеджера сервисов
void run_servicemanager_tests();

#ifdef UNIT_TESTS_MAIN
int main() {
	try {
		// Запускаем только тесты ServiceManager (проверка 3-х фаз и циклов графа)
		run_servicemanager_tests();
	}
	catch (const std::exception& e) {
		std::cerr << "Unit tests failed: " << e.what() << "\n";
		return 1;
	}
	std::cout << "All unit tests passed successfully!\n";
	return 0;
}
#endif
