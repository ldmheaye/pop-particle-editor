#include "../../include/Nodes/ParticleSystemDefinition.h"
#include "../../include/Nodes/NodeColors.h"
#include "../../include/Particles/ParticleJson.h"
#include "../../include/PopState.h"

#include <imgui.h>

namespace ed = ax::NodeEditor;

namespace pop
{
	ParticleSystemDefinitionNode::ParticleSystemDefinitionNode()
	{
		_name = "ParticleSystemDefinition";
		_serialization_type = "ParticleSystemDefinition";
		_header_color = node_colors::header::ParticleSystemDefinition;
	}

	void ParticleSystemDefinitionNode::_on_initialize(PopNodeEditor& editor)
	{
		if (_find_node(editor, "ParticleSystemDefinition") != nullptr)
		{
			_duplicate = true;
			_name = "Duplicate SystemDef";
			_header_color = node_colors::header::Error;
			return;
		}

		PopNodeEditor::Pin input{};
		input.name = "Emitters";
		input.type = "Emitter";
		input.kind = ed::PinKind::Input;
		input.color = node_colors::pin::Emitter;
		input.shape = PopNodeEditor::PinShape::Square;
		input.multi_input_pins = true;
		_add_pin(editor, input);
	}

	std::unique_ptr<PopNodeEditor::Node> ParticleSystemDefinitionNode::_clone() const
	{
		auto node = std::make_unique<ParticleSystemDefinitionNode>();
		node->_definition = _definition;
		return node;
	}

	nlohmann::json ParticleSystemDefinitionNode::_serialize() const
	{
		return { { "definition", _definition } };
	}

	void ParticleSystemDefinitionNode::_deserialize(const nlohmann::json& data)
	{
		_definition = data.value(
			"definition",
			particles::ParticleSystemDefinition{});
		_duplicate = false;
	}

	void ParticleSystemDefinitionNode::_on_render_workspace()
	{
		if (_duplicate)
			ImGui::TextColored(
				{ 1.0f, 0.35f, 0.35f, 1.0f },
				"%s",
				ML("Duplicate SystemDef"));
	}

}
