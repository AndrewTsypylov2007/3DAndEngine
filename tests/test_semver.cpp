#include "TestHarness.h"
#include "../include/core/SemVer.h"
#include <iostream>

void run_semver_tests() {
	using core::SemVer;
	auto v1 = SemVer::parse("1.2.3");
	CHECK(v1.major == 1);
	CHECK(v1.minor == 2);
	CHECK(v1.patch == 3);

	auto v2 = SemVer::parse("1.3.0");
	CHECK(v1 < v2);

	auto v3 = SemVer::parse("1.2.3");
	CHECK(v1 == v3);
	std::cout << "semver tests passed\n";
}
