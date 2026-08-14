#include "../../include/Nodes/BasicTypes.h"
#include "../../include/Nodes/NodeColors.h"

namespace ed = ax::NodeEditor;

namespace pop
{
	FloatNode::FloatNode()
		:Node(),
		_value(0)
	{
		_name = "Float";
		_serialization_type = "Float";
		_header_color = node_colors::header::Float;
	}

	FloatNode::~FloatNode()
	{

	}

	void FloatNode::_on_initialize(PopNodeEditor& editor)
	{
		PopNodeEditor::Pin output{};
		output.type = "Float";
		output.name = "Float";
		output.kind = ed::PinKind::Output;
		output.color = node_colors::pin::Float;
		output.shape = PopNodeEditor::PinShape::Circle;
		_add_pin(editor, output);
	}

	std::unique_ptr<PopNodeEditor::Node> FloatNode::_clone() const
	{
		auto node = std::make_unique<FloatNode>();
		node->_value = _value;
		return node;
	}

	nlohmann::json FloatNode::_serialize() const
	{
		return { { "value", _value } };
	}

	void FloatNode::_deserialize(const nlohmann::json& data)
	{
		_value = data.value("value", 0.0f);
	}

	void FloatNode::_on_render_workspace()
	{
		ImGui::SetNextItemWidth(150.0f);
		ImGui::InputFloat("##float", &_value, 0.1f, 1.0f, "%.3f");
		ImGui::SameLine();
		ImGui::Dummy({ 10,0 });
	}

	IntNode::IntNode()
		:Node(),
		_value(0)
	{
		_name = "Integer";
		_serialization_type = "Int";
		_header_color = node_colors::header::Integer;
	}

	IntNode::~IntNode()
	{

	}

	void IntNode::_on_initialize(PopNodeEditor& editor)
	{
		PopNodeEditor::Pin output{};
		output.type = "Int";
		output.name = "Int";
		output.kind = ed::PinKind::Output;
		output.color = node_colors::pin::Integer;
		output.shape = PopNodeEditor::PinShape::Circle;

		_add_pin(editor, output);
	}

	std::unique_ptr<PopNodeEditor::Node> IntNode::_clone() const
	{
		auto node = std::make_unique<IntNode>();
		node->_value = _value;
		return node;
	}

	nlohmann::json IntNode::_serialize() const
	{
		return { { "value", _value } };
	}

	void IntNode::_deserialize(const nlohmann::json& data)
	{
		_value = data.value("value", 0);
	}

	void IntNode::_on_render_workspace()
	{
		ImGui::SetNextItemWidth(150.0f);
		ImGui::InputInt("##int", &_value, 0.1f, 1.0f);
		ImGui::SameLine();
		ImGui::Dummy({ 10,0 });
	}

	StringNode::StringNode()
		:Node(), _value{ 0 }
	{
		_name = "String";
		_serialization_type = "String";
		_header_color = node_colors::header::String;
	}

	StringNode::~StringNode()
	{

	}

	void StringNode::_on_initialize(PopNodeEditor& editor)
	{
		PopNodeEditor::Pin output{};
		output.type = "String";
		output.name = "String";
		output.kind = ed::PinKind::Output;
		output.color = node_colors::pin::String;
		output.shape = PopNodeEditor::PinShape::Circle;

		_add_pin(editor, output);
	}

	std::unique_ptr<PopNodeEditor::Node> StringNode::_clone() const
	{
		auto node = std::make_unique<StringNode>();
		for (wx::Size index = 0; index < sizeof(_value); ++index)
			node->_value[index] = _value[index];
		return node;
	}

	nlohmann::json StringNode::_serialize() const
	{
		return { { "value", std::string(_value) } };
	}

	void StringNode::_deserialize(const nlohmann::json& data)
	{
		const std::string value = data.value("value", std::string{});
		const wx::Size length = std::min(value.size(), sizeof(_value) - 1);
		value.copy(_value, length);
		_value[length] = '\0';
	}

	void StringNode::_on_render_workspace()
	{
		ImVec2 text_size = ImGui::CalcTextSize(_value);

		wx::Float32 padding = ImGui::GetStyle().FramePadding.x * 2.0f;
		wx::Int32 width = text_size.x + padding + 10.0f;

		ImGui::SetNextItemWidth(std::max(width, 100));
		ImGui::InputText("##string", _value, sizeof(_value));
	}
}
