#include "../../include/WXMedia/Texture.h"
#include "../../include/WXBase/WXDefinitions.h"
#include "../../include/WXMedia/Stb.h"
#include <glad/glad.h>
#include <fstream>

namespace wx
{
	std::optional<ImTextureRef> LoadTexture(
		const std::filesystem::path& path, ImVec2& size)
	{
		std::error_code ec;
		if (!std::filesystem::is_regular_file(path, ec) || ec)
			return std::nullopt;

		std::ifstream file(path, std::ios::binary | std::ios::ate);
		if (!file)
			return std::nullopt;

		const wx::SSize fileSize = file.tellg();

		if (fileSize <= 0 ||
			fileSize > std::numeric_limits<int>::max())
		{
			return std::nullopt; // 防止过大
		}

		unsigned char* fileData = new unsigned char[fileSize];

		file.seekg(0, std::ios::beg);
		if (!file.read(
			reinterpret_cast<char*>(fileData),
			fileSize))
		{
			return std::nullopt;
		}

		Int32 width = 0;
		Int32 height = 0;
		Int32 channels = 0;
		// stbi_set_flip_vertically_on_load(true);

		stbi_uc* pixels = stbi_load_from_memory(
			fileData,
			static_cast<Int32>(fileSize),
			&width,
			&height,
			&channels,
			STBI_rgb_alpha
		);

		delete[] fileData;

		if (pixels == nullptr || width <= 0 || height <= 0)
		{
			stbi_image_free(pixels);
			return std::nullopt;
		}

		GLuint texture = 0;
		glGenTextures(1, &texture);

		if (texture == 0)
		{
			stbi_image_free(pixels);
			return std::nullopt;
		}

		GLint previousTexture = 0;
		GLint previousUnpackAlignment = 0;

		glGetIntegerv(
			GL_TEXTURE_BINDING_2D,
			&previousTexture
		);
		glGetIntegerv(
			GL_UNPACK_ALIGNMENT,
			&previousUnpackAlignment
		);

		glBindTexture(GL_TEXTURE_2D, texture);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

		glTexParameteri(
			GL_TEXTURE_2D,
			GL_TEXTURE_MIN_FILTER,
			GL_LINEAR_MIPMAP_LINEAR
		);
		glTexParameteri(
			GL_TEXTURE_2D,
			GL_TEXTURE_MAG_FILTER,
			GL_LINEAR
		);
		glTexParameteri(
			GL_TEXTURE_2D,
			GL_TEXTURE_WRAP_S,
			GL_CLAMP_TO_EDGE
		);
		glTexParameteri(
			GL_TEXTURE_2D,
			GL_TEXTURE_WRAP_T,
			GL_CLAMP_TO_EDGE
		);

		glTexImage2D(
			GL_TEXTURE_2D,
			0,
			GL_RGBA8,
			width,
			height,
			0,
			GL_RGBA,
			GL_UNSIGNED_BYTE,
			pixels
		);

		glGenerateMipmap(GL_TEXTURE_2D);

		const GLenum error = glGetError();

		glPixelStorei(
			GL_UNPACK_ALIGNMENT,
			previousUnpackAlignment
		);

		glBindTexture(
			GL_TEXTURE_2D,
			static_cast<GLuint>(previousTexture)
		);

		stbi_image_free(pixels);

		if (error != GL_NO_ERROR)
		{
			glDeleteTextures(1, &texture);
			return std::nullopt;
		}
		const ImTextureID textureId =
			static_cast<ImTextureID>(static_cast<std::uintptr_t>(texture));

		size.x = width;
		size.y = height;
		return ImTextureRef(textureId);
	}

	void DestroyTexture(ImTextureRef& textureRef)
	{
		const ImTextureID textureId = textureRef.GetTexID();
		if (textureId == ImTextureID_Invalid)
		{
			textureRef = {};
			return;
		}

		DestroyTexture(textureId);
		textureRef = {};
	}

	void DestroyTexture(ImTextureID textureId)
	{
		if (textureId == 0)
			return;
		const GLuint texture = static_cast<GLuint>(
			static_cast<std::uintptr_t>(textureId)
			);
		if (texture != 0)
			glDeleteTextures(1, &texture);
	}
}
