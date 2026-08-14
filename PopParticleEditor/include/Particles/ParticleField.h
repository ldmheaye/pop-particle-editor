#ifndef POP_PARTICLE_FIELD_H
#define POP_PARTICLE_FIELD_H

#include "ParticleTrack.h"

namespace pop::particles
{
	inline constexpr wx::Size kTodMaxParticleFields = 4;

	enum class ParticleFieldType
	{
		Invalid,
		Friction,
		Acceleration,
		Attractor,
		MaxVelocity,
		Velocity,
		Position,
		SystemPosition,
		GroundConstraint,
		Shake,
		Circle,
		Away
	};

	struct ParticleFieldDefinition
	{
		ParticleFieldType type = ParticleFieldType::Invalid;
		FloatTrack x;
		FloatTrack y;
	};
}

#endif // !POP_PARTICLE_FIELD_H
