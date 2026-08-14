#ifndef POP_NODE_PARTICLE_SYSTEM_DEFINITION_H
#define POP_NODE_PARTICLE_SYSTEM_DEFINITION_H

#include "../Frames/PopNodeEditor.h"
#include "../Particles/ParticleSystem.h"

namespace pop
{
	class ParticleSystemDefinitionNode final : public PopNodeEditor::Node
	{
	public:
		ParticleSystemDefinitionNode();
		virtual ~ParticleSystemDefinitionNode() = default;

	protected:
		void _on_initialize(PopNodeEditor& editor) override;
		std::unique_ptr<Node> _clone() const override;
		nlohmann::json _serialize() const override;
		void _deserialize(const nlohmann::json& data) override;
		void _on_render_workspace() override;

	private:
		particles::ParticleSystemDefinition _definition;
		bool _duplicate = false;
	};
}

#endif // !POP_NODE_PARTICLE_SYSTEM_DEFINITION_H
