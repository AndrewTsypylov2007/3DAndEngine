// tests/test_servicemanager.cpp — Версия v0.2.0 (Interface-Based Test Suite)
#include "../include/core/ServiceManager.h"
#include "../include/core/IServiceManager.h"
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

static LifecycleRecorder* g_currentTestRecorder = nullptr;

struct BaseService : public IService {
    const char* getServiceName() const override { return "BaseService"; }

    // ФИКС сигнатуры: IServiceManager&
    bool init(IServiceManager&) override {
        if (g_currentTestRecorder) g_currentTestRecorder->order.push_back("Base.init");
        return true;
    }
    void start() override { if (g_currentTestRecorder) g_currentTestRecorder->order.push_back("Base.start"); }
    void stop() override { if (g_currentTestRecorder) g_currentTestRecorder->order.push_back("Base.stop"); }
};

struct DependentService : public IService {
    const char* getServiceName() const override { return "DependentService"; }

    // ФИКС сигнатуры: IServiceManager&
    bool init(IServiceManager&) override {
        if (g_currentTestRecorder) g_currentTestRecorder->order.push_back("Dep.init");
        return true;
    }
    void start() override { if (g_currentTestRecorder) g_currentTestRecorder->order.push_back("Dep.start"); }
    void stop() override { if (g_currentTestRecorder) g_currentTestRecorder->order.push_back("Dep.stop"); }
};

void run_servicemanager_tests() {
    // ТЕСТ 1: Топологический порядок (init -> start)
    {
        LifecycleRecorder rec;
        g_currentTestRecorder = &rec;

        ServiceManager mgr; // Внутренний тип ядра

        auto base = std::make_unique<BaseService>();
        auto dep = std::make_unique<DependentService>();

        mgr.registerService<BaseService>(std::move(base));
        mgr.registerService<DependentService, BaseService>(std::move(dep));

        mgr.startAll();

        if (rec.order.size() != 4) throw std::runtime_error("Test 1 failed: wrong order size");

        auto check = [&](size_t idx, const std::string& expected) {
            if (rec.order[idx] != expected) {
                throw std::runtime_error("Test 1 error at " + std::to_string(idx) + ": expected " + expected);
            }
            };

        check(0, "Base.init");
        check(1, "Dep.init");
        check(2, "Base.start");
        check(3, "Dep.start");

        rec.order.clear();
        mgr.stopAll();

        g_currentTestRecorder = nullptr;
    }

    // ТЕСТ 2: Детекция циклов DFS
    {
        struct SA : public IService {
            const char* getServiceName() const override { return "SA"; }
            bool init(IServiceManager&) override { return true; }
            void start() override {}
            void stop() override {}
        };
        struct SB : public IService {
            const char* getServiceName() const override { return "SB"; }
            bool init(IServiceManager&) override { return true; }
            void start() override {}
            void stop() override {}
        };

        ServiceManager mgr2;
        mgr2.registerService<SA, SB>(std::make_unique<SA>());
        mgr2.registerService<SB, SA>(std::make_unique<SB>());

        try {
            mgr2.startAll();
            throw std::runtime_error("Test 2 failed: cycle loop not broken!");
        }
        catch (const std::runtime_error&) {
            std::cout << "[Test Success] Interface-based DFS successfully broke deadlock." << std::endl;
        }
    }

    std::cout << "All v0.2.0 ServiceManager tests passed.\n";
}
