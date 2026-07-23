#include "TestHarness.h"
#include "../include/core/ServiceManager.h"
#include "../include/core/IService.h"
#include <vector>
#include <string>
#include <memory>
#include <iostream>

using namespace core;

struct Recorder { std::vector<std::string> calls; };

struct SvcA : IService {
	Recorder &r; SvcA(Recorder &rr): r(rr) {}
	bool init(ServiceManager &) override { r.calls.push_back("A.init"); return true; }
	void start() override { r.calls.push_back("A.start"); }
	void postStart() override { r.calls.push_back("A.post"); }
	void stop() override { r.calls.push_back("A.stop"); }
};

struct SvcB : IService {
	Recorder &r; SvcB(Recorder &rr): r(rr) {}
	bool init(ServiceManager &) override { r.calls.push_back("B.init"); return true; }
	void start() override { r.calls.push_back("B.start"); }
	void postStart() override { r.calls.push_back("B.post"); }
	void stop() override { r.calls.push_back("B.stop"); }
};

void run_servicemanager_tests() {
	Recorder rec;
	ServiceManager mgr;
	auto a = std::make_shared<SvcA>(rec);
	auto b = std::make_shared<SvcB>(rec);
	mgr.registerService<SvcA>(a);
	mgr.registerService<SvcB, SvcA>(b);
	try { mgr.startAll(); } catch (...) { fail("startAll threw", __FILE__, __LINE__); }
	CHECK(rec.calls.size() >= 2);
	CHECK(rec.calls[0] == "A.init");
	CHECK(rec.calls[1] == "B.init");
	rec.calls.clear();
	try { mgr.stopAll(); } catch (...) { fail("stopAll threw", __FILE__, __LINE__); }

	// cycle test
	struct SA : IService { bool init(ServiceManager&) override { return true; } };
	struct SB : IService { bool init(ServiceManager&) override { return true; } };
	ServiceManager mgr2;
	auto sa = std::make_shared<SA>();
	auto sb = std::make_shared<SB>();
	mgr2.registerService<SA, SB>(sa);
	mgr2.registerService<SB, SA>(sb);
	CHECK_THROWS_AS(mgr2.startAll(), std::runtime_error);
	std::cout << "servicemanager tests passed\n";
}
