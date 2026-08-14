#ifndef POP_NODE_EMITTER_H
#define POP_NODE_EMITTER_H

#include "../Frames/PopNodeEditor.h"
#include "../Particles/ParticleEmitter.h"

namespace pop
{
	class EmitterNode final : public PopNodeEditor::Node
	{
		friend class PopNodeEditor;

	public:
		EmitterNode();
		virtual ~EmitterNode() = default;

	protected:
		void _on_initialize(PopNodeEditor& editor) override;
		std::unique_ptr<Node> _clone() const override;
		nlohmann::json _serialize() const override;
		void _deserialize(const nlohmann::json& data) override;
		void _on_render_workspace() override;

	private:
		const particles::ParticleEmitterDefinition& _get_definition() const noexcept;
		void _set_definition(const particles::ParticleEmitterDefinition& definition);

	private:
		particles::ParticleEmitterDefinition _definition;
		char _definition_name[128]{};
		char _on_duration[128]{};
	};
}

#endif // !POP_NODE_EMITTER_H
