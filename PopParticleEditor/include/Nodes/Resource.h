#ifndef __POP_NODE_RESOURCE_H__
#define __POP_NODE_RESOURCE_H__

#include <string_view>

#include "../Frames/PopNodeEditor.h"

namespace pop
{
	class ResourceNode final : public PopNodeEditor::Node
	{
		friend class PopNodeEditor;

	public:
		ResourceNode();
		explicit ResourceNode(std::string_view resourceId);
		virtual ~ResourceNode();

	protected:
		void _on_initialize(PopNodeEditor& editor) override;
		std::unique_ptr<Node> _clone() const override;
		nlohmann::json _serialize() const override;
		void _deserialize(const nlohmann::json& data) override;
		void _on_render_workspace() override;
		void _on_render_overlay() override;

	private:
		std::string_view _get_resource_id() const noexcept;
		wx::Int32 _get_image_col() const noexcept;
		wx::Int32 _get_image_row() const noexcept;
		wx::Int32 _get_image_frames() const noexcept;
		void _set_image_selection(
			wx::Int32 imageCol,
			wx::Int32 imageRow,
			wx::Int32 imageFrames) noexcept;

		void _set_resource_id(std::string_view resourceId);

	private:
		char _resource_id[128];
		wx::Int32 _image_cols = 1;
		wx::Int32 _image_rows = 1;
		wx::Int32 _image_col = 0;
		wx::Int32 _image_row = 0;
		wx::Int32 _image_frames = 1;
		wx::Int32 _preview_frame = 0;
		bool _open_resource_picker = false;
	};
}

#endif // !__POP_NODE_RESOURCE_H__
