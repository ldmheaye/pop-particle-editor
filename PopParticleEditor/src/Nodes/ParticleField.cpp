#include "../../include/Nodes/ParticleField.h"
#include "../../include/Nodes/NodeColors.h"
#include "../../include/Particles/ParticleJson.h"
#include "../../include/PopState.h"

#include <algorithm>
#include <imgui.h>

namespace ed = ax::NodeEditor;

namespace
{
	using pop::particles::ParticleFieldType;

	struct ParticleFieldTypeOption
	{
		ParticleFieldType value;
		const char* label;
	};

	constexpr ParticleFieldTypeOption kParticleFieldTypes[] = {
		{ ParticleFieldType::Invalid, "Invalid" },
		{ ParticleFieldType::Friction, "Friction" },
		{ ParticleFieldType::Acceleration, "Acceleration" },
		{ ParticleFieldType::Attractor, "Attractor" },
		{ ParticleFieldType::MaxVelocity, "Max Velocity" },
		{ ParticleFieldType::Velocity, "Velocity" },
		{ ParticleFieldType::Position, "Position" },
		{ ParticleFieldType::SystemPosition, "System Position" },
		{ ParticleFieldType::GroundConstraint, "Ground Constraint" },
		{ ParticleFieldType::Shake, "Shake" },
		{ ParticleFieldType::Circle, "Circle" },
		{ ParticleFieldType::Away, "Away" }
	};

	const char* GetParticleFieldTypeLabel(ParticleFieldType type)
	{
		for (const auto& option : kParticleFieldTypes)
		{
			if (option.value == type)
				return pop::ML(option.label);
		}

		return pop::ML("Invalid");
	}

	wx::Float32 CalculateTypeControlWidth()
	{
		wx::Float32 maxLabelWidth = 0.0f;
		for (const auto& option : kParticleFieldTypes)
			maxLabelWidth = std::max(maxLabelWidth, ImGui::CalcTextSize(pop::ML(option.label)).x);

		const ImGuiStyle& style = ImGui::GetStyle();
		return std::max(
			220.0f,
			maxLabelWidth + style.FramePadding.x * 2.0f +
			ImGui::GetFrameHeight() * 2.0f + style.ItemSpacing.x * 2.0f);
	}
}

namespace pop
{
	ParticleFieldNode::ParticleFieldNode()
	{
		_name = "ParticleField";
		_serialization_type = "ParticleField";
		_header_color = node_colors::header::ParticleField;
	}

	void ParticleFieldNode::_on_initialize(PopNodeEditor& editor)
	{
		PopNodeEditor::Pin input{};
		input.name = "X";
		input.type = "NodeTrack";
		input.kind = ed::PinKind::Input;
		input.color = node_colors::pin::NodeTrack;
		input.shape = PopNodeEditor::PinShape::Circle;
		_add_pin(editor, input);

		input.name = "Y";
		input.type = "NodeTrack";
		input.kind = ed::PinKind::Input;
		input.color = node_colors::pin::NodeTrack;
		input.shape = PopNodeEditor::PinShape::Circle;
		_add_pin(editor, input);

		PopNodeEditor::Pin output{};
		output.name = "Particle Field";
		output.type = "ParticleField";
		output.kind = ed::PinKind::Output;
		output.color = node_colors::pin::ParticleField;
		output.shape = PopNodeEditor::PinShape::Circle;
		_add_pin(editor, output);
	}

	std::unique_ptr<PopNodeEditor::Node> ParticleFieldNode::_clone() const
	{
		auto node = std::make_unique<ParticleFieldNode>();
		node->_definition = _definition;
		return node;
	}

	nlohmann::json ParticleFieldNode::_serialize() const
	{
		return { { "definition", _definition } };
	}

	void ParticleFieldNode::_deserialize(const nlohmann::json& data)
	{
		_definition = data.value(
			"definition",
			particles::ParticleFieldDefinition{});
	}

	void ParticleFieldNode::_on_render_workspace()
	{
		ImGui::Dummy({ 20,0 });
		ImGui::SameLine();

		ImGui::BeginGroup();
		ImGui::TextUnformatted(ML("field.type"));

		wx::Int32 typeIndex = 0;
		constexpr wx::Int32 typeCount =
			static_cast<wx::Int32>(sizeof(kParticleFieldTypes) / sizeof(kParticleFieldTypes[0]));
		for (wx::Int32 index = 0; index < typeCount; ++index)
		{
			if (kParticleFieldTypes[index].value == _definition.type)
			{
				typeIndex = index;
				break;
			}
		}

		if (ImGui::ArrowButton("##previous_field_type", ImGuiDir_Left) && typeIndex > 0)
		{
			--typeIndex;
			_definition.type = kParticleFieldTypes[typeIndex].value;
		}
		_set_item_tooltip(ML("field.previous_type"));

		ImGui::SameLine();
		const wx::Float32 arrowWidth = ImGui::GetFrameHeight();
		const wx::Float32 controlWidth = CalculateTypeControlWidth();
		const wx::Float32 typeButtonWidth =
			controlWidth - arrowWidth * 2.0f - ImGui::GetStyle().ItemSpacing.x * 2.0f;
		if (ImGui::Button(GetParticleFieldTypeLabel(_definition.type), { typeButtonWidth, 0.0f }))
		{
			typeIndex = (typeIndex + 1) % typeCount;
			_definition.type = kParticleFieldTypes[typeIndex].value;
		}
		_set_item_tooltip(ML("field.cycle_type"));

		ImGui::SameLine();
		if (ImGui::ArrowButton("##next_field_type", ImGuiDir_Right) && typeIndex + 1 < typeCount)
		{
			++typeIndex;
			_definition.type = kParticleFieldTypes[typeIndex].value;
		}
		_set_item_tooltip(ML("field.next_type"));
		ImGui::EndGroup();
	}

	const particles::ParticleFieldDefinition& ParticleFieldNode::_get_definition() const noexcept
	{
		return _definition;
	}

	particles::ParticleFieldDefinition& ParticleFieldNode::_get_definition() noexcept
	{
		return _definition;
	}
}
