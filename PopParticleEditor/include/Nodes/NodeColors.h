#ifndef __POP_NODE_COLORS_H__
#define __POP_NODE_COLORS_H__

#include <imgui.h>

namespace pop::node_colors
{
	// Data pins follow Unreal Blueprint's type hues. Headers use darker variants
	// so the shared light title text remains readable on the node canvas.
	namespace pin
	{
		inline constexpr ImU32 Float = IM_COL32(126, 211, 72, 255);
		inline constexpr ImU32 Integer = IM_COL32(31, 162, 208, 255);
		inline constexpr ImU32 String = IM_COL32(224, 77, 173, 255);
		inline constexpr ImU32 NodeTrack = IM_COL32(240, 181, 52, 255);
		inline constexpr ImU32 Resource = IM_COL32(82, 130, 230, 255);
		inline constexpr ImU32 ParticleField = IM_COL32(174, 111, 226, 255);
		inline constexpr ImU32 Emitter = IM_COL32(239, 104, 65, 255);
	}

	namespace header
	{
		inline constexpr ImU32 Float = IM_COL32(48, 105, 42, 255);
		inline constexpr ImU32 Integer = IM_COL32(19, 83, 116, 255);
		inline constexpr ImU32 String = IM_COL32(119, 34, 88, 255);
		inline constexpr ImU32 NodeTrack = IM_COL32(91, 58, 143, 255);
		inline constexpr ImU32 ConstantNodeTrack = IM_COL32(126, 82, 17, 255);
		inline constexpr ImU32 Resource = IM_COL32(39, 59, 125, 255);
		inline constexpr ImU32 ParticleField = IM_COL32(78, 45, 124, 255);
		inline constexpr ImU32 Emitter = IM_COL32(126, 48, 31, 255);
		inline constexpr ImU32 ParticleSystemDefinition = IM_COL32(24, 91, 79, 255);
		inline constexpr ImU32 Error = IM_COL32(138, 32, 42, 255);
	}
}

#endif // !__POP_NODE_COLORS_H__
