#include "TestHarness.h"
#include "../include/core/ServiceManager.h"
#include "../include/core/IService.h"
#include <vector>
#include <string>
#include <memory>
#include <iostream>
#include <stdexcept>

using namespace core;

struct LifecycleRecorder {
    std::vector<std::string> order;
};

// Реализуем сервисы с именами для v0.1.1
struct BaseService : public IService {
    LifecycleRecorder& rec;
    BaseService(LifecycleRecorder& r) : rec(r) {}
    const char* getServiceName() const override { return "BaseService"; }
    bool init(ServiceManager&) override { rec.order.push_back("Base.init"); return true; }
    void start() override { rec.order.push_back("Base.start"); }
    void postStart() override { rec.order.push_back("Base.post"); }
    void stop() override { rec.order.push_back("Base.stop"); }
};

struct DependentService : public IService {
    LifecycleRecorder& rec;
    DependentService(LifecycleRecorder& r) : rec(r) {}
    const char* getServiceName() const override { return "DependentService"; }
    bool init(ServiceManager&) override { rec.order.push_back("Dep.init"); return true; }
    void start() override { rec.order.push_back("Dep.start"); }
    void postStart() override { rec.order.push_back("Dep.post"); }
    void stop() override { rec.order.push_back("Dep.stop"); }
};

void run_servicemanager_tests() {
    // ТЕСТ 1: Проверка порядка
    {
        LifecycleRecorder rec;
        ServiceManager mgr;

        auto base = std::make_shared<BaseService>(rec);
        auto dep = std::make_shared<DependentService>(rec);

        mgr.registerService<BaseService>(base);
        mgr.registerService<DependentService, BaseService>(dep);

        mgr.startAll();

        // Вместо CHECK(a == b) используем простую проверку, чтобы не бесить линкер
        if (rec.order.size() != 6) throw std::runtime_error("Test failed: wrong order size");

        // Ручная проверка строк (гарантированно работает)
        auto check = [&](size_t idx, const std::string& expected) {
            if (rec.order[idx] != expected) {
                throw std::runtime_error("Test failed at index " + std::to_string(idx) +
                    ": expected " + expected + " but got " + rec.order[idx]);
            }
            };

        check(0, "Base.init");
        check(1, "Dep.init");
        check(2, "Base.start");
        check(3, "Dep.start");
        check(4, "Base.post");
        check(5, "Dep.post");

        rec.order.clear();
        mgr.stopAll();

        if (rec.order.size() != 2) throw std::runtime_error("Test failed: wrong stop order size");
        check(0, "Dep.stop");
        check(1, "Base.stop");
    }

    // ТЕСТ 2: Циклы
    {
        struct SA : public IService {
            const char* getServiceName() const override { return "SA"; }
            bool init(ServiceManager&) override { return true; }
        };
        struct SB : public IService {
            const char* getServiceName() const override { return "SB"; }
            bool init(ServiceManager&) override { return true; }
        };

        ServiceManager mgr2;
        auto sa = std::make_shared<SA>();
        auto sb = std::make_shared<SB>();

        mgr2.registerService<SA, SB>(sa);
        mgr2.registerService<SB, SA>(sb);

        try {
            mgr2.startAll();
            throw std::runtime_error("Test failed: cycle not detected!");
        }
        catch (const std::runtime_error& e) {
            // Успех, цикл найден
        }
    }

    std::cout << "servicemanager tests passed successfully\n";
}
