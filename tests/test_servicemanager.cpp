// tests/test_servicemanager.cpp
#include "TestHarness.h"
#include "../include/core/ServiceManager.h"
#include "../include/core/IService.h"
#include <vector>
#include <string>
#include <memory>
#include <iostream>

using namespace core;

struct LifecycleRecorder {
	std::vector<std::string> order;
};

struct BaseService : IService {
	LifecycleRecorder& rec;
	BaseService(LifecycleRecorder& r) : rec(r) {}

	bool init(ServiceManager&) override { rec.order.push_back("Base.init"); return true; }
	void start() override { rec.order.push_back("Base.start"); }
	void postStart() override { rec.order.push_back("Base.post"); }
	void stop() override { rec.order.push_back("Base.stop"); }
};

struct DependentService : IService {
	LifecycleRecorder& rec;
	DependentService(LifecycleRecorder& r) : rec(r) {}

	bool init(ServiceManager&) override { rec.order.push_back("Dep.init"); return true; }
	void start() override { rec.order.push_back("Dep.start"); }
	void postStart() override { rec.order.push_back("Dep.post"); }
	void stop() override { rec.order.push_back("Dep.stop"); }
};

void run_servicemanager_tests() {
	// Тест 1: Проверка правильного порядка 3-х фаз жизненного цикла и LIFO остановки
	{
		LifecycleRecorder rec;
		ServiceManager mgr;

		auto base = std::make_shared<BaseService>(rec);
		auto dep = std::make_shared<DependentService>(rec);

		// Регистрируем сервисы. DependentService зависит от BaseService
		mgr.registerService<BaseService>(base);
		mgr.registerService<DependentService, BaseService>(dep);

		mgr.startAll();

		// Проверяем прямой топологический порядок по фазам
		REQUIRE(rec.order.size() == 6);
		CHECK(rec.order[0] == "Base.init");
		CHECK(rec.order[1] == "Dep.init");
		CHECK(rec.order[2] == "Base.start");
		CHECK(rec.order[3] == "Dep.start");
		CHECK(rec.order[4] == "Base.post");
		CHECK(rec.order[5] == "Dep.post");

		rec.order.clear();
		mgr.stopAll();

		// Проверяем обратный порядок остановки (LIFO): сначала должен тушиться зависимый сервис!
		REQUIRE(rec.order.size() == 2);
		CHECK(rec.order[0] == "Dep.stop");
		CHECK(rec.order[1] == "Base.stop");
	}

	// Тест 2: Проверка детекции циклов графа зависимостей
	{
		struct SA : IService { bool init(ServiceManager&) override { return true; } };
		struct SB : IService { bool init(ServiceManager&) override { return true; } };

		ServiceManager mgr2;
		auto sa = std::make_shared<SA>();
		auto sb = std::make_shared<SB>();

		mgr2.registerService<SA, SB>(sa); // A зависит от B
		mgr2.registerService<SB, SA>(sb); // B зависит от A

		CHECK_THROWS_AS(mgr2.startAll(), std::runtime_error);
	}

	std::cout << "servicemanager tests passed successfully\n";
}
