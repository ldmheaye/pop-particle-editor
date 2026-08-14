#ifndef POP_PARTICLE_TRACK_H
#define POP_PARTICLE_TRACK_H

#include <vector>
#include <WXBase/WXDefinitions.h>

namespace pop::particles
{
	constexpr wx::Size kTodMaxTrackNodes = 10;

	enum class CurveType
	{
		Constant,
		Linear,
		EaseIn,
		EaseOut,
		EaseInOut,
		EaseInOutWeak,
		FastInOut,
		FastInOutWeak,
		WeakFastInOut,
		Bounce,
		BounceFastMiddle,
		BounceSlowMiddle,
		SinWave,
		EaseSinWave
	};

	struct FloatTrackNode
	{
		// Normalized to [0, 1]; explicit XML times use the range [0, 100].
		wx::Float32 time = 0.0f;
		wx::Float32 low_value = 0.0f;
		wx::Float32 high_value = 0.0f;
		CurveType curve = CurveType::Linear;
		CurveType distribution = CurveType::Linear;
	};

	struct FloatTrack
	{
		std::vector<FloatTrackNode> nodes;

		static FloatTrack Constant(wx::Float32 value)
		{
			return { { { 0.0f, value, value, CurveType::Constant, CurveType::Linear } } };
		}

		bool Empty() const noexcept
		{
			return nodes.empty();
		}
	};
}

#endif // !POP_PARTICLE_TRACK_H
