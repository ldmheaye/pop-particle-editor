#ifndef __POP_NODE_EDITOR_H__
#define __POP_NODE_EDITOR_H__

#include <memory>
#include <list>
#include <vector>
#include <variant>
#include <unordered_map>
#include <imgui-node-editor/imgui_node_editor.h>
#include <nlohmann/json.hpp>

#include <WXBase/WXDefinitions.h>
#include <functional>
#include <optional>
#include <string_view>

namespace pop
{
	class PopNodeFactory;
	namespace particles
	{
		struct FloatTrack;
		struct ParticleSystemDefinition;
	}

	class PopNodeEditor
	{
	public:
		enum class PinShape
		{
			TriAngle,
			Circle,
			Square
		};

		enum class NodeType
		{
			Normal,
			Comment
		};
		struct Link;

		struct Pin
		{
			friend class PopNodeEditor;

			ax::NodeEditor::PinId id;
			ax::NodeEditor::PinKind kind = ax::NodeEditor::PinKind::Input;

			bool multi_input_pins = false; // 允许多pin输入/输出

			std::vector<Link*> connected_links;

			ax::NodeEditor::NodeId node_id;

			std::string name;
			std::string type; // 用于匹配相同类型的pin，可能换成type definition

			ImColor color = 0xFFFFFFFF;
			PinShape shape = PinShape::TriAngle;
			wx::Float32 layout_y = -1.0f;

		private:
			static bool _can_create_link(Pin& start, Pin& end)
			{
				if (start.kind == end.kind)
					return false;

				if (start.node_id == end.node_id || start.type != end.type)
					return false;

				for (const Link* startLink : start.connected_links)
				{
					for (const Link* endLink : end.connected_links)
					{
						if (startLink == endLink)
							return false;
					}
				}

				Pin& input = start.kind == ax::NodeEditor::PinKind::Input ? start : end;
				return input.multi_input_pins || input.connected_links.empty();
			}
		};

		class Node
		{
			friend class PopNodeEditor;
		public:
			Node();
			virtual ~Node() {};

		protected:
			virtual void _on_initialize(PopNodeEditor& editor) {};
			virtual std::unique_ptr<Node> _clone() const = 0;
			virtual nlohmann::json _serialize() const = 0;
			virtual void _deserialize(const nlohmann::json& data) = 0;
			virtual void _on_render();
			virtual void _on_render_workspace();
			virtual void _on_render_overlay();

			const ax::NodeEditor::NodeId& _get_id() const;
			PopNodeEditor::Pin& _add_pin(
				PopNodeEditor& editor,
				const Pin& pin);
			Node* _find_node(
				PopNodeEditor& editor,
				std::string_view name);
			void _set_item_tooltip(std::string_view text);

		private:
			const std::string& _get_serialization_type() const noexcept;
			void _render_pin(Pin& pin);
			wx::Int32 _calc_pin_width(const Pin& pin);
			PopNodeEditor::Pin& _store_pin(const Pin& pin, ax::NodeEditor::PinId id);

		protected:
			NodeType _type;
			ImColor _header_color;

			std::string _name;
			std::string _serialization_type;
			std::list<Pin> _input_pins;
			std::list<Pin> _output_pins;

		private:
			ax::NodeEditor::NodeId _id;
			std::string _tooltip;
		};

		struct Link
		{
			ax::NodeEditor::LinkId id;
			ax::NodeEditor::PinId start_pin_id;
			ax::NodeEditor::PinId end_pin_id;
			ImColor color;
		};

	public:
		PopNodeEditor();
		~PopNodeEditor();

	public:
		void OnRender();

		void AddNode(std::unique_ptr<Node>&& node, bool moveToMouse);

		nlohmann::json Export() const;
		bool Import(
			const nlohmann::json& document,
			PopNodeFactory& factory,
			std::string& error);
		bool ExportParticleSystem(
			particles::ParticleSystemDefinition& definition,
			std::string& error) const;
		bool ImportParticleSystem(
			const particles::ParticleSystemDefinition& definition,
			std::string& error);
		void Clear();

	private:
		static void _draw_pin_shape(const Pin& pin, bool connected);
		wx::Size _get_next_id();
		Node* _find_node(std::string_view name, const Node* excluded = nullptr);
		Pin& _add_node_pin(Node& node, const Pin& pin);
		void _add_link(Pin& start, Pin& end);
		void _delete_link(ax::NodeEditor::LinkId id);
		void _delete_node(ax::NodeEditor::NodeId id);
		void _handle_link_creation();
		void _handle_deletion();
		void _arrange_nodes();
		void _apply_node_layout();
		Pin* _find_input_pin(Node& node, std::string_view name);
		const Pin* _find_input_pin(
			const Node& node,
			std::string_view name) const;
		std::vector<const Node*> _find_connected_nodes(const Pin& input) const;
		void _connect_node(Node& source, Pin& input);
		static bool _read_track_node(
			const Node& node,
			particles::FloatTrack& track);
		void _create_editor();
		void _reset_editor(std::string settings);
		static bool _save_editor_settings(
			const char* data,
			wx::Size size,
			ax::NodeEditor::SaveReasonFlags reason,
			void* userPointer);
		static wx::Size _load_editor_settings(char* data, void* userPointer);

	private:
		ax::NodeEditor::EditorContext* _p_editor;
		std::string _editor_settings;
		std::vector<std::unique_ptr<Node>> _nodes;
		std::list<Link> _links;

		std::unordered_map<wx::Size, Node*> _nodes_map;
		std::unordered_map<wx::Size, Pin*> _pins_map;

		ax::NodeEditor::NodeId _context_node_id;
		ax::NodeEditor::LinkId _context_link_id;

		wx::Size _next_id;
		bool _arrange_nodes_pending;
	};

	class PopNodeFactory
	{
	public:
		using NodeCreator = std::function<std::unique_ptr<PopNodeEditor::Node>()>;

		template<typename T>
		static std::unique_ptr<PopNodeEditor::Node> MakeCreator()
		{
			return std::make_unique<T>();
		}

	public:
		PopNodeFactory() {};
		~PopNodeFactory() {};

		void Register(std::string_view type, const NodeCreator& creator);

		std::optional<std::unique_ptr<PopNodeEditor::Node>>
			Instantiate(std::string_view type);

	private:
		std::unordered_map<std::string, NodeCreator> _creators;
	};

}

#endif // !__POP_NODE_EDITOR_H__
