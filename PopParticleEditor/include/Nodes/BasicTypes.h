#ifndef __POP_NODES_BASIC_H__
#define __POP_NODES_BASIC_H__

#include "../Frames/PopNodeEditor.h"

namespace pop
{
	class FloatNode final : public PopNodeEditor::Node
	{
	public:
		FloatNode();
		virtual ~FloatNode();

	protected:
		void _on_initialize(PopNodeEditor& editor) override;
		std::unique_ptr<Node> _clone() const override;
		nlohmann::json _serialize() const override;
		void _deserialize(const nlohmann::json& data) override;
		void _on_render_workspace() override;

	private:
		wx::Float32 _value;
	};

	class IntNode final : public PopNodeEditor::Node
	{
	public:
		IntNode();
		virtual ~IntNode();

	protected:
		void _on_initialize(PopNodeEditor& editor) override;
		std::unique_ptr<Node> _clone() const override;
		nlohmann::json _serialize() const override;
		void _deserialize(const nlohmann::json& data) override;
		void _on_render_workspace() override;

	private:
		wx::Int32 _value;
	};

	class StringNode final : public PopNodeEditor::Node
	{
	public:
		StringNode();
		virtual ~StringNode();

	protected:
		void _on_initialize(PopNodeEditor& editor) override;
		std::unique_ptr<Node> _clone() const override;
		nlohmann::json _serialize() const override;
		void _deserialize(const nlohmann::json& data) override;
		void _on_render_workspace() override;

	private:
		char _value[128];
	};
}

#endif // !__POP_NODES_BASIC_H__
