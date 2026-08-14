#include "../include/PopRender.h"

namespace
{
	float PositiveMod(float value, float period)
	{
		float m = fmodf(value, period);
		return (m < 0.0f) ? (m + period) : m;
	}
}

namespace pop
{
	void RenderGridBackground(const ImRect& rect,
		const ImVec2& gridSize,
		float zoom,
		ImVec2 viewOffset)
	{
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		if (!drawList) return;

		const ImU32 bgColor = IM_COL32(18, 18, 24, 255);
		const ImU32 minorGridCol = IM_COL32(35, 35, 48, 255);
		const ImU32 majorGridCol = IM_COL32(50, 50, 68, 255);
		const ImU32 dotColor = IM_COL32(70, 70, 95, 255);

		const float lineThickness = 1.0f;
		const int   majorStep = 5;

		const float scaledGridX = gridSize.x * zoom;
		const float scaledGridY = gridSize.y * zoom;
		const float majorScaledX = scaledGridX * majorStep;
		const float majorScaledY = scaledGridY * majorStep;

		// drawList->AddCircle(rect.Min, 20.0f, 0xFFFFFFFF);

		drawList->AddRectFilled(rect.Min, rect.Max, bgColor);

		if (scaledGridX < 4.0f && scaledGridY < 4.0f) return;

		drawList->PushClipRect(rect.Min, rect.Max, true);

		auto GetVisibleRange = [&](float minScreen, float maxScreen, float step, float offset) -> std::pair<int, int> {
			int startIdx = (int)floorf((minScreen - offset) / step);
			int endIdx = (int)ceilf((maxScreen - offset) / step);
			return { startIdx, endIdx };
			};

		if (scaledGridX >= 4.0f)
		{
			auto [startIdx, endIdx] = GetVisibleRange(rect.Min.x, rect.Max.x, scaledGridX, viewOffset.x);

			for (int i = startIdx; i <= endIdx; ++i)
			{
				if (i % majorStep == 0) continue;

				float x = viewOffset.x + i * scaledGridX;

				if (x >= rect.Min.x && x <= rect.Max.x)
					drawList->AddLine({ x, rect.Min.y }, { x, rect.Max.y }, minorGridCol, lineThickness);
			}
		}

		if (scaledGridY >= 4.0f)
		{
			auto [startIdx, endIdx] = GetVisibleRange(rect.Min.y, rect.Max.y, scaledGridY, viewOffset.y);

			for (int i = startIdx; i <= endIdx; ++i)
			{
				if (i % majorStep == 0) continue;

				float y = viewOffset.y + i * scaledGridY;
				if (y >= rect.Min.y && y <= rect.Max.y)
					drawList->AddLine({ rect.Min.x, y }, { rect.Max.x, y }, minorGridCol, lineThickness);
			}
		}

		if (majorScaledX >= 4.0f)
		{
			auto [startIdx, endIdx] = GetVisibleRange(rect.Min.x, rect.Max.x, majorScaledX, viewOffset.x);

			for (int i = startIdx; i <= endIdx; ++i)
			{
				float x = viewOffset.x + i * majorScaledX;
				if (x >= rect.Min.x && x <= rect.Max.x)
					drawList->AddLine({ x, rect.Min.y }, { x, rect.Max.y }, majorGridCol, lineThickness);
			}
		}

		if (majorScaledY >= 4.0f)
		{
			auto [startIdx, endIdx] = GetVisibleRange(rect.Min.y, rect.Max.y, majorScaledY, viewOffset.y);

			for (int i = startIdx; i <= endIdx; ++i)
			{
				float y = viewOffset.y + i * majorScaledY;
				if (y >= rect.Min.y && y <= rect.Max.y)
					drawList->AddLine({ rect.Min.x, y }, { rect.Max.x, y }, majorGridCol, lineThickness);
			}
		}
		if (majorScaledX >= 10.0f && majorScaledY >= 10.0f)
		{
			auto [startIdxX, endIdxX] = GetVisibleRange(rect.Min.x, rect.Max.x, majorScaledX, viewOffset.x);
			auto [startIdxY, endIdxY] = GetVisibleRange(rect.Min.y, rect.Max.y, majorScaledY, viewOffset.y);

			for (int i = startIdxX; i <= endIdxX; ++i)
			{
				float x = viewOffset.x + i * majorScaledX;
				if (x < rect.Min.x || x > rect.Max.x) continue;

				for (int j = startIdxY; j <= endIdxY; ++j)
				{
					float y = viewOffset.y + j * majorScaledY;
					if (y >= rect.Min.y && y <= rect.Max.y)
						drawList->AddCircleFilled({ x, y }, 1.5f, dotColor);
				}
			}
		}

		drawList->PopClipRect();
	}
}
