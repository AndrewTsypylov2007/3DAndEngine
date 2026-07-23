#pragma once

#include <string>
#include <tuple>

namespace core {

// Very small SemVer parser and comparator supporting MAJOR.MINOR.PATCH
struct SemVer {
	int major{0};
	int minor{0};
	int patch{0};

	static SemVer parse(const std::string &s);

	std::tuple<int,int,int> toTuple() const { return {major, minor, patch}; }
};

inline bool operator==(const SemVer &a, const SemVer &b) {
	return a.toTuple() == b.toTuple();
}
inline bool operator<(const SemVer &a, const SemVer &b) {
	return a.toTuple() < b.toTuple();
}
inline bool operator<=(const SemVer &a, const SemVer &b) { return !(b < a); }

} // namespace core
