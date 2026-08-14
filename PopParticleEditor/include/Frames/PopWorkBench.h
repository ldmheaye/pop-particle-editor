#ifndef __POP_WORKBENCH_H__
#define __POP_WORKBENCH_H__

#include "PopNodeEditor.h"
#include "../PopResource.h"
#include <WXBase/WXImGuiFrame.h>
#include "PopResourceView.h"
#include "PopParticleViewer.h"
#include <filesystem>

namespace pop
{

	class PopWorkBench final : public wx::ImGuiFrame
	{
	public:
		PopWorkBench();
		~PopWorkBench();

	public:
		virtual void OnRender() override;

	public:
		bool LoadResource(const std::filesystem::path& path);

		void CreateNewProject();
		bool SaveProject(const std::filesystem::path& path, std::string& error);
		bool PublishProject(const std::filesystem::path& path, std::string& error);
		bool OpenProject(const std::filesystem::path& path, std::string& error);
		bool HasProjectPath() const noexcept;
		const std::filesystem::path& GetProjectPath() const noexcept;

	private:
		void _register_nodes();
		void _render_node_option(std::string_view type_name);

	private:
		PopNodeEditor _node_editor;
		PopNodeFactory _node_factory;
		PopResourceView _resource_view;
		PopParticleViewer _particle_viewer;
		std::filesystem::path _project_path;
	};
}

#endif // !__POP_WORKBENCH_H__
