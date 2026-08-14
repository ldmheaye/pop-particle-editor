#ifndef __POP_PARTICLE_VIEWER_H__
#define __POP_PARTICLE_VIEWER_H__

#include <WXBase/WXImGuiFrame.h>
#include <WXBase/WXDefinitions.h>
#include <imgui.h>
#include <string>
#include <string_view>
#include "../Particles/ParticleHolder.h"


namespace pop
{
	class PopParticleViewer
	{
	public:
		PopParticleViewer() :
			_zoom(1.0f),
			_zoom_dst(1.0f),
			_emit_pos_start_dragging(false),
			_right_start_dragging(false),
			_is_emit_pos_hovered(false),
			_stage_width(800),
			_stage_height(600),
			_emit_offset{ 400,300 },
			_view_initialized(false)
		{};

		~PopParticleViewer() {};

	public:
		void OnRender();
		void SetParticleSystem(
			const particles::ParticleSystemDefinition& definition,
			std::string_view revision);
		void SetParticleSystemError(std::string_view error);
		void ClearParticleSystem();

	private:
		void _render_emitter_position(ImDrawList* canvas, const ImVec2& clip_min, const ImVec2& clip_max);
		void _render_particles(ImDrawList* canvas, const ImVec2& clip_min, const ImVec2& clip_max);
		bool _emit_particles();
		void _handle_zoom(bool canvas_hovered);
		void _handle_drag(bool canvas_hovered);

	private:
		ImVec2 _offset;
		wx::Float32 _zoom;

		wx::Float32 _zoom_dst;

		ImVec2 _emit_offset;

		bool _right_start_dragging;
		ImVec2 _offset_buffer;

		bool _is_emit_pos_hovered;
		bool _emit_pos_start_dragging;
		ImVec2 _emit_offset_buffer;

		wx::Int32 _stage_width;
		wx::Int32 _stage_height;
		bool _view_initialized;

		particles::ParticleHolder _particle_holder;
		particles::ParticleSystemDefinition _particle_definition;
		std::string _particle_revision;
		std::string _particle_system_error;
		std::string _emit_error;
		std::string _emit_status;
	};
}

#endif // !__POP_PARTICLE_VIEWER_H__
