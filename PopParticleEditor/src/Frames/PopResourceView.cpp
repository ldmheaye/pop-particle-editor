#include "../../include/Frames/PopResourceView.h"
#include "../../include/Fonts/IconsFontAwesome7.h"
#include "../../include/PopState.h"

#include <imgui.h>

namespace pop
{
	PopResourceView::PopResourceView()
		:_icon_size(100.0f)
	{

	}

	PopResourceView::~PopResourceView()
	{

	}

	void PopResourceView::OnRender()
	{
		ImGui::BeginChild("##asset_grid", ImVec2(0, 0), true,
			ImGuiWindowFlags_HorizontalScrollbar);
		wx::Float32 itemSpacing = ImGui::GetStyle().ItemInnerSpacing.x;
		wx::Int32 columnNum = (ImGui::GetContentRegionAvail().x + itemSpacing)
			/ (itemSpacing + _icon_size);

		if (gPopResources.size() && columnNum > 0)
		{
			wx::Int32 rowNum = static_cast<wx::Int32>
				((gPopResources.size() + columnNum - 1) / columnNum);

			ImGuiListClipper clipper;
			clipper.Begin(rowNum, _icon_size);

			while (clipper.Step()) {
				auto mapItr = gPopResources.begin();

				for (int i = 0; i < clipper.DisplayStart * columnNum; i++)
				{
					mapItr++;
				}

				for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
					wx::Int32 itemNum = 0;

					if ((i + 1) == rowNum)
						itemNum = ((gPopResources.size() - 1) % columnNum) + 1;
					else
						itemNum = columnNum;

					for (wx::Int32 j = 0; j < itemNum; j++)
					{
						wx::Int32 index = i * columnNum + j;
						const PopResource& resource = mapItr->second;
						mapItr++;

						const ImVec2 cellSize(_icon_size, _icon_size);
						const ImVec2 cellPos = ImGui::GetCursorScreenPos();

						const wx::Float32 maxSide =
							std::max(resource.size.x, resource.size.y);

						ImGui::PushID(index);
						ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);

						ImGui::Button("##resource_select", cellSize);

						ImGui::PopStyleVar();

						if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
						{
							ImGui::SetDragDropPayload(
								kPopResourceDragDropPayload,
								resource.id.c_str(),
								resource.id.size() + 1);
							ImGui::Text("%s %s", ICON_FA_IMAGES, resource.id.c_str());
							ImGui::EndDragDropSource();
						}

						ImGui::PopID();

						if (maxSide > 0.0f)
						{
							const wx::Float32 scale = _icon_size / maxSide;

							ImVec2 imageSize(
								resource.size.x,
								resource.size.y
							);

							if (scale < 1.0f)
							{
								imageSize.x *= scale;
								imageSize.y *= scale;
							}

							const ImVec2 imagePos(
								cellPos.x + (cellSize.x - imageSize.x) * 0.5f,
								cellPos.y + (cellSize.y - imageSize.y) * 0.5f
							);

							ImGui::GetWindowDrawList()->AddImage(
								resource.image,
								imagePos,
								ImVec2(imagePos.x + imageSize.x, imagePos.y + imageSize.y)
							);

							ImVec2 smallSize = ImGui::CalcTextSize(ICON_FA_IMAGES);

							ImGui::GetWindowDrawList()->AddText(
								{ cellPos.x + _icon_size - smallSize.x - 5.0f,cellPos.y + _icon_size - smallSize.y - 5.0f }, 0xFFFFFFFF, ICON_FA_IMAGES
							);
						}

						const ImVec2 itemSize = ImGui::GetItemRectSize();
						ImGui::SameLine();
					}

					ImGui::NewLine();
				}
			}

			clipper.End();
		}
		ImGui::EndChild();
	}
}
