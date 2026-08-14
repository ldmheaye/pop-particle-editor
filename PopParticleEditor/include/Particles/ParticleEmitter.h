#ifndef POP_PARTICLE_EMITTER_H
#define POP_PARTICLE_EMITTER_H

#include "ParticleField.h"

#include <string>
#include <type_traits>
#include <vector>

namespace pop::particles
{
	enum class EmitterType
	{
		Circle,
		Box,
		BoxPath,
		CirclePath,
		CircleEvenSpacing
	};

	enum ParticleFlags : wx::Uint32
	{
		None = 0,
		RandomLaunchSpin = 1u << 0,
		AlignLaunchSpin = 1u << 1,
		AlignToPixel = 1u << 2,
		SystemLoops = 1u << 3,
		ParticleLoops = 1u << 4,
		ParticlesDontFollow = 1u << 5,
		RandomStartTime = 1u << 6,
		DieIfOverloaded = 1u << 7,
		Additive = 1u << 8,
		FullScreen = 1u << 9,
		SoftwareOnly = 1u << 10,
		HardwareOnly = 1u << 11
	};

	constexpr bool HasFlag(ParticleFlags value, ParticleFlags flag) noexcept
	{
		return (value & flag) == flag;
	}

	struct ParticleEmitterDefinition
	{
		// The XML Image value is a resource name, not a runtime texture.

		// 资源方面的
		std::string image;
		wx::Int32 image_col = 0;
		wx::Int32 image_row = 0;
		wx::Int32 image_frames = 1;
		wx::Int32 image_cols = 1;
		wx::Int32 image_rows = 1;

		bool animated = false;
		ParticleFlags flags = ParticleFlags::None;
		EmitterType emitter_type = EmitterType::Box;
		std::string name;
		std::string on_duration;

		FloatTrack system_duration;
		FloatTrack cross_fade_duration;
		FloatTrack spawn_rate;
		FloatTrack spawn_min_active = FloatTrack::Constant(-1.0f);
		FloatTrack spawn_max_active = FloatTrack::Constant(-1.0f);
		FloatTrack spawn_max_launched = FloatTrack::Constant(-1.0f);

		FloatTrack emitter_radius;
		FloatTrack emitter_offset_x;
		FloatTrack emitter_offset_y;
		FloatTrack emitter_box_x;
		FloatTrack emitter_box_y;
		FloatTrack emitter_skew_x;
		FloatTrack emitter_skew_y;
		FloatTrack emitter_path;

		FloatTrack particle_duration = FloatTrack::Constant(100.0f);
		FloatTrack launch_speed;
		FloatTrack launch_angle;

		FloatTrack system_red = FloatTrack::Constant(1.0f);
		FloatTrack system_green = FloatTrack::Constant(1.0f);
		FloatTrack system_blue = FloatTrack::Constant(1.0f);
		FloatTrack system_alpha = FloatTrack::Constant(1.0f);
		FloatTrack system_brightness = FloatTrack::Constant(1.0f);

		std::vector<ParticleFieldDefinition> particle_fields;
		std::vector<ParticleFieldDefinition> system_fields;

		FloatTrack particle_red = FloatTrack::Constant(1.0f);
		FloatTrack particle_green = FloatTrack::Constant(1.0f);
		FloatTrack particle_blue = FloatTrack::Constant(1.0f);
		FloatTrack particle_alpha = FloatTrack::Constant(1.0f);
		FloatTrack particle_brightness = FloatTrack::Constant(1.0f);
		FloatTrack particle_spin_angle;
		FloatTrack particle_spin_speed;
		FloatTrack particle_scale = FloatTrack::Constant(1.0f);
		FloatTrack particle_stretch = FloatTrack::Constant(1.0f);
		FloatTrack collision_reflect;
		FloatTrack collision_spin;
		FloatTrack clip_top;
		FloatTrack clip_bottom;
		FloatTrack clip_left;
		FloatTrack clip_right;
		FloatTrack animation_rate;
	};
}

#endif // !POP_PARTICLE_EMITTER_H
