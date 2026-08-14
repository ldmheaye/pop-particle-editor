#ifndef POP_PARTICLE_SYSTEM_H
#define POP_PARTICLE_SYSTEM_H

#include "ParticleEmitter.h"

#include <vector>

namespace pop::particles
{
	// Root model of a TodLib particle XML document.
	struct ParticleSystemDefinition
	{
		std::vector<ParticleEmitterDefinition> emitters;
	};
}

#endif // !POP_PARTICLE_SYSTEM_H
