#include "../include/core/Application.h"
#include <iostream>

int main(int argc, char **argv) {
	core::Application app;
	std::cerr << "engine_core starting...\n";
	int rc = app.run();
	std::cerr << "engine_core exited with code " << rc << "\n";
	return rc;
}
