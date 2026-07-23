#pragma once
#include <stdexcept>
#include <string>
#include <iostream>

inline void fail(const char* expr, const char* file, int line) {
	std::cerr << "Test failed: " << expr << " at " << file << ":" << line << "\n";
	throw std::runtime_error("Test failed");
}

#define CHECK(expr) do { if (!(expr)) fail(#expr, __FILE__, __LINE__); } while(0)
#define REQUIRE(expr) CHECK(expr)

#define CHECK_THROWS_AS(stmt, ex) do { bool _th = false; try { stmt; } catch (const ex&) { _th = true; } catch (...) { } if (!_th) fail("expected exception", __FILE__, __LINE__); } while(0)
