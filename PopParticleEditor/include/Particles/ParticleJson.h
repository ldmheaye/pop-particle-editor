#ifndef POP_PARTICLE_JSON_H
#define POP_PARTICLE_JSON_H

#include "ParticleSystem.h"

#include <nlohmann/json.hpp>

namespace pop::particles
{
	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
		FloatTrackNode,
		time,
		low_value,
		high_value,
		curve,
		distribution)

	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(FloatTrack, nodes)

	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
		ParticleFieldDefinition,
		type,
		x,
		y)

	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
		ParticleEmitterDefinition,
		image,
		image_col,
		image_row,
		image_frames,
		animated,
		flags,
		emitter_type,
		name,
		on_duration,
		system_duration,
		cross_fade_duration,
		spawn_rate,
		spawn_min_active,
		spawn_max_active,
		spawn_max_launched,
		emitter_radius,
		emitter_offset_x,
		emitter_offset_y,
		emitter_box_x,
		emitter_box_y,
		emitter_skew_x,
		emitter_skew_y,
		emitter_path,
		particle_duration,
		launch_speed,
		launch_angle,
		system_red,
		system_green,
		system_blue,
		system_alpha,
		system_brightness,
		particle_fields,
		system_fields,
		particle_red,
		particle_green,
		particle_blue,
		particle_alpha,
		particle_brightness,
		particle_spin_angle,
		particle_spin_speed,
		particle_scale,
		particle_stretch,
		collision_reflect,
		collision_spin,
		clip_top,
		clip_bottom,
		clip_left,
		clip_right,
		animation_rate)

	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
		ParticleSystemDefinition,
		emitters)
}

#endif // !POP_PARTICLE_JSON_H
