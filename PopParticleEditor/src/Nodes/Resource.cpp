#include "../../include/Nodes/Resource.h"
#include "../../include/Nodes/NodeColors.h"
#include "../../include/Fonts/IconsFontAwesome7.h"
#include "../../include/PopState.h"

#include <algorithm>

namespace ed = ax::NodeEditor;

namespace pop
{
	ResourceNode::ResourceNode()
		:ResourceNode(std::string_view{})
	{
	}

	ResourceNode::ResourceNode(std::string_view resourceId)
		:_resource_id{ 0 }
	{
		_name = "Resource";
		_serialization_type = "Resource";
		_header_color = node_colors::header::Resource;
		_set_resource_id(resourceId);
	}

	ResourceNode::~ResourceNode()
	{

	}

	void ResourceNode::_set_resource_id(std::string_view resourceId)
	{
		const wx::Size length = resourceId.copy(_resource_id, sizeof(_resource_id) - 1);
		_resource_id[length] = '\0';
	}

	std::string_view ResourceNode::_get_resource_id() const noexcept
	{
		return _resource_id;
	}

	wx::Int32 ResourceNode::_get_image_col() const noexcept
	{
		return _image_col;
	}

	wx::Int32 ResourceNode::_get_image_row() const noexcept
	{
		return _image_row;
	}

	wx::Int32 ResourceNode::_get_image_frames() const noexcept
	{
		return _image_frames;
	}

	void ResourceNode::_set_image_selection(
		wx::Int32 imageCol,
		wx::Int32 imageRow,
		wx::Int32 imageFrames) noexcept
	{
		_image_col = std::max(imageCol, 0);
		_image_row = std::max(imageRow, 0);
		_image_frames = std::max(imageFrames, 1);
		_image_cols = std::max(_image_col + _image_frames, 1);
		_image_rows = std::max(_image_row + 1, 1);
		_preview_frame = 0;
	}

	void ResourceNode::_on_initialize(PopNodeEditor& editor)
	{
		PopNodeEditor::Pin output{};
		output.type = "Resource";
		output.name = "Resource";
		output.kind = ed::PinKind::Output;
		output.color = node_colors::pin::Resource;
		output.shape = PopNodeEditor::PinShape::Circle;
		_add_pin(editor, output);
	}

	std::unique_ptr<PopNodeEditor::Node> ResourceNode::_clone() const
	{
		auto node = std::make_unique<ResourceNode>(_get_resource_id());
		node->_image_cols = _image_cols;
		node->_image_rows = _image_rows;
		node->_image_col = _image_col;
		node->_image_row = _image_row;
		node->_image_frames = _image_frames;
		node->_preview_frame = _preview_frame;
		return node;
	}

	nlohmann::json ResourceNode::_serialize() const
	{
		return {
			{ "resource_id", std::string(_resource_id) },
			{ "image_cols", _image_cols },
			{ "image_rows", _image_rows },
			{ "image_col", _image_col },
			{ "image_row", _image_row },
			{ "image_frames", _image_frames },
			{ "preview_frame", _preview_frame }
		};
	}

	void ResourceNode::_deserialize(const nlohmann::json& data)
	{
		_set_resource_id(data.value("resource_id", std::string{}));
		_image_cols = std::max(data.value("image_cols", 1), 1);
		_image_rows = std::max(data.value("image_rows", 1), 1);
		_image_col = std::clamp(data.value("image_col", 0), 0, _image_cols - 1);
		_image_row = std::clamp(data.value("image_row", 0), 0, _image_rows - 1);
		_image_frames = std::clamp(
			data.value("image_frames", 1),
			1,
			_image_cols - _image_col);
		_preview_frame = std::clamp(
			data.value("preview_frame", 0),
			0,
			_image_frames - 1);
		_open_resource_picker = false;
	}

	void ResourceNode::_on_render_workspace()
	{
		ImVec2 text_size = ImGui::CalcTextSize(_resource_id);
		wx::Float32 padding = ImGui::GetStyle().FramePadding.x * 2.0f;
		wx::Float32 width = text_size.x + padding + 10.0f;

		ImGui::SetNextItemWidth(std::max(200.0f, width));
		ImGui::InputText("##resource_id", _resource_id, sizeof(_resource_id));

		ImGui::SameLine();

		const std::string selectLabel = std::string(ML("resource.select")) + "##select_resource";
		if (ImGui::Button(selectLabel.c_str()))
			_open_resource_picker = true;

		ImGui::Dummy({ 0,10 });

		ImGui::SetNextItemWidth(120.0f);
		ImGui::InputInt(ML("resource.total_cols"), &_image_cols);
		_image_cols = std::max(_image_cols, 1);
		_set_item_tooltip(ML("resource.cols_tooltip"));

		ImGui::SetNextItemWidth(120.0f);
		ImGui::InputInt(ML("resource.total_rows"), &_image_rows);
		_image_rows = std::max(_image_rows, 1);
		_set_item_tooltip(ML("resource.rows_tooltip"));

		ImGui::Dummy({ 0,10 });

		ImGui::SetNextItemWidth(120.0f);
		ImGui::InputInt(ML("resource.image_col"), &_image_col);
		_image_col = std::clamp(_image_col, 0, _image_cols - 1);
		_set_item_tooltip(ML("resource.col_tooltip"));


		ImGui::SetNextItemWidth(120.0f);
		ImGui::InputInt(ML("resource.image_row"), &_image_row);
		_image_row = std::clamp(_image_row, 0, _image_rows - 1);
		_set_item_tooltip(ML("resource.row_tooltip"));

		ImGui::SetNextItemWidth(120.0f);
		ImGui::InputInt(ML("resource.image_frames"), &_image_frames);
		_image_frames = std::clamp(_image_frames, 1, _image_cols - _image_col);
		_set_item_tooltip(ML("resource.frames_tooltip"));

		_preview_frame = std::clamp(_preview_frame, 0, _image_frames - 1);
		ImGui::SetNextItemWidth(180.0f);
		ImGui::BeginDisabled(_image_frames == 1);
		ImGui::SliderInt(ML("resource.preview_frame"), &_preview_frame, 0, _image_frames - 1);
		ImGui::EndDisabled();

		auto resItr = gPopResources.find(_resource_id);
		if (resItr != gPopResources.end())
		{
			const PopResource& resource = resItr->second;
			const wx::Float32 cellWidth = resource.size.x / _image_cols;
			const wx::Float32 cellHeight = resource.size.y / _image_rows;
			const ImVec2 previewBounds(320.0f, 240.0f);
			const wx::Float32 scale = std::min({
				1.0f,
				previewBounds.x / std::max(cellWidth, 1.0f),
				previewBounds.y / std::max(cellHeight, 1.0f)
			});
			const wx::Int32 previewCol = _image_col + _preview_frame;
			const ImVec2 uvMin(
				static_cast<wx::Float32>(previewCol) / _image_cols,
				static_cast<wx::Float32>(_image_row) / _image_rows);
			const ImVec2 uvMax(
				static_cast<wx::Float32>(previewCol + 1) / _image_cols,
				static_cast<wx::Float32>(_image_row + 1) / _image_rows);
			ImGui::Image(resource.image, {
				cellWidth * scale,
				cellHeight * scale
			}, uvMin, uvMax);
		}
	}

	void ResourceNode::_on_render_overlay()
	{
		Node::_on_render_overlay();

		ImGui::PushID(_get_id().AsPointer());

		if (_open_resource_picker)
		{
			ImGui::OpenPopup("Select Resource##resource_picker");
			_open_resource_picker = false;
		}

		ImGui::SetNextWindowSize({ 440.0f, 300.0f }, ImGuiCond_Appearing);
		if (ImGui::BeginPopup("Select Resource##resource_picker"))
		{
			ImGui::Text("%s %s", ICON_FA_IMAGES, ML("resource.select_title"));
			ImGui::Separator();

			if (gPopResources.empty())
			{
				ImGui::TextDisabled("%s", ML("resource.none_loaded"));
			}
			else
			{
				const bool listVisible = ImGui::BeginChild(
					"##resource_list", { 420.0f, 220.0f }, ImGuiChildFlags_Borders);
				if (listVisible)
				{
					auto itrBegin = gPopResources.begin();
					for (wx::Size index = 0; index < gPopResources.size(); ++index)
					{
						const PopResource& resource = itrBegin->second;
						itrBegin++;

						const std::string filename = resource.path.filename().string();
						std::string label = ICON_FA_IMAGES "  " + resource.id;
						if (!filename.empty() && filename != resource.id)
							label += "  [" + filename + "]";

						ImGui::PushID(static_cast<wx::Int32>(index));
						const bool selected = resource.id == _resource_id;
						if (ImGui::Selectable(label.c_str(), selected))
						{
							_set_resource_id(resource.id);
							ImGui::CloseCurrentPopup();
						}
						if (ImGui::IsItemHovered() && !resource.path.empty())
							ImGui::SetItemTooltip("%s", resource.path.string().c_str());
						if (selected)
							ImGui::SetItemDefaultFocus();
						ImGui::PopID();
					}
				}
				ImGui::EndChild();
			}

			ImGui::Separator();
			const std::string cancelLabel = std::string(ML("common.cancel")) + "##cancel_resource_picker";
			if (ImGui::Button(cancelLabel.c_str()))
				ImGui::CloseCurrentPopup();

			ImGui::EndPopup();
		}

		ImGui::PopID();
	}
}
