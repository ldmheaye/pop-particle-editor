#ifndef __POP_RESOURCE_H__
#define __POP_RESOURCE_H__

#include <filesystem>
#include <string>
#include <imgui.h>

namespace pop
{
	inline constexpr char kPopResourceDragDropPayload[] = "POP_RESOURCE";

	struct PopResource
	{
		std::string id;
		std::filesystem::path path;
		ImVec2 size;
		ImTextureRef image;
	};
}

#endif // !__POP_RESOURCE_H__
