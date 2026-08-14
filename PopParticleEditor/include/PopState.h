#ifndef __POP_STATE_H__
#define __POP_STATE_H__

#include <cstdio>
#include <string>
#include <unordered_map>
#include "PopResource.h"

#include <nlohmann/json.hpp>

namespace pop
{
	enum class PopLanguage
	{
		Chinese,
		English
	};

	NLOHMANN_JSON_SERIALIZE_ENUM(PopLanguage, {
		{PopLanguage::Chinese, "Chinese"},
		{PopLanguage::English, "English"},
		});

	inline std::unordered_map<std::string, PopResource> gPopResources;
	inline std::unordered_map<std::string, std::string> gMultiLangDictionary;
	inline PopLanguage gCurrentLanguage = PopLanguage::Chinese;

	// Loads <executable dir>/global/<code>.lang.json for the requested language.
	// Falls back to English when the requested language file does not exist.
	void OnInitMultiLang(PopLanguage language);
	void OnInitUserConfig();
	void SaveUserConfig();

	inline const char* ML(const char* index)
	{
		const auto itr = gMultiLangDictionary.find(index);
		if (itr == gMultiLangDictionary.end())
			return index;

		return itr->second.c_str();
	}

	template <typename... Args>
	std::string MLF(const char* index, Args... args)
	{
		const char* format = ML(index);
		const int length = std::snprintf(
			nullptr,
			0,
			format,
			args...);
		if (length <= 0)
			return format;

		std::string result(static_cast<std::size_t>(length) + 1, '\0');
		std::snprintf(
			result.data(),
			result.size(),
			format,
			args...);
		result.pop_back();
		return result;
	}

	struct UserConfig
	{
		PopLanguage language = PopLanguage::Chinese;

		NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(UserConfig, language);
	};

	inline UserConfig gUserConfig;
}

#endif // !__POP_STATE_H__
