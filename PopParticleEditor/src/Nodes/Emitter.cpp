#include "../../include/Nodes/Emitter.h"
#include "../../include/Nodes/NodeColors.h"
#include "../../include/Particles/ParticleJson.h"
#include "../../include/PopState.h"

#include <algorithm>
#include <cfloat>
#include <imgui.h>

namespace ed = ax::NodeEditor;

namespace
{
	using pop::particles::EmitterType;
	using pop::particles::ParticleFlags;

	template <wx::Size Size>
	void CopyText(char (&destination)[Size], std::string_view source)
	{
		const wx::Size length = std::min(source.size(), Size - 1);
		source.copy(destination, length);
		destination[length] = '\0';
	}

	struct EmitterTypeOption
	{
		EmitterType value;
		const char* label;
	};

	struct ParticleFlagOption
	{
		ParticleFlags value;
		const char* label;
	};

	constexpr EmitterTypeOption kEmitterTypes[] = {
		{ EmitterType::Circle, "Circle" },
		{ EmitterType::Box, "Box" },
		{ EmitterType::BoxPath, "Box Path" },
		{ EmitterType::CirclePath, "Circle Path" },
		{ EmitterType::CircleEvenSpacing, "Circle Even Spacing" }
	};

	constexpr ParticleFlagOption kParticleFlags[] = {
		{ pop::particles::RandomLaunchSpin, "Random Launch Spin" },
		{ pop::particles::AlignLaunchSpin, "Align Launch Spin" },
		{ pop::particles::AlignToPixel, "Align To Pixel" },
		{ pop::particles::SystemLoops, "System Loops" },
		{ pop::particles::ParticleLoops, "Particle Loops" },
		{ pop::particles::ParticlesDontFollow, "Particles Don't Follow" },
		{ pop::particles::RandomStartTime, "Random Start Time" },
		{ pop::particles::DieIfOverloaded, "Die If Overloaded" },
		{ pop::particles::Additive, "Additive" },
		{ pop::particles::FullScreen, "Full Screen" },
		{ pop::particles::SoftwareOnly, "Software Only" },
		{ pop::particles::HardwareOnly, "Hardware Only" }
	};

	constexpr const char* kEmitterTrackPins[] = {
		"System Duration",
		"Cross Fade Duration",
		"Spawn Rate",
		"Spawn Min Active",
		"Spawn Max Active",
		"Spawn Max Launched",
		"Emitter Radius",
		"Emitter Offset X",
		"Emitter Offset Y",
		"Emitter Box X",
		"Emitter Box Y",
		"Emitter Skew X",
		"Emitter Skew Y",
		"Emitter Path",
		"Particle Duration",
		"Launch Speed",
		"Launch Angle",
		"System Red",
		"System Green",
		"System Blue",
		"System Alpha",
		"System Brightness"
	};

	constexpr const char* kParticleTrackPins[] = {
		"Particle Red",
		"Particle Green",
		"Particle Blue",
		"Particle Alpha",
		"Particle Brightness",
		"Particle Spin Angle",
		"Particle Spin Speed",
		"Particle Scale",
		"Particle Stretch",
		"Collision Reflect",
		"Collision Spin",
		"Clip Top",
		"Clip Bottom",
		"Clip Left",
		"Clip Right",
		"Animation Rate"
	};

	pop::PopNodeEditor::Pin MakeInputPin(
		const char* name,
		const char* type,
		ImU32 color,
		pop::PopNodeEditor::PinShape shape = pop::PopNodeEditor::PinShape::Circle,
		bool multiInput = false)
	{
		pop::PopNodeEditor::Pin pin{};
		pin.name = name;
		pin.type = type;
		pin.kind = ed::PinKind::Input;
		pin.color = color;
		pin.shape = shape;
		pin.multi_input_pins = multiInput;
		return pin;
	}

	void RenderFlag(ParticleFlags& flags, const ParticleFlagOption& option)
	{
		bool enabled = pop::particles::HasFlag(flags, option.value);
		if (!ImGui::Checkbox(pop::ML(option.label), &enabled))
			return;

		wx::Uint32 value = static_cast<wx::Uint32>(flags);
		const wx::Uint32 bit = static_cast<wx::Uint32>(option.value);
		value = enabled ? value | bit : value & ~bit;
		flags = static_cast<ParticleFlags>(value);
	}

	wx::Float32 CalculateControlsWidth()
	{
		wx::Float32 maxLabelWidth = 0.0f;
		for (const auto& option : kEmitterTypes)
			maxLabelWidth = std::max(maxLabelWidth, ImGui::CalcTextSize(pop::ML(option.label)).x);
		for (const auto& option : kParticleFlags)
			maxLabelWidth = std::max(maxLabelWidth, ImGui::CalcTextSize(pop::ML(option.label)).x);

		const ImGuiStyle& style = ImGui::GetStyle();
		const wx::Float32 selectorWidth = ImGui::GetFrameHeight();
		return std::max(
			220.0f,
			maxLabelWidth + selectorWidth + style.ItemInnerSpacing.x + style.CellPadding.x * 2.0f + 8.0f);
	}
}

namespace pop
{
	EmitterNode::EmitterNode()
	{
		_name = "Emitter";
		_serialization_type = "Emitter";
		_header_color = node_colors::header::Emitter;
	}

	void EmitterNode::_on_initialize(PopNodeEditor& editor)
	{
		_add_pin(editor, MakeInputPin("Resource", "Resource", node_colors::pin::Resource));

		for (const char* name : kEmitterTrackPins)
			_add_pin(editor, MakeInputPin(name, "NodeTrack", node_colors::pin::NodeTrack));

		_add_pin(editor, MakeInputPin(
			"Particle Fields",
			"ParticleField",
			node_colors::pin::ParticleField,
			PopNodeEditor::PinShape::Square,
			true));
		_add_pin(editor, MakeInputPin(
			"System Fields",
			"ParticleField",
			node_colors::pin::ParticleField,
			PopNodeEditor::PinShape::Square,
			true));

		for (const char* name : kParticleTrackPins)
			_add_pin(editor, MakeInputPin(name, "NodeTrack", node_colors::pin::NodeTrack));

		PopNodeEditor::Pin output{};
		output.name = "Emitter";
		output.type = "Emitter";
		output.kind = ed::PinKind::Output;
		output.color = node_colors::pin::Emitter;
		output.shape = PopNodeEditor::PinShape::Circle;
		_add_pin(editor, output);
	}

	std::unique_ptr<PopNodeEditor::Node> EmitterNode::_clone() const
	{
		auto node = std::make_unique<EmitterNode>();
		node->_definition = _definition;
		for (wx::Size index = 0; index < sizeof(_definition_name); ++index)
			node->_definition_name[index] = _definition_name[index];
		for (wx::Size index = 0; index < sizeof(_on_duration); ++index)
			node->_on_duration[index] = _on_duration[index];
		return node;
	}

	nlohmann::json EmitterNode::_serialize() const
	{
		return { { "definition", _definition } };
	}

	void EmitterNode::_deserialize(const nlohmann::json& data)
	{
		_set_definition(data.value(
			"definition",
			particles::ParticleEmitterDefinition{}));
	}

	void EmitterNode::_on_render_workspace()
	{
		const wx::Float32 controlsWidth = CalculateControlsWidth();
		if (!ImGui::BeginTable(
			"##emitter_controls",
			1,
			ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings,
			{ controlsWidth, 0.0f }))
			return;

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::TextUnformatted(ML("emitter.name"));
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-FLT_MIN);
		ImGui::InputText("##emitter_name", _definition_name, sizeof(_definition_name));
		_definition.name = _definition_name;

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::TextUnformatted(ML("emitter.on_duration"));
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-FLT_MIN);
		ImGui::InputText("##emitter_on_duration", _on_duration, sizeof(_on_duration));
		_definition.on_duration = _on_duration;
		_set_item_tooltip(ML("emitter.on_duration_tooltip"));

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::Checkbox(ML("emitter.animated"), &_definition.animated);

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::SeparatorText(ML("emitter.type"));
		for (const auto& option : kEmitterTypes)
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			if (ImGui::RadioButton(ML(option.label), _definition.emitter_type == option.value))
				_definition.emitter_type = option.value;
		}

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::SeparatorText(ML("emitter.flags"));
		for (const auto& option : kParticleFlags)
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			RenderFlag(_definition.flags, option);
		}

		ImGui::EndTable();
	}

	const particles::ParticleEmitterDefinition& EmitterNode::_get_definition() const noexcept
	{
		return _definition;
	}

	void EmitterNode::_set_definition(
		const particles::ParticleEmitterDefinition& definition)
	{
		_definition = definition;
		CopyText(_definition_name, _definition.name);
		CopyText(_on_duration, _definition.on_duration);
	}
}
