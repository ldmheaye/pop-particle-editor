#ifndef __POP_NODE_TRACK_H__
#define __POP_NODE_TRACK_H__

#include "../Frames/PopNodeEditor.h"
#include "../Particles/ParticleTrack.h"

namespace pop
{
	class NodeTrackNode final : public PopNodeEditor::Node
	{
		friend class PopNodeEditor;

	public:
		using NodeIndex = wx::Int32;
	public:
		NodeTrackNode();
		virtual ~NodeTrackNode() {};

	protected:
		void _on_initialize(PopNodeEditor& editor) override;
		std::unique_ptr<Node> _clone() const override;
		nlohmann::json _serialize() const override;
		void _deserialize(const nlohmann::json& data) override;
		void _on_render_workspace() override;
		void _on_render_overlay() override;

	private:
		void _add_node(wx::Float32 time, wx::Float32 value);
		void _fit_view_to_track();
		const particles::FloatTrack& _get_track() const noexcept;
		particles::FloatTrack& _get_track() noexcept;

	private:
		particles::FloatTrack _track;
		NodeIndex _selected_node_index = -1;
		bool _selected_high_value = false;
		NodeIndex _dragging_node = -1;
		bool _dragging_vertical_only = false;
		bool _splitting_range = false;
		wx::Float32 _split_anchor_value = 0.0f;
		wx::Float32 _view_min = -1.0f;
		wx::Float32 _view_max = 1.0f;
		std::string _anchor_tooltip;
	};

	class ConstantNodeTrackNode final : public PopNodeEditor::Node
	{
		friend class PopNodeEditor;

	public:
		ConstantNodeTrackNode();
		virtual ~ConstantNodeTrackNode();

	protected:
		void _on_initialize(PopNodeEditor& editor) override;
		std::unique_ptr<Node> _clone() const override;
		nlohmann::json _serialize() const override;
		void _deserialize(const nlohmann::json& data) override;
		void _on_render_workspace() override;

	private:
		wx::Float32 _get_value() const noexcept;
		void _set_value(wx::Float32 value) noexcept;

	private:
		wx::Float32 _value;
	};
}

#endif // !__POP_NODE_TRACK_H__
