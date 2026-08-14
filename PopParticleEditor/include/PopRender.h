#ifndef __POP_RENDER_H__
#define __POP_RENDER_H__

#include <WXBase/WXDefinitions.h>
#include <imgui_internal.h>

namespace pop
{
	void RenderGridBackground(const ImRect& rect,
		const ImVec2& gridSize,
		float zoom = 1.0f,
		ImVec2 viewOffset = { 0.0f, 0.0f });
}

#endif // !__POP_RENDER_H__
