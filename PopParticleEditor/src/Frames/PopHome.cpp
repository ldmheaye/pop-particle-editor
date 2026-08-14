#include "../../include/Frames/PopHome.h"
#include "../../include/PopRender.h"
#include "../../include/PopState.h"
#include "../../include/PopStyle.h"
#include "../../include/Fonts/IconsFontAwesome7.h"
#include <imgui.h>

namespace
{
	void CenterText(const char* text, ImVec4 color)
	{
		const float textWidth = ImGui::CalcTextSize(text).x;
		ImGui::SetCursorPosX((ImGui::GetWindowSize().x - textWidth) * 0.5f);
		ImGui::TextColored(color, "%s", text);
	}

	void DrawParticleMark()
	{
		ImGui::PushFont(nullptr, 38.0f);
		CenterText(ICON_FA_ATOM, ImGui::GetStyleColorVec4(ImGuiCol_CheckMark));
		ImGui::PopFont();
		ImGui::Dummy({ 1.0f, 8.0f });
	}
}

namespace pop
{
	PopHome::PopHome()
	{

	}

	PopHome::~PopHome()
	{

	}

	void PopHome::OnRender()
	{
		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->WorkPos);
		ImGui::SetNextWindowSize(viewport->WorkSize);

		if (ImGui::Begin("WelcomePage", nullptr,
			ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoScrollbar |
			ImGuiWindowFlags_NoScrollWithMouse |
			ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoNav
		))
		{
			RenderGridBackground(
				{
					viewport->WorkPos.x,
					viewport->WorkPos.y,
					viewport->WorkPos.x + viewport->WorkSize.x,
					viewport->WorkPos.y + viewport->WorkSize.y
				},
				{ 20,20 }
			);

			const ImVec2 contentOrigin = ImGui::GetCursorPos();
			const ImVec2 contentSize = ImGui::GetContentRegionAvail();
			const ImVec2 welcomeWindowSize(
				ImMin(contentSize.x, ImClamp(contentSize.x * 0.46f, 420.0f, 540.0f)),
				ImMin(contentSize.y, 340.0f));
			const float offsetX = (contentSize.x - welcomeWindowSize.x) * 0.5f;
			const float offsetY = (contentSize.y - welcomeWindowSize.y) * 0.5f;
			ImGui::SetCursorPos({ contentOrigin.x + offsetX, contentOrigin.y + offsetY });

			const ImVec2 cardPosition = ImGui::GetCursorScreenPos();
			ImGui::GetWindowDrawList()->AddRectFilled(
				{ cardPosition.x + 8.0f, cardPosition.y + 10.0f },
				{ cardPosition.x + welcomeWindowSize.x + 8.0f,
					cardPosition.y + welcomeWindowSize.y + 10.0f },
				IM_COL32(0, 0, 0, 70), 8.0f);

			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 34.0f, 28.0f });

			ImGui::BeginChild("Welcome",
				welcomeWindowSize, ImGuiChildFlags_Borders, 0);

			const ImVec2 childPosition = ImGui::GetWindowPos();
			const ImVec2 childSize = ImGui::GetWindowSize();
			ImGui::GetWindowDrawList()->AddRectFilled(
				childPosition,
				{ childPosition.x + childSize.x, childPosition.y + 3.0f },
				IM_COL32(116, 164, 255, 255), 8.0f,
				ImDrawFlags_RoundCornersTop);

			DrawParticleMark();

			ImGui::PushFont(nullptr, 19.0f);
			CenterText(ML("app.title"), ImGui::GetStyleColorVec4(ImGuiCol_Text));
			ImGui::PopFont();
			ImGui::Dummy({ 1.0f, 4.0f });
			CenterText(ML("home.tagline"),
				ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));

			const float buttonRowY = ImMax(
				ImGui::GetCursorPosY() + 24.0f,
				ImGui::GetWindowHeight() - 82.0f);
			ImGui::SetCursorPosY(buttonRowY);
			const float buttonGap = 12.0f;
			const float buttonWidth = (ImGui::GetContentRegionAvail().x - buttonGap) * 0.5f;
			const ImVec2 buttonSize(buttonWidth, 44.0f);

			PushPrimaryButtonStyle();
			if (ImGui::Button(ML("home.create_new"), buttonSize))
			{
				_on_create_new.Invoke();
			}
			PopPrimaryButtonStyle();

			ImGui::SameLine(0.0f, buttonGap);
			if (ImGui::Button(ML("menu.open_project"), buttonSize))
			{
				_on_open_project.Invoke();
			}

			ImGui::EndChild();

			ImGui::PopStyleVar();
		}

		ImGui::End();
	}

	Event<>& PopHome::GetOnCreateNewEvent()
	{
		return _on_create_new;
	}

	Event<>& PopHome::GetOnOpenProjectEvent()
	{
		return _on_open_project;
	}
}
