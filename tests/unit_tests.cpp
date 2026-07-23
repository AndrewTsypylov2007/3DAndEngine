#include <iostream>

void run_semver_tests();
void run_servicemanager_tests();

// Define main only when UNIT_TESTS_MAIN is set. This avoids duplicate main
// if tests are accidentally compiled into the application target.
#ifdef UNIT_TESTS_MAIN
int main() {
	try {
		run_semver_tests();
		run_servicemanager_tests();
	} catch (const std::exception &e) {
		std::cerr << "Unit tests failed: " << e.what() << "\n";
		return 1;
	}
	std::cout << "All unit tests passed\n";
	return 0;
}
#endif
