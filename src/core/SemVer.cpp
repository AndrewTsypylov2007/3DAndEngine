#include "../../include/core/SemVer.h"
#include <sstream>

namespace core {

SemVer SemVer::parse(const std::string &s) {
	SemVer v;
	std::istringstream in(s);
	char dot;
	if (!(in >> v.major)) return v;
	if (!(in >> dot) || dot != '.') return v;
	if (!(in >> v.minor)) { v.minor = 0; return v; }
	if (!(in >> dot) || dot != '.') return v;
	if (!(in >> v.patch)) { v.patch = 0; }
	return v;
}

} // namespace core
