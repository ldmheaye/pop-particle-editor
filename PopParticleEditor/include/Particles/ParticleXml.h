#ifndef POP_PARTICLE_XML_H
#define POP_PARTICLE_XML_H

#include "ParticleSystem.h"

#include <string>
#include <string_view>

namespace pop::particles
{
	bool SerializeParticleSystemXml(
		const ParticleSystemDefinition& definition,
		std::string& xml,
		std::string& error);
	bool DeserializeParticleSystemXml(
		std::string_view xml,
		ParticleSystemDefinition& definition,
		std::string& error);
}

#endif // !POP_PARTICLE_XML_H
