#include "../../include/Particles/ParticleHolder.h"

#include "../../include/PopState.h"

#include <glad/glad.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <random>
#include <utility>
#include <vector>

namespace
{
	using pop::particles::CurveType;
	using pop::particles::FloatTrack;
	using pop::particles::ParticleEmitterDefinition;
	using pop::particles::ParticleFieldDefinition;
	using pop::particles::ParticleFieldType;

	constexpr wx::Float32 kPi = 3.14159265358979323846f;

	void EnableAdditiveBlend(const ImDrawList*, const ImDrawCmd*)
	{
		glBlendEquation(GL_FUNC_ADD);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
	}

	enum ParticleTrackIndex : wx::Size
	{
		ParticleRed,
		ParticleGreen,
		ParticleBlue,
		ParticleAlpha,
		ParticleBrightness,
		ParticleSpinSpeed,
		ParticleSpinAngle,
		ParticleScale,
		ParticleStretch,
		ParticleCollisionReflect,
		ParticleCollisionSpin,
		ParticleClipTop,
		ParticleClipBottom,
		ParticleClipLeft,
		ParticleClipRight,
		ParticleAnimationRate
	};

	enum SystemTrackIndex : wx::Size
	{
		SpawnRate,
		SpawnMinActive,
		SpawnMaxActive,
		SpawnMaxLaunched,
		EmitterPath,
		SystemRed,
		SystemGreen,
		SystemBlue,
		SystemAlpha,
		SystemBrightness
	};

	wx::Float32 Clamp01(wx::Float32 value)
	{
		return std::clamp(value, 0.0f, 1.0f);
	}

	wx::Float32 CurveS(wx::Float32 time)
	{
		return 3.0f * time * time - 2.0f * time * time * time;
	}

	wx::Float32 CurveInvQuad(wx::Float32 time)
	{
		return 2.0f * time - time * time;
	}

	wx::Float32 CurveInvQuadS(wx::Float32 time)
	{
		if (time <= 0.5f)
			return CurveInvQuad(time * 2.0f) * 0.5f;
		const wx::Float32 adjusted = (time - 0.5f) * 2.0f;
		return adjusted * adjusted * 0.5f + 0.5f;
	}

	wx::Float32 WarpCurve(wx::Float32 time, CurveType curve)
	{
		switch (curve)
		{
		case CurveType::Constant: return 0.0f;
		case CurveType::Linear: return time;
		case CurveType::EaseIn: return time * time;
		case CurveType::EaseOut: return CurveInvQuad(time);
		case CurveType::EaseInOut: return CurveS(CurveS(time));
		case CurveType::EaseInOutWeak: return CurveS(time);
		case CurveType::FastInOut: return CurveInvQuadS(CurveInvQuadS(time));
		case CurveType::FastInOutWeak: return CurveInvQuadS(time);
		case CurveType::WeakFastInOut:
			if (time <= 0.5f)
				return time * time * 2.0f;
			return CurveInvQuad((time - 0.5f) * 2.0f) * 0.5f + 0.5f;
		case CurveType::Bounce: return 1.0f - std::fabs(2.0f * time - 1.0f);
		case CurveType::BounceFastMiddle:
		{
			const wx::Float32 bounce = 1.0f - std::fabs(2.0f * time - 1.0f);
			return bounce * bounce;
		}
		case CurveType::BounceSlowMiddle:
			return CurveInvQuad(1.0f - std::fabs(2.0f * time - 1.0f));
		case CurveType::SinWave: return std::sin(2.0f * kPi * time);
		case CurveType::EaseSinWave: return std::sin(2.0f * kPi * CurveS(time));
		default: return time;
		}
	}

	wx::Float32 CurveEvaluate(
		wx::Float32 time,
		wx::Float32 from,
		wx::Float32 to,
		CurveType curve)
	{
		return from + (to - from) * WarpCurve(time, curve);
	}

	wx::Float32 EvaluateTrack(
		const FloatTrack& track,
		wx::Float32 time,
		wx::Float32 interpolation)
	{
		if (track.nodes.empty())
			return 0.0f;

		const auto evaluateNode = [interpolation](const auto& node)
		{
			return CurveEvaluate(
				interpolation,
				node.low_value,
				node.high_value,
				node.distribution);
		};

		if (time < track.nodes.front().time)
			return evaluateNode(track.nodes.front());

		for (wx::Size index = 1; index < track.nodes.size(); ++index)
		{
			const auto& next = track.nodes[index];
			if (time <= next.time)
			{
				const auto& current = track.nodes[index - 1];
				const wx::Float32 duration = next.time - current.time;
				const wx::Float32 fraction = duration > 0.0f
					? (time - current.time) / duration
					: 1.0f;
				return CurveEvaluate(
					fraction,
					evaluateNode(current),
					evaluateNode(next),
					current.curve);
			}
		}
		return evaluateNode(track.nodes.back());
	}

	wx::Float32 EvaluateFromLastTime(
		const FloatTrack& track,
		wx::Float32 time,
		wx::Float32 interpolation)
	{
		return time < 0.0f ? 0.0f : EvaluateTrack(track, time, interpolation);
	}

	bool IsTrackSet(const FloatTrack& track)
	{
		return !track.nodes.empty() &&
			track.nodes.front().curve != CurveType::Constant;
	}

	bool IsConstantZero(const FloatTrack& track)
	{
		return track.nodes.empty() ||
			(track.nodes.size() == 1 &&
				track.nodes.front().low_value == 0.0f &&
				track.nodes.front().high_value == 0.0f);
	}

	ImVec2 Normalize(const ImVec2& value)
	{
		const wx::Float32 magnitude = std::sqrt(value.x * value.x + value.y * value.y);
		if (magnitude <= std::numeric_limits<wx::Float32>::epsilon())
			return {};
		return { value.x / magnitude, value.y / magnitude };
	}

	wx::Float32 Magnitude(const ImVec2& value)
	{
		return std::sqrt(value.x * value.x + value.y * value.y);
	}

	ImVec2 TransformPoint(
		wx::Float32 x,
		wx::Float32 y,
		const ImVec2& center,
		wx::Float32 radians,
		wx::Float32 scaleX,
		wx::Float32 scaleY)
	{
		const wx::Float32 cosine = std::cos(radians);
		const wx::Float32 sine = std::sin(radians);
		return {
			center.x + cosine * scaleX * x + sine * scaleY * y,
			center.y - sine * scaleX * x + cosine * scaleY * y
		};
	}
}

namespace pop::particles
{
	wx::Float32 ParticleHolder::_random_unit()
	{
		return std::uniform_real_distribution<wx::Float32>(0.0f, 1.0f)(_random);
	}

	wx::Int32 ParticleHolder::_random_int(wx::Int32 upperExclusive)
	{
		if (upperExclusive <= 1)
			return 0;
		return std::uniform_int_distribution<wx::Int32>(0, upperExclusive - 1)(_random);
	}

	wx::Size ParticleHolder::_particle_count() const
	{
		wx::Size count = 0;
		for (const RuntimeEmitter& emitter : _emitters)
			count += emitter.particles.size();
		return count;
	}

	wx::Float32 ParticleHolder::_system_track(
		const RuntimeEmitter& emitter,
		const FloatTrack& track,
		wx::Size index) const
	{
		return EvaluateTrack(track, emitter.time, emitter.track_interpolation[index]);
	}

	wx::Float32 ParticleHolder::_particle_track(
		const RuntimeParticle& particle,
		const FloatTrack& track,
		wx::Size index) const
	{
		return EvaluateTrack(track, particle.time, particle.track_interpolation[index]);
	}

	void ParticleHolder::_initialize_emitter(
		RuntimeEmitter& emitter,
		const ParticleEmitterDefinition& emitterDefinition)
	{
		emitter = {};
		emitter.definition = &emitterDefinition;
		emitter.center = _system_position;
		emitter.age = -1;
		emitter.time = -1.0f;
		emitter.last_time = -1.0f;
		emitter.duration = std::max(
			1,
			static_cast<wx::Int32>(EvaluateTrack(
				IsTrackSet(emitterDefinition.system_duration)
					? emitterDefinition.system_duration
					: emitterDefinition.particle_duration,
				0.0f,
				IsTrackSet(emitterDefinition.system_duration) ? _random_unit() : 1.0f)));
		for (wx::Float32& interpolation : emitter.track_interpolation)
			interpolation = _random_unit();
		for (auto& field : emitter.field_interpolation)
		{
			field[0] = _random_unit();
			field[1] = _random_unit();
		}
	}

	void ParticleHolder::_reset(const ParticleSystemDefinition& system)
	{
		_definition = system;
		_emitters.clear();
		_emitters.reserve(_definition.emitters.size());
		for (const ParticleEmitterDefinition& emitterDefinition : _definition.emitters)
		{
			if (IsTrackSet(emitterDefinition.cross_fade_duration))
				continue;
			_emitters.emplace_back();
			_initialize_emitter(_emitters.back(), emitterDefinition);
		}
		_update_accumulator = 0.0f;
		_update_tick();
	}

	ParticleHolder::RuntimeParticle* ParticleHolder::_spawn_particle(
		RuntimeEmitter& emitter,
		wx::Int32 index,
		wx::Int32 spawnCount)
	{
		if (_particle_count() >= kMaxParticles || emitter.definition == nullptr)
			return nullptr;

		const ParticleEmitterDefinition& emitterDefinition = *emitter.definition;
		emitter.particles.emplace_back();
		RuntimeParticle& particle = emitter.particles.back();
		for (wx::Float32& interpolation : particle.track_interpolation)
			interpolation = _random_unit();
		for (auto& field : particle.field_interpolation)
		{
			field[0] = _random_unit();
			field[1] = _random_unit();
		}
		particle.random_seed = _random();
		particle.duration = std::max(
			1,
			static_cast<wx::Int32>(EvaluateTrack(
				emitterDefinition.particle_duration,
				emitter.time,
				_random_unit())));
		if (HasFlag(emitterDefinition.flags, RandomStartTime))
			particle.age = _random_int(particle.duration);

		const wx::Float32 launchSpeed = EvaluateTrack(
			emitterDefinition.launch_speed,
			emitter.time,
			_random_unit()) * 0.01f;
		wx::Float32 launchAngle = 0.0f;
		if (emitterDefinition.emitter_type == EmitterType::CirclePath)
		{
				launchAngle = _system_track(emitter, emitterDefinition.emitter_path, EmitterPath) *
					2.0f * kPi;
				launchAngle += EvaluateTrack(
					emitterDefinition.launch_angle,
					emitter.time,
					_random_unit()) * kPi / 180.0f;
			}
			else if (emitterDefinition.emitter_type == EmitterType::CircleEvenSpacing)
			{
				launchAngle = 2.0f * kPi * static_cast<wx::Float32>(index) /
					static_cast<wx::Float32>(std::max(spawnCount, 1));
				launchAngle += EvaluateTrack(
					emitterDefinition.launch_angle,
					emitter.time,
					_random_unit()) * kPi / 180.0f;
			}
			else if (IsConstantZero(emitterDefinition.launch_angle))
			{
				launchAngle = _random_unit() * 2.0f * kPi;
			}
			else
			{
				launchAngle = EvaluateTrack(
					emitterDefinition.launch_angle,
					emitter.time,
					_random_unit()) * kPi / 180.0f;
			}

			wx::Float32 positionX = 0.0f;
			wx::Float32 positionY = 0.0f;
			switch (emitterDefinition.emitter_type)
			{
			case EmitterType::Circle:
			case EmitterType::CirclePath:
			case EmitterType::CircleEvenSpacing:
			{
				const wx::Float32 radius = EvaluateTrack(
					emitterDefinition.emitter_radius,
					emitter.time,
					_random_unit());
				positionX = std::sin(launchAngle) * radius;
				positionY = std::cos(launchAngle) * radius;
				break;
			}
			case EmitterType::Box:
				positionX = EvaluateTrack(emitterDefinition.emitter_box_x, emitter.time, _random_unit());
				positionY = EvaluateTrack(emitterDefinition.emitter_box_y, emitter.time, _random_unit());
				break;
			case EmitterType::BoxPath:
			{
				const wx::Float32 path = _system_track(emitter, emitterDefinition.emitter_path, EmitterPath);
				const wx::Float32 minX = EvaluateTrack(emitterDefinition.emitter_box_x, emitter.time, 0.0f);
				const wx::Float32 maxX = EvaluateTrack(emitterDefinition.emitter_box_x, emitter.time, 1.0f);
				const wx::Float32 minY = EvaluateTrack(emitterDefinition.emitter_box_y, emitter.time, 0.0f);
				const wx::Float32 maxY = EvaluateTrack(emitterDefinition.emitter_box_y, emitter.time, 1.0f);
				const wx::Float32 width = std::max(0.0f, maxX - minX);
				const wx::Float32 height = std::max(0.0f, maxY - minY);
				const wx::Float32 perimeter = 2.0f * (width + height);
				wx::Float32 pathPosition = std::fmod(path, 1.0f);
				if (pathPosition < 0.0f)
					pathPosition += 1.0f;
				pathPosition *= perimeter;
				if (pathPosition < height)
				{
					positionX = minX;
					positionY = minY + pathPosition;
				}
				else if (pathPosition < height + width)
				{
					positionX = minX + pathPosition - height;
					positionY = maxY;
				}
				else if (pathPosition < 2.0f * height + width)
				{
					positionX = maxX;
					positionY = maxY - (pathPosition - height - width);
				}
				else
				{
					positionX = maxX - (pathPosition - 2.0f * height - width);
					positionY = minY;
				}
				break;
			}
			}

			const wx::Float32 skewX = EvaluateTrack(
				emitterDefinition.emitter_skew_x, emitter.time, _random_unit());
			const wx::Float32 skewY = EvaluateTrack(
				emitterDefinition.emitter_skew_y, emitter.time, _random_unit());
			particle.position.x = emitter.center.x + positionX + positionY * skewX;
			particle.position.y = emitter.center.y + positionY + positionX * skewY;
			particle.position.x += EvaluateTrack(
				emitterDefinition.emitter_offset_x, emitter.time, _random_unit());
			particle.position.y += EvaluateTrack(
				emitterDefinition.emitter_offset_y, emitter.time, _random_unit());
			particle.velocity = {
				std::sin(launchAngle) * launchSpeed,
				std::cos(launchAngle) * launchSpeed
			};

			const wx::Int32 imageFrames = std::max(emitterDefinition.image_frames, 1);
			particle.image_frame =
				(emitterDefinition.animated || IsTrackSet(emitterDefinition.animation_rate))
				? 0
				: _random_int(imageFrames);
			if (HasFlag(emitterDefinition.flags, RandomLaunchSpin))
				particle.spin = _random_unit() * 2.0f * kPi;
			else if (HasFlag(emitterDefinition.flags, AlignLaunchSpin))
				particle.spin = launchAngle;

		++emitter.particles_spawned;
		return &particle;
	}

	void ParticleHolder::_update_particle_field(
		RuntimeEmitter& emitter,
		RuntimeParticle& particle,
		const ParticleFieldDefinition& field,
		wx::Size fieldIndex)
		{
			const wx::Float32 interpolationX = particle.field_interpolation[fieldIndex][0];
			const wx::Float32 interpolationY = particle.field_interpolation[fieldIndex][1];
			const wx::Float32 x = EvaluateTrack(field.x, particle.time, interpolationX);
			const wx::Float32 y = EvaluateTrack(field.y, particle.time, interpolationY);
			switch (field.type)
			{
			case ParticleFieldType::Friction:
				particle.velocity.x *= 1.0f - x;
				particle.velocity.y *= 1.0f - y;
				break;
			case ParticleFieldType::Acceleration:
				particle.velocity.x += 0.01f * x;
				particle.velocity.y += 0.01f * y;
				break;
			case ParticleFieldType::Attractor:
				particle.velocity.x += 0.01f * (x - (particle.position.x - emitter.center.x));
				particle.velocity.y += 0.01f * (y - (particle.position.y - emitter.center.y));
				break;
			case ParticleFieldType::MaxVelocity:
				particle.velocity.x = std::clamp(particle.velocity.x, -std::fabs(x), std::fabs(x));
				particle.velocity.y = std::clamp(particle.velocity.y, -std::fabs(y), std::fabs(y));
				break;
			case ParticleFieldType::Velocity:
				particle.position.x += 0.01f * x;
				particle.position.y += 0.01f * y;
				break;
			case ParticleFieldType::Position:
				particle.position.x += x - EvaluateFromLastTime(field.x, particle.last_time, interpolationX);
				particle.position.y += y - EvaluateFromLastTime(field.y, particle.last_time, interpolationY);
				break;
			case ParticleFieldType::GroundConstraint:
				if (particle.position.y > emitter.center.y + y)
				{
					particle.position.y = emitter.center.y + y;
					const wx::Float32 reflect = _particle_track(
						particle, emitter.definition->collision_reflect, ParticleCollisionReflect);
					const wx::Float32 collisionSpin = _particle_track(
						particle, emitter.definition->collision_spin, ParticleCollisionSpin) / 1000.0f;
					particle.spin_velocity = particle.velocity.y * collisionSpin;
					particle.velocity.x *= reflect;
					particle.velocity.y *= -reflect;
				}
				break;
			case ParticleFieldType::Shake:
			{
				particle.position.x -= particle.shake_offset[fieldIndex].x;
				particle.position.y -= particle.shake_offset[fieldIndex].y;
				std::seed_seq seed{
					particle.random_seed,
					static_cast<std::uint32_t>(particle.age),
					static_cast<std::uint32_t>(fieldIndex)
				};
				std::mt19937 shakeRandom(seed);
				std::uniform_real_distribution<wx::Float32> distribution(-1.0f, 1.0f);
				particle.shake_offset[fieldIndex] = {
					x * distribution(shakeRandom),
					y * distribution(shakeRandom)
				};
				particle.position.x += particle.shake_offset[fieldIndex].x;
				particle.position.y += particle.shake_offset[fieldIndex].y;
				break;
			}
			case ParticleFieldType::Circle:
			case ParticleFieldType::Away:
			{
				const ImVec2 relative{
					particle.position.x - emitter.center.x,
					particle.position.y - emitter.center.y
				};
				const wx::Float32 magnitude = Magnitude(relative);
				ImVec2 direction = field.type == ParticleFieldType::Circle
					? Normalize({ -relative.y, relative.x })
					: Normalize(relative);
				const wx::Float32 distance = 0.01f * (x + magnitude * y);
				particle.position.x += direction.x * distance;
				particle.position.y += direction.y * distance;
				break;
			}
			default:
				break;
			}
		}

	bool ParticleHolder::_update_particle(RuntimeEmitter& emitter, RuntimeParticle& particle)
	{
			const ParticleEmitterDefinition& emitterDefinition = *emitter.definition;
			if (particle.age >= particle.duration)
			{
				if (HasFlag(emitterDefinition.flags, ParticleLoops))
					particle.age = 0;
				else
					return false;
			}

			particle.time = particle.duration > 1
				? static_cast<wx::Float32>(particle.age) /
					static_cast<wx::Float32>(particle.duration - 1)
				: 1.0f;
			const wx::Size fieldCount = std::min(
				emitterDefinition.particle_fields.size(),
				static_cast<wx::Size>(kTodMaxParticleFields));
			for (wx::Size index = 0; index < fieldCount; ++index)
				_update_particle_field(emitter, particle, emitterDefinition.particle_fields[index], index);

			particle.position.x += particle.velocity.x;
			particle.position.y += particle.velocity.y;
			const wx::Float32 spinSpeed = _particle_track(
				particle, emitterDefinition.particle_spin_speed, ParticleSpinSpeed) * 0.01f;
			const wx::Float32 spinAngle = _particle_track(
				particle, emitterDefinition.particle_spin_angle, ParticleSpinAngle);
			const wx::Float32 lastSpinAngle = EvaluateFromLastTime(
				emitterDefinition.particle_spin_angle,
				particle.last_time,
				particle.track_interpolation[ParticleSpinAngle]);
			particle.spin += (spinSpeed + spinAngle - lastSpinAngle) * kPi / 180.0f +
				particle.spin_velocity;

			if (IsTrackSet(emitterDefinition.animation_rate))
			{
				particle.animation_time += _particle_track(
					particle, emitterDefinition.animation_rate, ParticleAnimationRate) * 0.01f;
				particle.animation_time -= std::floor(particle.animation_time);
			}
			++particle.age;
			particle.last_time = particle.time;
			return true;
	}

	void ParticleHolder::_update_system_fields(RuntimeEmitter& emitter)
	{
			const auto& fields = emitter.definition->system_fields;
			const wx::Size fieldCount = std::min(
				fields.size(), static_cast<wx::Size>(kTodMaxParticleFields));
			for (wx::Size index = 0; index < fieldCount; ++index)
			{
				const ParticleFieldDefinition& field = fields[index];
				if (field.type != ParticleFieldType::SystemPosition)
					continue;
				const wx::Float32 interpolationX = emitter.field_interpolation[index][0];
				const wx::Float32 interpolationY = emitter.field_interpolation[index][1];
				const wx::Float32 x = EvaluateTrack(field.x, emitter.time, interpolationX);
				const wx::Float32 y = EvaluateTrack(field.y, emitter.time, interpolationY);
				emitter.center.x += x - EvaluateFromLastTime(field.x, emitter.last_time, interpolationX);
				emitter.center.y += y - EvaluateFromLastTime(field.y, emitter.last_time, interpolationY);
			}
	}

	void ParticleHolder::_update_spawning(RuntimeEmitter& emitter)
	{
			const ParticleEmitterDefinition& emitterDefinition = *emitter.definition;
			emitter.spawn_accumulator += _system_track(emitter, emitterDefinition.spawn_rate, SpawnRate) * 0.01f;
			wx::Int32 spawnCount = std::max(0, static_cast<wx::Int32>(emitter.spawn_accumulator));
			emitter.spawn_accumulator -= static_cast<wx::Float32>(spawnCount);

			const wx::Int32 minActive = static_cast<wx::Int32>(_system_track(
				emitter, emitterDefinition.spawn_min_active, SpawnMinActive));
			if (minActive >= 0)
				spawnCount = std::max(
					spawnCount,
					minActive - static_cast<wx::Int32>(emitter.particles.size()));

			const wx::Int32 maxActive = static_cast<wx::Int32>(_system_track(
				emitter, emitterDefinition.spawn_max_active, SpawnMaxActive));
			if (maxActive >= 0)
				spawnCount = std::min(
					spawnCount,
					maxActive - static_cast<wx::Int32>(emitter.particles.size()));

			if (IsTrackSet(emitterDefinition.spawn_max_launched))
			{
				const wx::Int32 maxLaunched = static_cast<wx::Int32>(_system_track(
					emitter, emitterDefinition.spawn_max_launched, SpawnMaxLaunched));
				spawnCount = std::min(spawnCount, maxLaunched - emitter.particles_spawned);
			}
			spawnCount = std::clamp(spawnCount, 0, static_cast<wx::Int32>(kMaxParticles));
			for (wx::Int32 index = 0; index < spawnCount; ++index)
				if (_spawn_particle(emitter, index, spawnCount) == nullptr)
					break;
	}

	void ParticleHolder::_update_emitter(RuntimeEmitter& emitter)
	{
			if (emitter.dead || emitter.definition == nullptr)
				return;

			++emitter.age;
			bool shouldDie = false;
			if (emitter.age >= emitter.duration)
			{
				if (HasFlag(emitter.definition->flags, SystemLoops))
					emitter.age = 0;
				else
				{
					emitter.age = emitter.duration - 1;
					shouldDie = true;
				}
			}
			emitter.time = emitter.duration > 1
				? static_cast<wx::Float32>(emitter.age) /
					static_cast<wx::Float32>(emitter.duration - 1)
				: 1.0f;
			_update_system_fields(emitter);

			auto particle = emitter.particles.begin();
			while (particle != emitter.particles.end())
			{
				if (_update_particle(emitter, *particle))
					++particle;
				else
					particle = emitter.particles.erase(particle);
			}
			_update_spawning(emitter);

			if (shouldDie)
			{
				emitter.particles.clear();
				emitter.dead = true;
			}
			emitter.last_time = emitter.time;
	}

	void ParticleHolder::_update_tick()
	{
		for (RuntimeEmitter& emitter : _emitters)
			_update_emitter(emitter);
	}

	void ParticleHolder::_set_position(const ImVec2& position)
	{
			const ImVec2 delta{
				position.x - _system_position.x,
				position.y - _system_position.y
			};
			if (delta.x == 0.0f && delta.y == 0.0f)
				return;
			_system_position = position;
			for (RuntimeEmitter& emitter : _emitters)
			{
				emitter.center.x += delta.x;
				emitter.center.y += delta.y;
				if (emitter.definition == nullptr ||
					HasFlag(emitter.definition->flags, ParticlesDontFollow))
				{
					continue;
				}
				for (RuntimeParticle& particle : emitter.particles)
				{
					particle.position.x += delta.x;
					particle.position.y += delta.y;
				}
			}
	}

	void ParticleHolder::_render_particle(
		ImDrawList* canvas,
		const RuntimeEmitter& emitter,
		const RuntimeParticle& particle,
		const ImVec2& stageOrigin,
		wx::Float32 zoom,
		const ImVec2& stageSize) const
	{
			const ParticleEmitterDefinition& emitterDefinition = *emitter.definition;
			if (HasFlag(emitterDefinition.flags, SoftwareOnly))
				return;
			const wx::Float32 systemBrightness = _system_track(
				emitter, emitterDefinition.system_brightness, SystemBrightness);
			const wx::Float32 particleBrightness = _particle_track(
				particle, emitterDefinition.particle_brightness, ParticleBrightness);
			const wx::Float32 brightness = systemBrightness * particleBrightness;
			const wx::Float32 red = _particle_track(
				particle, emitterDefinition.particle_red, ParticleRed) *
				_system_track(emitter, emitterDefinition.system_red, SystemRed) * brightness;
			const wx::Float32 green = _particle_track(
				particle, emitterDefinition.particle_green, ParticleGreen) *
				_system_track(emitter, emitterDefinition.system_green, SystemGreen) * brightness;
			const wx::Float32 blue = _particle_track(
				particle, emitterDefinition.particle_blue, ParticleBlue) *
				_system_track(emitter, emitterDefinition.system_blue, SystemBlue) * brightness;
			const wx::Float32 alpha = _particle_track(
				particle, emitterDefinition.particle_alpha, ParticleAlpha) *
				_system_track(emitter, emitterDefinition.system_alpha, SystemAlpha) * brightness;
			const ImU32 color = IM_COL32(
				static_cast<wx::Int32>(Clamp01(red) * 255.0f + 0.5f),
				static_cast<wx::Int32>(Clamp01(green) * 255.0f + 0.5f),
				static_cast<wx::Int32>(Clamp01(blue) * 255.0f + 0.5f),
				static_cast<wx::Int32>(Clamp01(alpha) * 255.0f + 0.5f));
			if ((color & IM_COL32_A_MASK) == 0)
				return;

			if (HasFlag(emitterDefinition.flags, FullScreen))
			{
				const bool additive = HasFlag(emitterDefinition.flags, Additive);
				if (additive)
					canvas->AddCallback(EnableAdditiveBlend);
				canvas->AddRectFilled(
					stageOrigin,
					{ stageOrigin.x + stageSize.x * zoom, stageOrigin.y + stageSize.y * zoom },
					color);
				if (additive)
					canvas->AddCallback(ImGui::GetPlatformIO().DrawCallback_ResetRenderState);
				return;
			}

			const auto resource = gPopResources.find(emitterDefinition.image);
			if (resource == gPopResources.end())
				return;

			const wx::Int32 columns = std::max(emitterDefinition.image_cols, 1);
			const wx::Int32 rows = std::max(emitterDefinition.image_rows, 1);
			const wx::Int32 frameCount = std::max(emitterDefinition.image_frames, 1);
			wx::Int32 frame = particle.image_frame;
			if (IsTrackSet(emitterDefinition.animation_rate))
				frame = std::clamp(
					static_cast<wx::Int32>(particle.animation_time * frameCount),
					0,
					frameCount - 1);
			else if (emitterDefinition.animated)
				frame = std::clamp(
					static_cast<wx::Int32>(particle.time * frameCount),
					0,
					frameCount - 1);
			const wx::Int32 column = std::clamp(emitterDefinition.image_col + frame, 0, columns - 1);
			const wx::Int32 row = std::clamp(emitterDefinition.image_row, 0, rows - 1);

			wx::Float32 clipTop = Clamp01(_particle_track(
				particle, emitterDefinition.clip_top, ParticleClipTop));
			wx::Float32 clipBottom = Clamp01(_particle_track(
				particle, emitterDefinition.clip_bottom, ParticleClipBottom));
			wx::Float32 clipLeft = Clamp01(_particle_track(
				particle, emitterDefinition.clip_left, ParticleClipLeft));
			wx::Float32 clipRight = Clamp01(_particle_track(
				particle, emitterDefinition.clip_right, ParticleClipRight));
			if (clipLeft + clipRight >= 1.0f || clipTop + clipBottom >= 1.0f)
				return;

			const wx::Float32 cellWidth = resource->second.size.x / columns;
			const wx::Float32 cellHeight = resource->second.size.y / rows;
			const wx::Float32 width = cellWidth * (1.0f - clipLeft - clipRight);
			const wx::Float32 height = cellHeight * (1.0f - clipTop - clipBottom);
			ImVec2 center{
				stageOrigin.x + (particle.position.x + clipLeft * cellWidth) * zoom,
				stageOrigin.y + (particle.position.y + clipTop * cellHeight) * zoom
			};
			if (HasFlag(emitterDefinition.flags, AlignToPixel))
			{
				center.x = std::round(center.x);
				center.y = std::round(center.y);
			}
			const wx::Float32 scale = _particle_track(
				particle, emitterDefinition.particle_scale, ParticleScale) * zoom;
			const wx::Float32 stretch = _particle_track(
				particle, emitterDefinition.particle_stretch, ParticleStretch);
			const wx::Float32 scaleX = scale;
			const wx::Float32 scaleY = scale * stretch;
			const ImVec2 p1 = TransformPoint(-width * 0.5f, -height * 0.5f, center, particle.spin, scaleX, scaleY);
			const ImVec2 p2 = TransformPoint(width * 0.5f, -height * 0.5f, center, particle.spin, scaleX, scaleY);
			const ImVec2 p3 = TransformPoint(width * 0.5f, height * 0.5f, center, particle.spin, scaleX, scaleY);
			const ImVec2 p4 = TransformPoint(-width * 0.5f, height * 0.5f, center, particle.spin, scaleX, scaleY);
			const ImVec2 uv1{
				(static_cast<wx::Float32>(column) + clipLeft) / columns,
				(static_cast<wx::Float32>(row) + clipTop) / rows
			};
			const ImVec2 uv2{
				(static_cast<wx::Float32>(column + 1) - clipRight) / columns,
				uv1.y
			};
			const ImVec2 uv3{ uv2.x, (static_cast<wx::Float32>(row + 1) - clipBottom) / rows };
			const ImVec2 uv4{ uv1.x, uv3.y };
			const bool additive = HasFlag(emitterDefinition.flags, Additive);
			if (additive)
				canvas->AddCallback(EnableAdditiveBlend);
			canvas->AddImageQuad(resource->second.image, p1, p2, p3, p4, uv1, uv2, uv3, uv4, color);
			if (additive)
				canvas->AddCallback(ImGui::GetPlatformIO().DrawCallback_ResetRenderState);
	}

	void ParticleHolder::Emitt(const ParticleSystemDefinition& system)
	{
		_reset(system);
	}

	void ParticleHolder::SetSystemPosition(const ImVec2& position)
	{
		_set_position(position);
	}

	void ParticleHolder::OnRenderParticles(
		ImDrawList* canvas,
		const ImVec2& stageOrigin,
		wx::Float32 zoom,
		const ImVec2& stageSize) const
	{
		if (canvas == nullptr || zoom <= 0.0f)
			return;
		for (const RuntimeEmitter& emitter : _emitters)
		{
			if (emitter.definition == nullptr)
				continue;
			for (const RuntimeParticle& particle : emitter.particles)
				_render_particle(
					canvas, emitter, particle, stageOrigin, zoom, stageSize);
		}
	}

	void ParticleHolder::OnUpdate(wx::Float32 deltaSeconds)
	{
		if (!_emulating || deltaSeconds <= 0.0f)
			return;
		_update_accumulator += std::min(deltaSeconds, 0.25f) * _speed;
		wx::Int32 updateCount = 0;
		while (_update_accumulator >= kSimulationStep && updateCount < 100)
		{
			_update_tick();
			_update_accumulator -= kSimulationStep;
			++updateCount;
		}
	}

	void ParticleHolder::StartEmulate()
	{
		_emulating = true;
	}

	void ParticleHolder::PauseEmulate()
	{
		_emulating = false;
	}

	void ParticleHolder::SetEmulateSpeed(wx::Float32 speed)
	{
		_speed = std::clamp(speed, 0.05f, 8.0f);
	}

	bool ParticleHolder::IsEmulating() const noexcept
	{
		return _emulating;
	}

	wx::Float32 ParticleHolder::GetEmulateSpeed() const noexcept
	{
		return _speed;
	}

	wx::Size ParticleHolder::GetParticleCount() const noexcept
	{
		return _particle_count();
	}
}
