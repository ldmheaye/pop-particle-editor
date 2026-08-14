#include "../include/PopState.h"

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

#ifdef _WIN32
#include <Windows.h>
#endif

namespace fs = std::filesystem;

namespace
{
	fs::path ExecutableDirectory()
	{
#ifdef _WIN32
		wchar_t path[MAX_PATH]{};
		const DWORD length = GetModuleFileNameW(nullptr, path, MAX_PATH);
		if (length > 0 && length < MAX_PATH)
			return fs::path(path).parent_path();
#endif
		return {};
	}

	const char* LanguageFileName(pop::PopLanguage language)
	{
		switch (language)
		{
			case pop::PopLanguage::Chinese: return "zh.lang.json";
			case pop::PopLanguage::English:  return "en.lang.json";
		}
		return "en.lang.json";
	}

	bool LoadLanguageFile(const fs::path& path)
	{
		std::ifstream file(path);
		if (!file.is_open())
			return false;

		try
		{
			nlohmann::json json;
			file >> json;
			if (!json.is_object())
				return false;

			for (auto itr = json.begin(); itr != json.end(); ++itr)
			{
				if (itr.value().is_string())
					pop::gMultiLangDictionary.insert_or_assign(itr.key(), itr.value().get<std::string>());
			}
			return true;
		}
		catch (...)
		{
			return false;
		}
	}
}

namespace pop
{
	void OnInitMultiLang(PopLanguage language)
	{
		gMultiLangDictionary.clear();

		const fs::path globalDirectory = ExecutableDirectory() / "global";

		// Try the requested language first.
		if (LoadLanguageFile(globalDirectory / LanguageFileName(language)))
		{
			gCurrentLanguage = language;
			return;
		}

		// Fall back to English when the requested language file is missing.
		if (language != PopLanguage::English &&
			LoadLanguageFile(globalDirectory / LanguageFileName(PopLanguage::English)))
		{
			gCurrentLanguage = PopLanguage::English;
			return;
		}

		// Neither file is available; keep the dictionary empty (ML returns the raw key).
		gCurrentLanguage = PopLanguage::English;
	}

	void OnInitUserConfig()
	{
		fs::path configPath = ExecutableDirectory() / "user.config.json";

		if (!fs::exists(configPath))
			SaveUserConfig();

		std::ifstream ifs;
		ifs.open(configPath, std::ios::binary);

		nlohmann::json config;
		ifs >> config;

		gUserConfig = config.get<UserConfig>();

		OnInitMultiLang(gUserConfig.language);

		ifs.close();
	}

	void SaveUserConfig()
	{
		fs::path configPath = ExecutableDirectory() / "user.config.json";

		std::ofstream ofs;
		ofs.open(configPath);

		nlohmann::json outJson(gUserConfig);

		ofs << outJson;

		ofs.close();
	}
}
