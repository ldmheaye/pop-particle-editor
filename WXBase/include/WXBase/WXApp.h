#ifndef __WX_APP_H__
#define __WX_APP_H__

#include "WXDefinitions.h"
#include "WXImGuiFrame.h"
#include <vector>
#include <memory>

namespace wx
{
	using ImGuiSetupCallback = void (*)(Float32 dpiScale);

	struct AppCreateInfo
	{
		Int32 width = 1280;
		Int32 height = 720;
		std::string captain = "Client";
		ImGuiSetupCallback imguiSetup = nullptr;
	};

	inline std::vector<std::unique_ptr<ImGuiFrame>> gImGuiFrameArray;

	void AppInit(const AppCreateInfo& info);

	template<typename T>
	void AppAppendFrame()
	{
		gImGuiFrameArray.emplace_back(std::make_unique<T>());
	}

	void AppStart();

	void AppQuit();
}

#endif // !__WX_APP_H__
