#ifndef POP_NODE_PARTICLE_FIELD_H
#define POP_NODE_PARTICLE_FIELD_H

#include "../Frames/PopNodeEditor.h"
#include "../Particles/ParticleField.h"

namespace pop
{
	class ParticleFieldNode final : public PopNodeEditor::Node
	{
		friend class PopNodeEditor;

	public:
		ParticleFieldNode();
		virtual ~ParticleFieldNode() = default;

	protected:
		void _on_initialize(PopNodeEditor& editor) override;
		std::unique_ptr<Node> _clone() const override;
		nlohmann::json _serialize() const override;
		void _deserialize(const nlohmann::json& data) override;
		void _on_render_workspace() override;

	private:
		const particles::ParticleFieldDefinition& _get_definition() const noexcept;
		particles::ParticleFieldDefinition& _get_definition() noexcept;

	private:
		particles::ParticleFieldDefinition _definition;
	};
}

#endif // !POP_NODE_PARTICLE_FIELD_H
