#include "../../include/Frames/PopWorkBench.h"

#include "../../include/Nodes/BasicTypes.h"
#include "../../include/Nodes/Emitter.h"
#include "../../include/Nodes/NodeTrack.h"
#include "../../include/Nodes/ParticleField.h"
#include "../../include/Nodes/ParticleSystemDefinition.h"
#include "../../include/Nodes/Resource.h"
#include "../../include/Particles/ParticleXml.h"

#include "../../include/Fonts/IconsFontAwesome7.h"
#include <imgui.h>
#include <WXMedia/Texture.h>
#include "../../include/PopState.h"

#include <cctype>
#include <fstream>
#include <iterator>

namespace
{
	std::string MakeCrlfText(std::string_view content)
	{
		std::string crlfContent;
		crlfContent.reserve(content.size() + content.size() / 16 + 2);
		for (const char character : content)
		{
			if (character == '\r')
				continue;
			if (character == '\n')
				crlfContent += "\r\n";
			else
				crlfContent += character;
		}
		if (crlfContent.size() < 2 ||
			crlfContent.compare(crlfContent.size() - 2, 2, "\r\n") != 0)
		{
			crlfContent += "\r\n";
		}
		return crlfContent;
	}
}

namespace pop
{
	PopWorkBench::PopWorkBench()
	{
		_register_nodes();
	}

	PopWorkBench::~PopWorkBench()
	{

	}

	void PopWorkBench::OnRender()
	{
		ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
		const std::string resourcesWindow = std::string(ML("window.resources")) + "###Resources";
		const std::string nodesWindow = std::string(ML("window.nodes")) + "###Nodes";
		const std::string workspaceWindow = std::string(ML("window.workspace")) + "###Work Space";
		const std::string viewerWindow = std::string(ML("window.particle_viewer")) + "###Particle Viewer";

		// Navigation
		if (ImGui::Begin(resourcesWindow.c_str()))
		{
			_resource_view.OnRender();
		}
		ImGui::End();

		if (ImGui::Begin(nodesWindow.c_str()))
		{
			// Nodes
			{
				std::string title = ICON_FA_CUBES;
				title += " ";
				title += ML("category.basic");
				if (ImGui::CollapsingHeader(title.c_str()))
				{
					_render_node_option("Float");
					_render_node_option("Int");
					_render_node_option("String");
				}
			}

			{
				std::string title = ICON_FA_CODE_BRANCH;
				title += " ";
				title += ML("category.popcap");

				if (ImGui::CollapsingHeader(title.c_str()))
				{
					_render_node_option("NodeTrack");
					_render_node_option("ConstantNodeTrack");
					_render_node_option("Resource");
					_render_node_option("ParticleField");
					_render_node_option("Emitter");
					_render_node_option("ParticleSystemDefinition");
				}
			}


		}
		ImGui::End();

		if (ImGui::Begin(workspaceWindow.c_str()))
		{
			_node_editor.OnRender();

			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("NODE"))
				{
					std::string node = reinterpret_cast<char*>(payload->Data);

					if (auto ans = _node_factory.Instantiate(node))
					{
						_node_editor.AddNode(std::move(ans.value()), true);
					}
				}

				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kPopResourceDragDropPayload))
				{
					if (payload->Data != nullptr && payload->DataSize > 0)
					{
						const char* resourceId = static_cast<const char*>(payload->Data);
						wx::Size resourceIdLength = static_cast<wx::Size>(payload->DataSize);
						if (resourceId[resourceIdLength - 1] == '\0')
							--resourceIdLength;

						auto node = std::make_unique<ResourceNode>(
							std::string_view(resourceId, resourceIdLength));
						_node_editor.AddNode(std::move(node), true);
					}
				}

				ImGui::EndDragDropTarget();
			}
		}

		ImGui::End();

		if (ImGui::Begin(viewerWindow.c_str()))
		{
			particles::ParticleSystemDefinition definition;
			std::string error;
			std::string revision;
			if (_node_editor.ExportParticleSystem(definition, error) &&
				particles::SerializeParticleSystemXml(definition, revision, error))
			{
				for (const particles::ParticleEmitterDefinition& emitter : definition.emitters)
				{
					revision += '#';
					revision += std::to_string(emitter.image_cols);
					revision += 'x';
					revision += std::to_string(emitter.image_rows);
				}
				_particle_viewer.SetParticleSystem(definition, revision);
			}
			else
			{
				_particle_viewer.SetParticleSystemError(
					error.empty() ? ML("graph.invalid") : error);
			}
			_particle_viewer.OnRender();
		}

		ImGui::End();
	}

	void PopWorkBench::_register_nodes()
	{
		_node_factory.Register("Float", PopNodeFactory::MakeCreator<FloatNode>);
		_node_factory.Register("Int", PopNodeFactory::MakeCreator<IntNode>);
		_node_factory.Register("String", PopNodeFactory::MakeCreator<StringNode>);
		_node_factory.Register("NodeTrack", PopNodeFactory::MakeCreator<NodeTrackNode>);
		_node_factory.Register("ConstantNodeTrack", PopNodeFactory::MakeCreator<ConstantNodeTrackNode>);
		_node_factory.Register("Resource", PopNodeFactory::MakeCreator<ResourceNode>);
		_node_factory.Register("ParticleField", PopNodeFactory::MakeCreator<ParticleFieldNode>);
		_node_factory.Register("Emitter", PopNodeFactory::MakeCreator<EmitterNode>);
		_node_factory.Register("ParticleSystemDefinition", PopNodeFactory::MakeCreator<ParticleSystemDefinitionNode>);
	}

	void PopWorkBench::_render_node_option(std::string_view typeName)
	{
		ImGui::PushID(typeName.data());

		std::string finalText;
		finalText += ICON_FA_LINK;
		finalText += " ";
		finalText += ML(typeName.data());

		ImGui::PushStyleColor(ImGuiCol_Button, { 0, 0, 0, 0 });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 0.3f, 0.3f, 0.3f, 0.5f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, { 0.2f, 0.2f, 0.2f, 0.7f });
		ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, { 0.0f, 0.5f });

		ImGui::Button(finalText.c_str(), { ImGui::GetContentRegionAvail().x ,0 });

		if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {

			ImGui::SetDragDropPayload("NODE", typeName.data(), typeName.size() + 1);

			ImGui::Text("%s %s", ICON_FA_LINK, ML(typeName.data()));

			ImGui::EndDragDropSource();
		}

		ImGui::PopStyleVar(2);
		ImGui::PopStyleColor(3);
		ImGui::PopID();
	}

	bool PopWorkBench::LoadResource(const std::filesystem::path& path)
	{
		if (std::filesystem::is_regular_file(path))
		{
			ImVec2 size{};

			if (auto tex = wx::LoadTexture(path, size))
			{
				auto idStem = path.stem().string();
				std::transform(idStem.begin(), idStem.end(), idStem.begin(),
					[](unsigned char c) { return std::toupper(c); });

				std::string resId = "IMAGE_" + idStem;

				auto& res = gPopResources[resId];
				res.path = path;
				res.image = tex.value();
				res.size = size;
				res.id = resId;

				std::cout << MLF(
					"log.resource_loaded",
					res.path.string().c_str(),
					res.id.c_str()) << std::endl;

				return true;
			}
			else
				return false;
		}
		else
			return false;
	}

	void PopWorkBench::CreateNewProject()
	{
		_node_editor.Clear();
		_project_path.clear();
	}

	bool PopWorkBench::SaveProject(
		const std::filesystem::path& path,
		std::string& error)
	{
		std::string content = _node_editor.Export().dump(
			2,
			' ',
			false,
			nlohmann::json::error_handler_t::replace);
		const std::string crlfContent = MakeCrlfText(content);

		std::ofstream output(path, std::ios::binary | std::ios::trunc);
		if (!output)
		{
			error = ML("file.project_open_write_failed");
			return false;
		}

		output.write(
			crlfContent.data(),
			static_cast<wx::SSize>(crlfContent.size()));
		output.close();
		if (!output)
		{
			error = ML("file.project_write_failed");
			return false;
		}

		_project_path = path;
		error.clear();
		return true;
	}

	bool PopWorkBench::PublishProject(
		const std::filesystem::path& path,
		std::string& error)
	{
		particles::ParticleSystemDefinition definition;
		if (!_node_editor.ExportParticleSystem(definition, error))
			return false;

		std::string content;
		if (!particles::SerializeParticleSystemXml(definition, content, error))
			return false;
		const std::string crlfContent = MakeCrlfText(content);

		std::ofstream output(path, std::ios::binary | std::ios::trunc);
		if (!output)
		{
			error = ML("file.xml_open_write_failed");
			return false;
		}
		output.write(
			crlfContent.data(),
			static_cast<wx::SSize>(crlfContent.size()));
		output.close();
		if (!output)
		{
			error = ML("file.xml_write_failed");
			return false;
		}

		error.clear();
		return true;
	}

	bool PopWorkBench::OpenProject(
		const std::filesystem::path& path,
		std::string& error)
	{
		std::ifstream input(path, std::ios::binary);
		if (!input)
		{
			error = ML("file.project_open_failed");
			return false;
		}

		const std::string content{
			std::istreambuf_iterator<char>{ input },
			std::istreambuf_iterator<char>{}
		};
		std::string extension = path.extension().string();
		std::transform(
			extension.begin(),
			extension.end(),
			extension.begin(),
			[](unsigned char character)
			{
				return static_cast<char>(std::tolower(character));
			});

		if (extension == ".xml")
		{
			particles::ParticleSystemDefinition definition;
			if (!particles::DeserializeParticleSystemXml(content, definition, error) ||
				!_node_editor.ImportParticleSystem(definition, error))
			{
				return false;
			}
			_project_path.clear();
		}
		else
		{
			const nlohmann::json document = nlohmann::json::parse(
				content,
				nullptr,
				false);
			if (document.is_discarded())
			{
				error = ML("file.invalid_project_json");
				return false;
			}
			if (!_node_editor.Import(document, _node_factory, error))
				return false;
			_project_path = path;
		}

		error.clear();
		return true;
	}

	bool PopWorkBench::HasProjectPath() const noexcept
	{
		return !_project_path.empty();
	}

	const std::filesystem::path& PopWorkBench::GetProjectPath() const noexcept
	{
		return _project_path;
	}
}
