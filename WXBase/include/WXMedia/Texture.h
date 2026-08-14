#ifndef __WX_MEDIA_H__
#define __WX_MEDIA_H__

#include <filesystem>
#include <imgui.h>
#include <optional>

namespace wx
{
	std::optional<ImTextureRef> LoadTexture(const std::filesystem::path& path, ImVec2& size);

	void DestroyTexture(ImTextureRef& textureRef);

	void DestroyTexture(ImTextureID textureId);
}

#endif // !__WX_MEDIA_H__
