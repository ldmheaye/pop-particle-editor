#ifndef __POP_PARTICLE_PARTICLE_HOLDER_H__
#define __POP_PARTICLE_PARTICLE_HOLDER_H__

#include "ParticleSystem.h"
#include <imgui.h>
#include <array>
#include <cstdint>
#include <random>
#include <vector>

namespace pop::particles
{
	// 顶点数据
	struct ParticleVertex
	{
		wx::Float32 x, y;
		wx::Float32 u, v;
		ImColor color;
		ImTextureRef tex;
	};

	class ParticleHolder
	{
	public:
		ParticleHolder() = default;
		~ParticleHolder() = default;

		ParticleHolder(const ParticleHolder&) = delete;
		ParticleHolder& operator=(const ParticleHolder&) = delete;

	public:
		void Emitt(const ParticleSystemDefinition& system);
		void SetSystemPosition(const ImVec2& position);

		void OnRenderParticles(
			ImDrawList* canvas,
			const ImVec2& stageOrigin,
			wx::Float32 zoom,
			const ImVec2& stageSize) const;
		void OnUpdate(wx::Float32 deltaSeconds);

		void StartEmulate(); // 开始模拟粒子(在OnUpdate刷新模拟取数据)
		void PauseEmulate(); // 停止模拟粒子
		void SetEmulateSpeed(wx::Float32 speed); // 设置模拟速度
		bool IsEmulating() const noexcept;
		wx::Float32 GetEmulateSpeed() const noexcept;
		wx::Size GetParticleCount() const noexcept;

	private:
		static constexpr wx::Float32 kSimulationStep = 0.01f;
		static constexpr wx::Size kMaxParticles = 4096;
		static constexpr wx::Size kParticleTrackCount = 16;
		static constexpr wx::Size kSystemTrackCount = 10;

		struct RuntimeParticle
		{
			wx::Int32 duration = 1;
			wx::Int32 age = 0;
			wx::Float32 time = -1.0f;
			wx::Float32 last_time = -1.0f;
			wx::Float32 animation_time = 0.0f;
			ImVec2 velocity{};
			ImVec2 position{};
			wx::Int32 image_frame = 0;
			wx::Float32 spin = 0.0f;
			wx::Float32 spin_velocity = 0.0f;
			std::array<wx::Float32, kParticleTrackCount> track_interpolation{};
			std::array<std::array<wx::Float32, 2>, kTodMaxParticleFields>
				field_interpolation{};
			std::array<ImVec2, kTodMaxParticleFields> shake_offset{};
			std::uint32_t random_seed = 0;
		};

		struct RuntimeEmitter
		{
			const ParticleEmitterDefinition* definition = nullptr;
			std::vector<RuntimeParticle> particles;
			wx::Float32 spawn_accumulator = 0.0f;
			wx::Int32 particles_spawned = 0;
			wx::Int32 age = -1;
			wx::Int32 duration = 1;
			wx::Float32 time = -1.0f;
			wx::Float32 last_time = -1.0f;
			ImVec2 center{};
			bool dead = false;
			std::array<wx::Float32, kSystemTrackCount> track_interpolation{};
			std::array<std::array<wx::Float32, 2>, kTodMaxParticleFields>
				field_interpolation{};
		};

	private:
		wx::Float32 _random_unit();
		wx::Int32 _random_int(wx::Int32 upperExclusive);
		wx::Size _particle_count() const;
		wx::Float32 _system_track(
			const RuntimeEmitter& emitter,
			const FloatTrack& track,
			wx::Size index) const;
		wx::Float32 _particle_track(
			const RuntimeParticle& particle,
			const FloatTrack& track,
			wx::Size index) const;
		void _initialize_emitter(
			RuntimeEmitter& emitter,
			const ParticleEmitterDefinition& emitterDefinition);
		void _reset(const ParticleSystemDefinition& system);
		RuntimeParticle* _spawn_particle(
			RuntimeEmitter& emitter,
			wx::Int32 index,
			wx::Int32 spawnCount);
		void _update_particle_field(
			RuntimeEmitter& emitter,
			RuntimeParticle& particle,
			const ParticleFieldDefinition& field,
			wx::Size fieldIndex);
		bool _update_particle(RuntimeEmitter& emitter, RuntimeParticle& particle);
		void _update_system_fields(RuntimeEmitter& emitter);
		void _update_spawning(RuntimeEmitter& emitter);
		void _update_emitter(RuntimeEmitter& emitter);
		void _update_tick();
		void _set_position(const ImVec2& position);
		void _render_particle(
			ImDrawList* canvas,
			const RuntimeEmitter& emitter,
			const RuntimeParticle& particle,
			const ImVec2& stageOrigin,
			wx::Float32 zoom,
			const ImVec2& stageSize) const;

	private:
		ParticleSystemDefinition _definition;
		std::vector<RuntimeEmitter> _emitters;
		std::mt19937 _random{ std::random_device{}() };
		wx::Float32 _update_accumulator = 0.0f;
		wx::Float32 _speed = 1.0f;
		bool _emulating = true;
		ImVec2 _system_position{};
	};
}

#endif // !__POP_PARTICLE_PARTICLE_HOLDER_H__
