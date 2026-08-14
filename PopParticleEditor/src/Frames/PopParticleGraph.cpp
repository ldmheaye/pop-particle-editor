#include "../../include/Frames/PopNodeEditor.h"

#include "../../include/Nodes/Emitter.h"
#include "../../include/Nodes/NodeTrack.h"
#include "../../include/Nodes/ParticleField.h"
#include "../../include/Nodes/ParticleSystemDefinition.h"
#include "../../include/Nodes/Resource.h"
#include "../../include/Particles/ParticleSystem.h"
#include "../../include/PopState.h"

#include <algorithm>
#include <utility>

namespace ed = ax::NodeEditor;

namespace
{
	using pop::particles::FloatTrack;
	using pop::particles::ParticleEmitterDefinition;

	struct EmitterTrackInput
	{
		const char* pin_name;
		FloatTrack ParticleEmitterDefinition::* value;
	};

	constexpr EmitterTrackInput kEmitterTrackInputs[] = {
		{ "System Duration", &ParticleEmitterDefinition::system_duration },
		{ "Cross Fade Duration", &ParticleEmitterDefinition::cross_fade_duration },
		{ "Spawn Rate", &ParticleEmitterDefinition::spawn_rate },
		{ "Spawn Min Active", &ParticleEmitterDefinition::spawn_min_active },
		{ "Spawn Max Active", &ParticleEmitterDefinition::spawn_max_active },
		{ "Spawn Max Launched", &ParticleEmitterDefinition::spawn_max_launched },
		{ "Emitter Radius", &ParticleEmitterDefinition::emitter_radius },
		{ "Emitter Offset X", &ParticleEmitterDefinition::emitter_offset_x },
		{ "Emitter Offset Y", &ParticleEmitterDefinition::emitter_offset_y },
		{ "Emitter Box X", &ParticleEmitterDefinition::emitter_box_x },
		{ "Emitter Box Y", &ParticleEmitterDefinition::emitter_box_y },
		{ "Emitter Skew X", &ParticleEmitterDefinition::emitter_skew_x },
		{ "Emitter Skew Y", &ParticleEmitterDefinition::emitter_skew_y },
		{ "Emitter Path", &ParticleEmitterDefinition::emitter_path },
		{ "Particle Duration", &ParticleEmitterDefinition::particle_duration },
		{ "Launch Speed", &ParticleEmitterDefinition::launch_speed },
		{ "Launch Angle", &ParticleEmitterDefinition::launch_angle },
		{ "System Red", &ParticleEmitterDefinition::system_red },
		{ "System Green", &ParticleEmitterDefinition::system_green },
		{ "System Blue", &ParticleEmitterDefinition::system_blue },
		{ "System Alpha", &ParticleEmitterDefinition::system_alpha },
		{ "System Brightness", &ParticleEmitterDefinition::system_brightness },
		{ "Particle Red", &ParticleEmitterDefinition::particle_red },
		{ "Particle Green", &ParticleEmitterDefinition::particle_green },
		{ "Particle Blue", &ParticleEmitterDefinition::particle_blue },
		{ "Particle Alpha", &ParticleEmitterDefinition::particle_alpha },
		{ "Particle Brightness", &ParticleEmitterDefinition::particle_brightness },
		{ "Particle Spin Angle", &ParticleEmitterDefinition::particle_spin_angle },
		{ "Particle Spin Speed", &ParticleEmitterDefinition::particle_spin_speed },
		{ "Particle Scale", &ParticleEmitterDefinition::particle_scale },
		{ "Particle Stretch", &ParticleEmitterDefinition::particle_stretch },
		{ "Collision Reflect", &ParticleEmitterDefinition::collision_reflect },
		{ "Collision Spin", &ParticleEmitterDefinition::collision_spin },
		{ "Clip Top", &ParticleEmitterDefinition::clip_top },
		{ "Clip Bottom", &ParticleEmitterDefinition::clip_bottom },
		{ "Clip Left", &ParticleEmitterDefinition::clip_left },
		{ "Clip Right", &ParticleEmitterDefinition::clip_right },
		{ "Animation Rate", &ParticleEmitterDefinition::animation_rate }
	};

	bool IsExplicitTrack(const FloatTrack& track)
	{
		return !track.nodes.empty() &&
			!(track.nodes.size() == 1 &&
				track.nodes.front().curve == pop::particles::CurveType::Constant);
	}

	FloatTrack MakeConstantTrack(wx::Float32 value)
	{
		return { { {
			0.0f,
			value,
			value,
			pop::particles::CurveType::Linear,
			pop::particles::CurveType::Linear
		} } };
	}

	void ResetEmitterInputs(ParticleEmitterDefinition& definition)
	{
		definition.image.clear();
		definition.image_col = 0;
		definition.image_row = 0;
		definition.image_frames = 1;
		definition.image_cols = 1;
		definition.image_rows = 1;
		definition.particle_fields.clear();
		definition.system_fields.clear();
		const ParticleEmitterDefinition defaults;
		for (const EmitterTrackInput& input : kEmitterTrackInputs)
			definition.*input.value = defaults.*input.value;
	}
}

namespace pop
{
	bool PopNodeEditor::_read_track_node(const Node& node, FloatTrack& track)
	{
		if (const auto* trackNode = dynamic_cast<const NodeTrackNode*>(&node))
		{
			track = trackNode->_get_track();
			return true;
		}
		if (const auto* constantNode =
			dynamic_cast<const ConstantNodeTrackNode*>(&node))
		{
			track = MakeConstantTrack(constantNode->_get_value());
			return true;
		}
		return false;
	}

	PopNodeEditor::Pin* PopNodeEditor::_find_input_pin(
		Node& node,
		std::string_view name)
	{
		for (Pin& pin : node._input_pins)
			if (std::string_view(pin.name.data(), pin.name.size()) == name)
				return &pin;
		return nullptr;
	}

	const PopNodeEditor::Pin* PopNodeEditor::_find_input_pin(
		const Node& node,
		std::string_view name) const
	{
		for (const Pin& pin : node._input_pins)
			if (std::string_view(pin.name.data(), pin.name.size()) == name)
				return &pin;
		return nullptr;
	}

	std::vector<const PopNodeEditor::Node*> PopNodeEditor::_find_connected_nodes(
		const Pin& input) const
	{
		std::vector<const Node*> nodes;
		nodes.reserve(input.connected_links.size());
		std::vector<const Link*> links(
			input.connected_links.begin(),
			input.connected_links.end());
		std::sort(
			links.begin(),
			links.end(),
			[](const Link* lhs, const Link* rhs)
			{
				if (lhs == nullptr || rhs == nullptr)
					return lhs != nullptr;
				return reinterpret_cast<wx::Size>(lhs->id.AsPointer()) <
					reinterpret_cast<wx::Size>(rhs->id.AsPointer());
			});

		for (const Link* link : links)
		{
			if (link == nullptr)
				continue;

			const ed::PinId connectedPinId = link->start_pin_id == input.id
				? link->end_pin_id
				: link->start_pin_id;
			const auto pinItr = _pins_map.find(
				reinterpret_cast<wx::Size>(connectedPinId.AsPointer()));
			if (pinItr == _pins_map.end() || pinItr->second == nullptr ||
				pinItr->second->kind != ed::PinKind::Output)
			{
				continue;
			}

			const auto nodeItr = _nodes_map.find(
				reinterpret_cast<wx::Size>(pinItr->second->node_id.AsPointer()));
			if (nodeItr != _nodes_map.end() && nodeItr->second != nullptr)
				nodes.push_back(nodeItr->second);
		}
		return nodes;
	}

	void PopNodeEditor::_connect_node(Node& source, Pin& input)
	{
		if (!source._output_pins.empty())
			_add_link(source._output_pins.front(), input);
	}

	bool PopNodeEditor::ExportParticleSystem(
		particles::ParticleSystemDefinition& definition,
		std::string& error) const
	{
		const ParticleSystemDefinitionNode* systemNode = nullptr;
		wx::Size systemNodeCount = 0;
		for (const auto& node : _nodes)
		{
			if (const auto* currentSystemNode =
				dynamic_cast<const ParticleSystemDefinitionNode*>(node.get()))
			{
				++systemNodeCount;
				if (systemNode == nullptr)
					systemNode = currentSystemNode;
			}
		}

		if (systemNode == nullptr)
		{
			error = ML("graph.no_system_node");
			return false;
		}
		if (systemNodeCount != 1)
		{
			error = ML("graph.duplicate_system_node");
			return false;
		}

		const Pin* emitterInput = _find_input_pin(*systemNode, "Emitters");
		if (emitterInput == nullptr)
		{
			error = ML("graph.no_emitter_input");
			return false;
		}

		particles::ParticleSystemDefinition exportedDefinition;
		for (const Node* sourceNode : _find_connected_nodes(*emitterInput))
		{
			const auto* emitterNode = dynamic_cast<const EmitterNode*>(sourceNode);
			if (emitterNode == nullptr)
			{
				error = ML("graph.non_emitter");
				return false;
			}

			particles::ParticleEmitterDefinition emitter =
				emitterNode->_get_definition();
			const Pin* resourceInput = _find_input_pin(*emitterNode, "Resource");
			if (resourceInput != nullptr)
			{
				const std::vector<const Node*> resourceNodes =
					_find_connected_nodes(*resourceInput);
				if (resourceNodes.size() > 1)
				{
					error = ML("graph.resource_multiple");
					return false;
				}
				if (!resourceNodes.empty())
				{
					const auto* resourceNode =
						dynamic_cast<const ResourceNode*>(resourceNodes.front());
					if (resourceNode == nullptr)
					{
						error = ML("graph.resource_invalid");
						return false;
					}
					const std::string_view resourceId = resourceNode->_get_resource_id();
					emitter.image.assign(resourceId.data(), resourceId.size());
					emitter.image_col = resourceNode->_get_image_col();
					emitter.image_row = resourceNode->_get_image_row();
					emitter.image_frames = resourceNode->_get_image_frames();
					emitter.image_cols = resourceNode->_image_cols;
					emitter.image_rows = resourceNode->_image_rows;
				}
			}

			for (const EmitterTrackInput& input : kEmitterTrackInputs)
			{
				const Pin* trackInput = _find_input_pin(*emitterNode, input.pin_name);
				if (trackInput == nullptr)
					continue;
				const std::vector<const Node*> trackNodes =
					_find_connected_nodes(*trackInput);
				if (trackNodes.size() > 1)
				{
					error = MLF(
						"graph.input_multiple",
						ML(input.pin_name));
					return false;
				}
				if (!trackNodes.empty() &&
					!_read_track_node(*trackNodes.front(), emitter.*input.value))
				{
					error = MLF(
						"graph.input_invalid",
						ML(input.pin_name));
					return false;
				}
			}

			emitter.particle_fields.clear();
			emitter.system_fields.clear();
			const char* fieldPinNames[] = { "Particle Fields", "System Fields" };
			for (wx::Size fieldGroup = 0; fieldGroup < 2; ++fieldGroup)
			{
				std::vector<particles::ParticleFieldDefinition>& fields = fieldGroup == 0
					? emitter.particle_fields
					: emitter.system_fields;
				const Pin* fieldsInput = _find_input_pin(
					*emitterNode,
					fieldPinNames[fieldGroup]);
				if (fieldsInput == nullptr)
					continue;
				const std::vector<const Node*> fieldNodes =
					_find_connected_nodes(*fieldsInput);
				if (fieldNodes.size() > particles::kTodMaxParticleFields)
				{
					error = MLF(
						"graph.fields_too_many",
						ML(fieldPinNames[fieldGroup]));
					return false;
				}

				for (const Node* fieldSource : fieldNodes)
				{
					const auto* fieldNode =
						dynamic_cast<const ParticleFieldNode*>(fieldSource);
					if (fieldNode == nullptr)
					{
						error = MLF(
							"graph.field_source_invalid",
							ML(fieldPinNames[fieldGroup]));
						return false;
					}

					particles::ParticleFieldDefinition field =
						fieldNode->_get_definition();
					const char* axisNames[] = { "X", "Y" };
					particles::FloatTrack* axes[] = { &field.x, &field.y };
					for (wx::Size axis = 0; axis < 2; ++axis)
					{
						const Pin* axisInput = _find_input_pin(
							*fieldNode,
							axisNames[axis]);
						if (axisInput == nullptr)
							continue;
						const std::vector<const Node*> axisNodes =
							_find_connected_nodes(*axisInput);
						if (axisNodes.size() > 1 ||
							(!axisNodes.empty() &&
								!_read_track_node(*axisNodes.front(), *axes[axis])))
						{
							error = MLF(
								"graph.axis_invalid",
								axisNames[axis]);
							return false;
						}
					}
					fields.push_back(std::move(field));
				}
			}

			exportedDefinition.emitters.push_back(std::move(emitter));
		}

		if (emitterInput->connected_links.size() !=
			exportedDefinition.emitters.size())
		{
			error = ML("graph.broken_emitter_link");
			return false;
		}

		definition = std::move(exportedDefinition);
		error.clear();
		return true;
	}

	bool PopNodeEditor::ImportParticleSystem(
		const particles::ParticleSystemDefinition& definition,
		std::string& error)
	{
		for (const particles::ParticleEmitterDefinition& emitter : definition.emitters)
		{
			if (emitter.particle_fields.size() > particles::kTodMaxParticleFields ||
				emitter.system_fields.size() > particles::kTodMaxParticleFields)
			{
				error = ML("graph.xml_fields_too_many");
				return false;
			}
		}

		Clear();
		std::vector<EmitterNode*> emitterNodes;
		emitterNodes.reserve(definition.emitters.size());

		auto connectNodes = [this](Node& source, Pin& input)
		{
			_connect_node(source, input);
		};

		auto addTrackNode = [this, &connectNodes](
			const particles::FloatTrack& track,
			Pin& input)
		{
			if (!IsExplicitTrack(track))
				return;

			if (track.nodes.size() == 1 &&
				track.nodes.front().low_value == track.nodes.front().high_value)
			{
				auto node = std::make_unique<ConstantNodeTrackNode>();
				ConstantNodeTrackNode* nodePtr = node.get();
				nodePtr->_set_value(track.nodes.front().low_value);
				AddNode(std::move(node), false);
				connectNodes(*nodePtr, input);
				return;
			}

			auto node = std::make_unique<NodeTrackNode>();
			NodeTrackNode* nodePtr = node.get();
			nodePtr->_get_track() = track;
			nodePtr->_fit_view_to_track();
			AddNode(std::move(node), false);
			connectNodes(*nodePtr, input);
		};

		for (const particles::ParticleEmitterDefinition& sourceEmitter :
			definition.emitters)
		{
			auto emitter = std::make_unique<EmitterNode>();
			EmitterNode* emitterPtr = emitter.get();
			particles::ParticleEmitterDefinition nodeDefinition = sourceEmitter;
			ResetEmitterInputs(nodeDefinition);
			emitterPtr->_set_definition(nodeDefinition);
			AddNode(std::move(emitter), false);
			emitterNodes.push_back(emitterPtr);

			if (!sourceEmitter.image.empty() || sourceEmitter.image_col != 0 ||
				sourceEmitter.image_row != 0 || sourceEmitter.image_frames != 1)
			{
				auto resource = std::make_unique<ResourceNode>(sourceEmitter.image);
				ResourceNode* resourcePtr = resource.get();
				resourcePtr->_set_image_selection(
					sourceEmitter.image_col,
					sourceEmitter.image_row,
					sourceEmitter.image_frames);
				AddNode(std::move(resource), false);
				if (Pin* input = _find_input_pin(*emitterPtr, "Resource"))
					connectNodes(*resourcePtr, *input);
			}

			for (const EmitterTrackInput& trackInput : kEmitterTrackInputs)
			{
				if (Pin* input = _find_input_pin(*emitterPtr, trackInput.pin_name))
					addTrackNode(sourceEmitter.*trackInput.value, *input);
			}

			const std::vector<particles::ParticleFieldDefinition>* fieldGroups[] = {
				&sourceEmitter.particle_fields,
				&sourceEmitter.system_fields
			};
			const char* fieldPinNames[] = { "Particle Fields", "System Fields" };
			for (wx::Size fieldGroup = 0; fieldGroup < 2; ++fieldGroup)
			{
				Pin* fieldsInput = _find_input_pin(
					*emitterPtr,
					fieldPinNames[fieldGroup]);
				if (fieldsInput == nullptr)
					continue;

				for (const particles::ParticleFieldDefinition& sourceField :
					*fieldGroups[fieldGroup])
				{
					auto field = std::make_unique<ParticleFieldNode>();
					ParticleFieldNode* fieldPtr = field.get();
					fieldPtr->_get_definition() = sourceField;
					fieldPtr->_get_definition().x = {};
					fieldPtr->_get_definition().y = {};
					AddNode(std::move(field), false);
					connectNodes(*fieldPtr, *fieldsInput);

					if (Pin* input = _find_input_pin(*fieldPtr, "X"))
						addTrackNode(sourceField.x, *input);
					if (Pin* input = _find_input_pin(*fieldPtr, "Y"))
						addTrackNode(sourceField.y, *input);
				}
			}
		}

		auto system = std::make_unique<ParticleSystemDefinitionNode>();
		ParticleSystemDefinitionNode* systemPtr = system.get();
		AddNode(std::move(system), false);
		Pin* emitterInput = _find_input_pin(*systemPtr, "Emitters");
		if (emitterInput == nullptr)
		{
			error = ML("graph.create_system_input_failed");
			Clear();
			return false;
		}
		for (EmitterNode* emitter : emitterNodes)
			connectNodes(*emitter, *emitterInput);

		_arrange_nodes();
		error.clear();
		return true;
	}
}
