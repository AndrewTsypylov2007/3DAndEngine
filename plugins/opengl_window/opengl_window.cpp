#include "core/IPlugin.h"
#include "core/ServiceManager.h"
#include "core/EventBus.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>

namespace core {
	class OpenGLWindowPlugin : public IPlugin {
	public:
		OpenGLWindowPlugin() = default;
		~OpenGLWindowPlugin() override = default;

		bool initialize(ServiceManager &services) override {
			std::cerr << "[OpenGLWindow] initialize\n";

			if (!glfwInit()) {
				std::cerr << "[OpenGLWindow] glfwInit failed\n";
				return false;
			}

			glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
			glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
			glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

			window_ = glfwCreateWindow(800, 600, "3DAndEngine - OpenGL", nullptr, nullptr);
			if (!window_) {
				std::cerr << "[OpenGLWindow] glfwCreateWindow failed\n";
				glfwTerminate();
				return false;
			}

			glfwMakeContextCurrent(window_);

			if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
				std::cerr << "[OpenGLWindow] Failed to initialize GLAD\n";
				glfwDestroyWindow(window_);
				glfwTerminate();
				window_ = nullptr;
				return false;
			}

			glViewport(0, 0, 800, 600);

			// Subscribe to tick events to render
			auto eb = services.getService<core::EventBus>();
			if (eb) {
				handlerId_ = eb->subscribe("tick", [this]() {
					if (!window_) return;
					glfwPollEvents();
					glClearColor(0.1f, 0.2f, 0.3f, 1.0f);
					glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
					glfwSwapBuffers(window_);
				});
				eventBus_ = eb;
			}

			return true;
		}

		void shutdown() override {
			std::cerr << "[OpenGLWindow] shutdown\n";
			if (eventBus_ && handlerId_) {
				eventBus_->unsubscribe("tick", handlerId_);
				handlerId_ = 0;
			}
			if (window_) {
				glfwDestroyWindow(window_);
				window_ = nullptr;
			}
			glfwTerminate();
		}

	private:
		GLFWwindow* window_ = nullptr;
		core::EventHandlerId handlerId_ = 0;
		std::shared_ptr<core::EventBus> eventBus_;
	};
}

extern "C" {
	core::IPlugin* CreatePlugin() { return new core::OpenGLWindowPlugin(); }
	void DestroyPlugin(core::IPlugin* p) { delete p; }
	const char* GetPluginManifest() {
		return R"({"name":"opengl_window","version":"0.0.1","core_version_required":"0.0.1","enabled":true})";
	}
}
