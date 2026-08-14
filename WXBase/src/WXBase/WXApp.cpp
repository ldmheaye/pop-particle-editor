#include "../../include/WXBase/WXApp.h"
#include <thread>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <nfd.h>

namespace
{
	void AsyncWindowIcon(GLFWwindow* window)
	{
#ifdef _WIN32
		// 比较奇怪但是不管了
		HICON hIcon = LoadIconW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(101));
		HWND hwnd = glfwGetWin32Window(window);
		SendMessageW(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
		SendMessageW(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
#endif
	}
}

namespace wx
{
	class AppWindow
	{
	public:
		AppWindow():_window(nullptr), _dpi_scale(1.0f), _imgui_setup(nullptr){};
		~AppWindow() {};

	private:
		friend void AppInit(const AppCreateInfo& info);
		friend void AppStart();
		friend void AppQuit();

		bool _initialize(Int32 width, Int32 height, std::string_view title,
			ImGuiSetupCallback imguiSetup)
		{
			if (_window)
				return false;

			_imgui_setup = imguiSetup;

			glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
			glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
			glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
			glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
			glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
			glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);
			_window = glfwCreateWindow(width, height, title.data(), nullptr, nullptr);
			glfwMakeContextCurrent(_window);

			AsyncWindowIcon(_window);

			if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
			{
				glfwDestroyWindow(_window);
				_window = nullptr;
				return false;
			}

			if (_window)
			{
				float xscale = 1.0f, yscale = 1.0f;
				glfwGetWindowContentScale(_window, &xscale, &yscale);
				_dpi_scale = xscale;
			}

			return _window;
		}

		void _start()
		{
			if (!_window)
				return;

			IMGUI_CHECKVERSION();
			ImGui::CreateContext();

			_setup_imgui_io(ImGui::GetIO());
			_setup_imgui_styles(ImGui::GetStyle());
			 if (_imgui_setup)
			 	_imgui_setup(_dpi_scale);

			ImGui_ImplGlfw_InitForOpenGL(_window, true);
			ImGui_ImplOpenGL3_Init("#version 330");

			while (!glfwWindowShouldClose(_window))
			{
				glfwPollEvents();

				ImGui_ImplOpenGL3_NewFrame();
				ImGui_ImplGlfw_NewFrame();
				ImGui::NewFrame();

				for (auto& frame : gImGuiFrameArray)
					frame->OnRender();

				ImGui::Render();

				glClearColor(0, 0, 0, 1.0f);
				glClear(GL_COLOR_BUFFER_BIT);
				ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
				glfwSwapBuffers(_window);
			}
		}

		void _quit()
		{
			if (!_window)
				return;
			glfwDestroyWindow(_window);
			_window = nullptr;
		}

		void _setup_imgui_io(ImGuiIO& io)
		{
			io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
			io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
			io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

			io.ConfigDpiScaleFonts = true;
		}

		void _setup_imgui_styles(ImGuiStyle& style)
		{
			ImGui::StyleColorsDark();
			style.ScaleAllSizes(_dpi_scale);
		}

	private:
		GLFWwindow* _window;
		Float32 _dpi_scale;
		ImGuiSetupCallback _imgui_setup;
	};

	AppWindow gAppWindow;

	void AppInit(const AppCreateInfo& info)
	{
		glfwInit();
		NFD_Init();
		// glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
		gAppWindow._initialize(info.width, info.height, info.captain.c_str(), info.imguiSetup);
	}

	void AppStart()
	{
		gAppWindow._start();
	}

	void AppQuit()
	{
		gAppWindow._quit();
	}
}
