#ifndef __WX_IMGUI_FRAME_H__
#define __WX_IMGUI_FRAME_H__

namespace wx
{
	class ImGuiFrame
	{
	public:
		ImGuiFrame() {};
		virtual ~ImGuiFrame() {};

	public:
		virtual void OnRender() = 0;
	};
}

#endif // !__WX_IMGUI_FRAME_H__
