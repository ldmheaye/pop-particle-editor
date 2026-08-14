#include "../../include/Frames/PopParticleViewer.h"
#include "../../include/PopRender.h"
#include "../../include/Fonts/IconsFontAwesome7.h"
#include "../../include/PopState.h"
#include <algorithm>

namespace
{
	bool TrackCanBePositive(const pop::particles::FloatTrack& track)
	{
		return std::any_of(
			track.nodes.begin(),
			track.nodes.end(),
			[](const pop::particles::FloatTrackNode& node)
			{
				return node.low_value > 0.0f || node.high_value > 0.0f;
			});
	}

	bool TrackIsAlwaysZero(const pop::particles::FloatTrack& track)
	{
		return !track.nodes.empty() && std::all_of(
			track.nodes.begin(),
			track.nodes.end(),
			[](const pop::particles::FloatTrackNode& node)
			{
				return node.low_value == 0.0f && node.high_value == 0.0f;
			});
	}
}

namespace pop
{
	void PopParticleViewer::OnRender()
	{
		ImVec2 size = ImGui::GetContentRegionAvail();
		ImVec2 min = ImGui::GetCursorScreenPos();
		ImVec2 max = { min.x + size.x, min.y + size.y };
		if (!_view_initialized)
		{
			_offset = { min.x + 20.0f, min.y + 20.0f };
			_view_initialized = true;
		}

		const bool viewerFocused = ImGui::IsWindowFocused(
			ImGuiFocusedFlags_RootAndChildWindows);
		const bool viewerHovered = ImGui::IsWindowHovered(
			ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
		const bool mouseInsideCanvas = ImGui::IsMouseHoveringRect(min, max);

		RenderGridBackground({ min,max }, { 40,40 }, _zoom, _offset);

		ImDrawList* list = ImGui::GetWindowDrawList();
		_particle_holder.SetSystemPosition(_emit_offset);
		_particle_holder.OnUpdate(ImGui::GetIO().DeltaTime);

		_render_particles(list, min, max);
		_render_emitter_position(list, min, max);

		ImGui::SetCursorScreenPos({ min.x + 10.0f, max.y - ImGui::GetFrameHeight() - 10.0f });
		const ImVec2 toolbarMin = ImGui::GetCursorScreenPos();
		ImGui::BeginGroup();
		const std::string emitLabel = std::string(ICON_FA_ROCKET) + " " + ML("viewer.emit");
		if (ImGui::Button(emitLabel.c_str()))
			_emit_particles();
		if (ImGui::IsItemHovered())
			ImGui::SetItemTooltip("%s", ML("viewer.emit_tooltip"));
		ImGui::SameLine();
		if (ImGui::Button(
			_particle_holder.IsEmulating() ? ICON_FA_PAUSE : ICON_FA_PLAY))
		{
			if (_particle_holder.IsEmulating())
				_particle_holder.PauseEmulate();
			else
				_particle_holder.StartEmulate();
		}
		if (ImGui::IsItemHovered())
			ImGui::SetItemTooltip(
				"%s",
				_particle_holder.IsEmulating()
					? ML("viewer.pause")
					: ML("viewer.play"));
		ImGui::SameLine();
		wx::Float32 speed = _particle_holder.GetEmulateSpeed();
		ImGui::SetNextItemWidth(90.0f);
		if (ImGui::SliderFloat(
			"##simulation_speed",
			&speed,
			0.05f,
			4.0f,
			ML("viewer.speed_format")))
			_particle_holder.SetEmulateSpeed(speed);
		if (ImGui::IsItemHovered())
			ImGui::SetItemTooltip("%s", ML("viewer.speed"));
		ImGui::SameLine();
		ImGui::TextDisabled(ML("viewer.particle_count"), _particle_holder.GetParticleCount());
		ImGui::SameLine();
		ImGui::SetNextItemWidth(100.0f);
		ImGui::DragInt("##stage_width", &_stage_width, 1);
		ImGui::SameLine();
		ImGui::TextUnformatted(ML("viewer.stage_separator"));
		ImGui::SameLine();
		ImGui::SetNextItemWidth(100.0f);
		ImGui::DragInt("##stage_height", &_stage_height, 1);
		_stage_width = std::max(_stage_width, 1);
		_stage_height = std::max(_stage_height, 1);
		if (ImGui::IsItemHovered())
			ImGui::SetItemTooltip("%s", ML("viewer.stage_size"));
		ImGui::EndGroup();
		const ImVec2 toolbarMax = ImGui::GetItemRectMax();
		const bool toolbarHovered = ImGui::IsMouseHoveringRect(toolbarMin, toolbarMax);

		bool statusHovered = false;
		const bool emitFailed = !_emit_error.empty();
		if (emitFailed || !_emit_status.empty())
		{
			std::string& message = emitFailed ? _emit_error : _emit_status;
			const float messageWidth = std::min(
				std::max(size.x - 20.0f, 1.0f),
				620.0f);
			ImGui::SetCursorScreenPos({ min.x + 10.0f, min.y + 10.0f });
			ImGui::PushStyleColor(
				ImGuiCol_ChildBg,
				emitFailed ? IM_COL32(92, 24, 24, 235) : IM_COL32(20, 76, 45, 235));
			ImGui::PushStyleColor(
				ImGuiCol_Border,
				emitFailed ? IM_COL32(235, 85, 85, 255) : IM_COL32(75, 205, 125, 255));
			ImGui::BeginChild(
				"##particle_emit_status",
				{ messageWidth, 0.0f },
				ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
			ImGui::TextColored(
				emitFailed
					? ImVec4(1.0f, 0.45f, 0.45f, 1.0f)
					: ImVec4(0.45f, 1.0f, 0.62f, 1.0f),
				"%s %s",
				emitFailed ? ICON_FA_CIRCLE_EXCLAMATION : ICON_FA_ROCKET,
				emitFailed ? ML("viewer.emit_failed") : ML("viewer.emit_started"));
			ImGui::SameLine();
			if (ImGui::SmallButton(ML("viewer.dismiss")))
				message.clear();
			if (!message.empty())
				ImGui::TextWrapped("%s", message.c_str());
			ImGui::EndChild();
			statusHovered = ImGui::IsMouseHoveringRect(
				ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
			ImGui::PopStyleColor(2);
		}

		const bool canvasInteractive = viewerFocused && viewerHovered &&
			mouseInsideCanvas && !toolbarHovered && !statusHovered;
		_handle_drag(canvasInteractive);
		_handle_zoom(canvasInteractive);
		ImGui::SetCursorScreenPos(min);
		ImGui::Dummy(size);
	}

	void PopParticleViewer::SetParticleSystem(
		const particles::ParticleSystemDefinition& definition,
		std::string_view revision)
	{
		_particle_system_error.clear();
		if (_particle_revision == revision)
			return;
		_emit_error.clear();
		_emit_status.clear();
		_particle_definition = definition;
		_particle_revision.assign(revision.data(), revision.size());
	}

	void PopParticleViewer::SetParticleSystemError(std::string_view error)
	{
		if (_particle_system_error == error &&
			_particle_revision.empty() && _particle_definition.emitters.empty())
		{
			return;
		}
		_particle_system_error.assign(error.data(), error.size());
		_emit_status.clear();
		_particle_revision.clear();
		_particle_definition = {};
		_particle_holder.Emitt(_particle_definition);
	}

	void PopParticleViewer::ClearParticleSystem()
	{
		_particle_system_error.clear();
		_emit_error.clear();
		_emit_status.clear();
		if (_particle_revision.empty() && _particle_definition.emitters.empty())
			return;
		_particle_revision.clear();
		_particle_definition = {};
		_particle_holder.Emitt(_particle_definition);
	}

	bool PopParticleViewer::_emit_particles()
	{
		_emit_error.clear();
		_emit_status.clear();
		if (!_particle_system_error.empty())
		{
			_emit_error = _particle_system_error;
			return false;
		}
		if (_particle_definition.emitters.empty())
		{
			_emit_error = ML("viewer.no_emitter");
			return false;
		}

		bool hasRunnableEmitter = false;
		for (wx::Size index = 0; index < _particle_definition.emitters.size(); ++index)
		{
			const particles::ParticleEmitterDefinition& emitter =
				_particle_definition.emitters[index];
			const std::string emitterName = emitter.name.empty()
				? MLF("viewer.emitter_index", index + 1)
				: MLF("viewer.emitter_name", emitter.name.c_str());

			if (emitter.image.empty())
			{
				_emit_error = MLF("viewer.no_resource", emitterName.c_str());
				return false;
			}
			const auto resource = gPopResources.find(emitter.image);
			if (resource == gPopResources.end())
			{
				_emit_error = MLF(
					"viewer.resource_unloaded",
					emitterName.c_str(),
					emitter.image.c_str());
				return false;
			}
			if (emitter.image_cols <= 0 || emitter.image_rows <= 0 ||
				emitter.image_col < 0 || emitter.image_row < 0 ||
				emitter.image_col >= emitter.image_cols ||
				emitter.image_row >= emitter.image_rows ||
				emitter.image_frames <= 0 ||
				emitter.image_col + emitter.image_frames > emitter.image_cols)
			{
				_emit_error = MLF("viewer.invalid_atlas", emitterName.c_str());
				return false;
			}
			if (emitter.particle_duration.nodes.empty())
			{
				_emit_error = MLF("viewer.no_duration", emitterName.c_str());
				return false;
			}

			const bool crossFadeOnly = !emitter.cross_fade_duration.nodes.empty() &&
				emitter.cross_fade_duration.nodes.front().curve !=
					particles::CurveType::Constant;
			if (!crossFadeOnly)
			{
				hasRunnableEmitter = true;
				if (!TrackCanBePositive(emitter.spawn_rate) &&
					!TrackCanBePositive(emitter.spawn_min_active))
				{
					_emit_error = MLF("viewer.cannot_spawn", emitterName.c_str());
					return false;
				}
				if (TrackIsAlwaysZero(emitter.spawn_max_active))
				{
					_emit_error = MLF("viewer.max_active_zero", emitterName.c_str());
					return false;
				}
				if (TrackIsAlwaysZero(emitter.spawn_max_launched))
				{
					_emit_error = MLF("viewer.max_launched_zero", emitterName.c_str());
					return false;
				}
				if (!TrackCanBePositive(emitter.system_alpha) ||
					!TrackCanBePositive(emitter.particle_alpha))
				{
					_emit_error = MLF("viewer.alpha_invisible", emitterName.c_str());
					return false;
				}
				if (!TrackCanBePositive(emitter.particle_scale))
				{
					_emit_error = MLF("viewer.scale_invisible", emitterName.c_str());
					return false;
				}
			}
		}
		if (!hasRunnableEmitter)
		{
			_emit_error = ML("viewer.no_start_emitter");
			return false;
		}

		_particle_holder.SetSystemPosition(_emit_offset);
		_particle_holder.Emitt(_particle_definition);
		_particle_holder.StartEmulate();
		_emit_status = MLF("viewer.started", _particle_definition.emitters.size());
		return true;
	}

	void PopParticleViewer::_render_emitter_position(
		ImDrawList* canvas,
		const ImVec2& clipMin,
		const ImVec2& clipMax)
	{
		ImVec2 pivot = _offset;
		pivot.x += _emit_offset.x * _zoom;
		pivot.y += _emit_offset.y * _zoom;

		const ImVec2 textSize = ImGui::CalcTextSize(ICON_FA_CROSSHAIRS);
		const ImVec2 hitMin{
			pivot.x - textSize.x * 0.75f,
			pivot.y - textSize.y * 0.75f
		};
		const ImVec2 hitMax{
			pivot.x + textSize.x * 0.75f,
			pivot.y + textSize.y * 0.75f
		};
		const bool insideClip = hitMax.x >= clipMin.x && hitMin.x <= clipMax.x &&
			hitMax.y >= clipMin.y && hitMin.y <= clipMax.y;
		_is_emit_pos_hovered = insideClip &&
			ImGui::IsMouseHoveringRect(hitMin, hitMax);

		const ImColor iconColor = _is_emit_pos_hovered
			? ImColor(0xFF, 0xD5, 0x5E)
			: ImColor(255, 255, 255);
		canvas->PushClipRect(clipMin, clipMax);
		if (_is_emit_pos_hovered)
		{
			canvas->AddCircle(
				pivot,
				textSize.x * 0.75f,
				iconColor, 0, 3.0f);
		}
		canvas->AddText(
			{
				pivot.x - textSize.x * 0.5f,
				pivot.y - textSize.y * 0.5f
			},
			iconColor, ICON_FA_CROSSHAIRS);
		canvas->PopClipRect();
	}

	void PopParticleViewer::_render_particles(
		ImDrawList* canvas,
		const ImVec2& clipMin,
		const ImVec2& clipMax)
	{
		ImVec2 pivot = _offset;

		canvas->PushClipRect(clipMin, clipMax);

		canvas->AddRect(pivot, { pivot.x + _stage_width * _zoom, pivot.y + _stage_height * _zoom },
			ImColor(0, 100, 0), 0, 2.0f);

		canvas->AddCircleFilled(pivot, 5.0f, ImColor(255,0,0));

		// 开始渲染粒子系统
		_particle_holder.OnRenderParticles(
			canvas,
			pivot,
			_zoom,
			{ static_cast<wx::Float32>(_stage_width), static_cast<wx::Float32>(_stage_height) });

		canvas->PopClipRect();
	}

	void PopParticleViewer::_handle_zoom(bool canvasHovered)
	{
		if (!canvasHovered)
			return;

		ImGuiIO& io = ImGui::GetIO();
		const wx::Float32 wheel_y = io.MouseWheel;
		_zoom_dst += wheel_y / 10.0f;

		_zoom_dst = std::clamp(_zoom_dst, 0.5f, 2.0f);

		wx::Float32 distance = _zoom_dst - _zoom;

		wx::Float32 deltaZoom = 0;

		if (distance > 0.02f)
			deltaZoom = 0.02f;
		else if (distance < -0.02f)
			deltaZoom = -0.02f;

		if (deltaZoom != 0)
		{
			ImVec2 deltaOffset = ImGui::GetMousePos();
			deltaOffset.x -= _offset.x;
			deltaOffset.y -= _offset.y;

			deltaOffset.x = deltaOffset.x * (_zoom + deltaZoom) / _zoom - deltaOffset.x;
			deltaOffset.y = deltaOffset.y * (_zoom + deltaZoom) / _zoom - deltaOffset.y;

			_zoom += deltaZoom;

			_offset.x -= deltaOffset.x;
			_offset.y -= deltaOffset.y;
		}

	}

	void PopParticleViewer::_handle_drag(bool canvasHovered)
	{
		if (canvasHovered && ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
			if (!_right_start_dragging)
			{
				_offset_buffer = _offset;
				_right_start_dragging = true;
			}

			ImVec2 delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Right);
			_offset.x = _offset_buffer.x + delta.x;
			_offset.y = _offset_buffer.y + delta.y;
		}
		else
			_right_start_dragging = false;

		if (canvasHovered && ImGui::IsMouseDown(ImGuiMouseButton_Left))
		{
			if (_is_emit_pos_hovered && !_emit_pos_start_dragging)
			{
				_emit_offset_buffer = _emit_offset;
				_emit_pos_start_dragging = true;
			}
		}
		else
			_emit_pos_start_dragging = false;

		if (_emit_pos_start_dragging && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
			ImVec2 delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
			_emit_offset.x = _emit_offset_buffer.x + delta.x / _zoom;
			_emit_offset.y = _emit_offset_buffer.y + delta.y / _zoom;
		}
	}

}
