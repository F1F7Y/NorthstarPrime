#include "audio.h"
#include "dedicated/dedicated.h"
#include "tier1/convar.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <random>

CustomAudioManager g_CustomAudioManager;

EventOverrideData::EventOverrideData()
{
	Warning(eLog::AUDIO, "Initialised struct EventOverrideData without any data!\n");
	LoadedSuccessfully = false;
}

// Empty stereo 48000 WAVE file
unsigned char EMPTY_WAVE[45] = {0x52, 0x49, 0x46, 0x46, 0x25, 0x00, 0x00, 0x00, 0x57, 0x41, 0x56, 0x45, 0x66, 0x6D, 0x74, 0x20, 0x10, 0x00, 0x00, 0x00, 0x01, 0x00, 0x02,
								0x00, 0x44, 0xAC, 0x00, 0x00, 0x88, 0x58, 0x01, 0x00, 0x02, 0x00, 0x10, 0x00, 0x64, 0x61, 0x74, 0x61, 0x74, 0x00, 0x00, 0x00, 0x00};

EventOverrideData::EventOverrideData(const std::string& data, const fs::path& path)
{
	if (data.length() <= 0)
	{
		Error(eLog::AUDIO, NO_ERROR, "Failed reading audio override file %s: file is empty\n", path.string().c_str());
		return;
	}

	fs::path fsSamplesFolder = path;
	fsSamplesFolder = fsSamplesFolder.replace_extension();

	if (!FileExists(fsSamplesFolder))
	{
		Error(eLog::AUDIO, NO_ERROR,
			  "Failed reading audio override file %s: samples folder doesn't exist; should be named the same as the definition file without "
			  "JSON extension.\n",
			  path.string().c_str());
		return;
	}

	nlohmann::json jsData;
	try
	{
		jsData = nlohmann::json::parse(data);
	}
	catch (const std::exception& ex)
	{
		Error(eLog::AUDIO, NO_ERROR, "Failed reading audio override file %s: %s\n", path.string().c_str(), ex.what());
	}

	// fail if it's not a json obj (could be an array, string, etc)
	if (!jsData.is_object())
	{
		Error(eLog::AUDIO, NO_ERROR, "Failed reading audio override file %s: file is not a JSON object\n", path.string().c_str());
		return;
	}

	// fail if no event ids given
	if (!jsData.contains("EventId"))
	{
		Error(eLog::AUDIO, NO_ERROR, "Failed reading audio override file %s: JSON object does not have the EventId property\n", path.string().c_str());
		return;
	}

	// array of event ids
	if (jsData["EventId"].is_array())
	{
		for (auto& eventId : jsData["EventId"])
		{
			if (!eventId.is_string())
			{
				Error(eLog::AUDIO, NO_ERROR, "Failed reading audio override file %s: EventId array has a value of invalid type, all must be strings\n", path.string().c_str());
				return;
			}

			EventIds.push_back(eventId.get<std::string>());
		}
	}
	// singular event id
	else if (jsData["EventId"].is_string())
	{
		EventIds.push_back(jsData["EventId"].get<std::string>());
	}
	// incorrect type
	else
	{
		Error(eLog::AUDIO, NO_ERROR, "Failed reading audio override file %s: EventId property is of invalid type (must be a string or an array of strings)\n", path.string().c_str());
		return;
	}

	if (jsData.contains("EventIdRegex"))
	{
		// array of event id regex
		if (jsData["EventIdRegex"].is_array())
		{
			for (auto& eventId : jsData["EventIdRegex"])
			{
				if (!eventId.is_string())
				{
					Error(eLog::AUDIO, NO_ERROR, "Failed reading audio override file %s: EventIdRegex array has a value of invalid type, all must be strings\n", path.string().c_str());
					return;
				}

				const std::string& regex = eventId.get<std::string>();

				try
				{
					EventIdsRegex.push_back({regex, std::regex(regex)});
				}
				catch (...)
				{
					Error(eLog::AUDIO, NO_ERROR, "Malformed regex \"%s\" in audio override file %s\n", regex.c_str(), path.string().c_str());
					return;
				}
			}
		}
		// singular event id regex
		else if (jsData["EventIdRegex"].is_string())
		{
			const std::string& regex = jsData["EventIdRegex"].get<std::string>();
			try
			{
				EventIdsRegex.push_back({regex, std::regex(regex)});
			}
			catch (...)
			{
				Error(eLog::AUDIO, NO_ERROR, "Malformed regex \"%s\" in audio override file %s\n", regex.c_str(), path.string().c_str());
				return;
			}
		}
		// incorrect type
		else
		{
			Error(eLog::AUDIO, NO_ERROR,
				  "Failed reading audio override file %s: EventIdRegex property is of invalid type (must be a string or an array of "
				  "strings)\n",
				  path.string().c_str());
			return;
		}
	}

	if (jsData.contains("AudioSelectionStrategy"))
	{
		if (!jsData["AudioSelectionStrategy"].is_string())
		{
			Error(eLog::AUDIO, NO_ERROR, "Failed reading audio override file %s: AudioSelectionStrategy property must be a string\n", path.string().c_str());
			return;
		}

		std::string strategy = jsData["AudioSelectionStrategy"].get<std::string>();

		if (strategy == "sequential")
		{
			Strategy = AudioSelectionStrategy::SEQUENTIAL;
		}
		else if (strategy == "random")
		{
			Strategy = AudioSelectionStrategy::RANDOM;
		}
		else
		{
			Error(eLog::AUDIO, NO_ERROR, "Failed reading audio override file %s: AudioSelectionStrategy string must be either \"sequential\" or \"random\"\n", path.string().c_str());
			return;
		}
	}

	// load samples
	for (fs::directory_entry file : fs::recursive_directory_iterator(fsSamplesFolder))
	{
		if (file.is_regular_file() && file.path().extension().string() == ".wav")
		{
			std::string pathString = file.path().string();

			// Open the file.
			std::ifstream wavStream(pathString, std::ios::binary);

			if (wavStream.fail())
			{
				Error(eLog::AUDIO, NO_ERROR, "Failed reading audio sample %s\n", file.path().string().c_str());
				continue;
			}

			// Get file size.
			wavStream.seekg(0, std::ios::end);
			size_t fileSize = wavStream.tellg();
			wavStream.close();

			// Allocate enough memory for the file.
			// blank out the memory for now, then read it later
			uint8_t* data = new uint8_t[fileSize];
			memcpy(data, EMPTY_WAVE, sizeof(EMPTY_WAVE));
			Samples.push_back({fileSize, std::unique_ptr<uint8_t[]>(data)});

			// thread off the file read
			// should we spawn one thread per read? or should there be a cap to the number of reads at once?
			std::thread readThread(
				[pathString, fileSize, data]
				{
					std::shared_lock lock(g_CustomAudioManager.m_loadingMutex);
					std::ifstream wavStream(pathString, std::ios::binary);

					// would be weird if this got hit, since it would've worked previously
					if (wavStream.fail())
					{
						Error(eLog::AUDIO, NO_ERROR, "Failed async read of audio sample %s\n", pathString.c_str());
						return;
					}

					// read from after the header first to preserve the empty header, then read the header last
					wavStream.seekg(0, std::ios::beg);
					wavStream.read(reinterpret_cast<char*>(data), fileSize);
					wavStream.close();

					DevMsg(eLog::AUDIO, "Finished async read of audio sample %s\n", pathString.c_str());
				});

			readThread.detach();
		}
	}

	if (Samples.size() == 0)
		Warning(eLog::AUDIO, "Audio override %s has no valid samples! Sounds will not play for this event.\n", path.string().c_str());

	DevMsg(eLog::AUDIO, "Loaded audio override file %s\n", path.string().c_str());

	LoadedSuccessfully = true;
}

bool CustomAudioManager::TryLoadAudioOverride(const fs::path& defPath)
{
	if (IsDedicatedServer())
		return true; // silently fail

	std::ifstream jsonStream(defPath);
	std::stringstream jsonStringStream;

	// fail if no audio json
	if (jsonStream.fail())
	{
		Warning(eLog::AUDIO, "Unable to read audio override from file %s\n", defPath.string().c_str());
		return false;
	}

	while (jsonStream.peek() != EOF)
		jsonStringStream << (char)jsonStream.get();

	jsonStream.close();

	std::shared_ptr<EventOverrideData> data = std::make_shared<EventOverrideData>(jsonStringStream.str(), defPath);

	if (!data->LoadedSuccessfully)
		return false; // no logging, the constructor has probably already logged

	for (const std::string& eventId : data->EventIds)
	{
		DevMsg(eLog::AUDIO, "Registering sound event %s\n", eventId.c_str());
		m_loadedAudioOverrides.insert({eventId, data});
	}

	for (const auto& eventIdRegexData : data->EventIdsRegex)
	{
		DevMsg(eLog::AUDIO, "Registering sound event regex %s\n", eventIdRegexData.first.c_str());
		m_loadedAudioOverridesRegex.insert({eventIdRegexData.first, data});
	}

	return true;
}

typedef void (*MilesStopAll_Type)();
MilesStopAll_Type MilesStopAll;

void CustomAudioManager::ClearAudioOverrides()
{
	if (IsDedicatedServer())
		return;

	if (m_loadedAudioOverrides.size() > 0 || m_loadedAudioOverridesRegex.size() > 0)
	{
		// stop all miles sounds beforehand
		// miles_stop_all

		MilesStopAll();

		// this is cancer but it works
		Sleep(50);
	}

	// slightly (very) bad
	// wait for all audio reads to complete so we don't kill preexisting audio buffers as we're writing to them
	std::unique_lock lock(m_loadingMutex);

	m_loadedAudioOverrides.clear();
	m_loadedAudioOverridesRegex.clear();
}

template <typename Iter, typename RandomGenerator>
Iter select_randomly(Iter start, Iter end, RandomGenerator& g)
{
	std::uniform_int_distribution<> dis(0, std::distance(start, end) - 1);
	std::advance(start, dis(g));
	return start;
}

template <typename Iter>
Iter select_randomly(Iter start, Iter end)
{
	static std::random_device rd;
	static std::mt19937 gen(rd());
	return select_randomly(start, end, gen);
}

bool ShouldPlayAudioEvent(const char* eventName, const std::shared_ptr<EventOverrideData>& data)
{
	std::string eventNameString = eventName;
	std::string eventNameStringBlacklistEntry = ("!" + eventNameString);

	for (const std::string& name : data->EventIds)
	{
		if (name == eventNameStringBlacklistEntry)
			return false; // event blacklisted

		if (name == "*")
		{
			// check for bad sounds I guess?
			// really feel like this should be an option but whatever
			if (!!strstr(eventName, "_amb_") || !!strstr(eventName, "_emit_") || !!strstr(eventName, "amb_"))
				return false; // would play static noise, I hate this
		}
	}

	return true; // good to go
}

bool (*o_LoadSampleMetadata)(void* sample, void* audioBuffer, unsigned int audioBufferLength, int audioType);

bool LoadSampleMetadata_Internal(const char* parentEvent, void* sample, void* audioBuffer, unsigned int audioBufferLength, int audioType)
{
	const char* eventName = parentEvent;

	if (Cvar_ns_print_played_sounds->GetInt() > 0)
		DevMsg(eLog::AUDIO, "Playing event %s\n", eventName);

	auto iter = g_CustomAudioManager.m_loadedAudioOverrides.find(eventName);
	std::shared_ptr<EventOverrideData> overrideData;

	if (iter == g_CustomAudioManager.m_loadedAudioOverrides.end())
	{
		// override for that specific event not found, try wildcard
		iter = g_CustomAudioManager.m_loadedAudioOverrides.find("*");

		if (iter == g_CustomAudioManager.m_loadedAudioOverrides.end())
		{
			// not found

			// try regex
			for (const auto& item : g_CustomAudioManager.m_loadedAudioOverridesRegex)
				for (const auto& regexData : item.second->EventIdsRegex)
					if (std::regex_search(eventName, regexData.second))
						overrideData = item.second;

			if (!overrideData)
				// not found either
				return o_LoadSampleMetadata(sample, audioBuffer, audioBufferLength, audioType);
			else
			{
				// cache found pattern to improve performance
				g_CustomAudioManager.m_loadedAudioOverrides[eventName] = overrideData;
			}
		}
		else
			overrideData = iter->second;
	}
	else
		overrideData = iter->second;

	if (!ShouldPlayAudioEvent(eventName, overrideData))
		return o_LoadSampleMetadata(sample, audioBuffer, audioBufferLength, audioType);

	void* data = 0;
	unsigned int dataLength = 0;

	if (overrideData->Samples.size() == 0)
	{
		// 0 samples, turn off this particular event.

		// using a dummy empty wave file
		data = EMPTY_WAVE;
		dataLength = sizeof(EMPTY_WAVE);
	}
	else
	{
		std::pair<size_t, std::unique_ptr<uint8_t[]>>* dat = NULL;

		switch (overrideData->Strategy)
		{
		case AudioSelectionStrategy::RANDOM:
			dat = &*select_randomly(overrideData->Samples.begin(), overrideData->Samples.end());
			break;
		case AudioSelectionStrategy::SEQUENTIAL:
		default:
			dat = &overrideData->Samples[overrideData->CurrentIndex++];
			if (overrideData->CurrentIndex >= overrideData->Samples.size())
				overrideData->CurrentIndex = 0; // reset back to the first sample entry
			break;
		}

		if (!dat)
			Warning(eLog::AUDIO, "Could not get sample data from override struct for event %s! Shouldn't happen\n", eventName);
		else
		{
			data = dat->second.get();
			dataLength = dat->first;
		}
	}

	if (!data)
	{
		Warning(eLog::AUDIO, "Could not fetch override sample data for event {}! Using original data instead.\n", eventName);
		return o_LoadSampleMetadata(sample, audioBuffer, audioBufferLength, audioType);
	}

	audioBuffer = data;
	audioBufferLength = dataLength;

	// most important change: set the sample class buffer so that the correct audio plays
	*(void**)((uintptr_t)sample + 0xE8) = audioBuffer;
	*(unsigned int*)((uintptr_t)sample + 0xF0) = audioBufferLength;

	// 64 - Auto-detect sample type
	bool res = o_LoadSampleMetadata(sample, audioBuffer, audioBufferLength, 64);
	if (!res)
		Error(eLog::AUDIO, NO_ERROR, "LoadSampleMetadata failed! The game will crash :(\n");

	return res;
}

const char* pszAudioEventName = nullptr;

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
// Only: mileswin64.dll + 0xf66d is caled with audioType being variable
bool h_LoadSampleMetadata(void* sample, void* audioBuffer, unsigned int audioBufferLength, int audioType)
{
	// Raw source, used for voice data only
	if (audioType == 0)
		return o_LoadSampleMetadata(sample, audioBuffer, audioBufferLength, audioType);

	return LoadSampleMetadata_Internal(pszAudioEventName, sample, audioBuffer, audioBufferLength, audioType);
}

// calls LoadSampleMetadata ( only caller where audiotype isnt 0 )
bool (*o_SetSource)(void* sample, void* audioBuffer, unsigned int audioBufferLength, int audioType);

bool h_SetSource(void* sample, void* audioBuffer, unsigned int audioBufferLength, int audioType)
{
	return o_SetSource(sample, audioBuffer, audioBufferLength, audioType);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
// Calls SetSource, looks like a queue func or smth
void* (*o_sub_1800294C0)(void* a1, void* a2);

void* h_sub_1800294C0(void* a1, void* a2)
{
	pszAudioEventName = reinterpret_cast<const char*>((*((__int64*)a2 + 6)));
	return o_sub_1800294C0(a1, a2);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void (*o_MilesLog)(int level, const char* string);

void h_MilesLog(int level, const char* string)
{
	DevMsg(eLog::AUDIO, "%i - %s\n", level, string);
}

ON_DLL_LOAD_CLIENT("mileswin64.dll", MilesHook, (CModule module))
{
	o_LoadSampleMetadata = module.Offset(0xF110).RCast<bool (*)(void*, void*, unsigned int, int)>();
	HookAttach(&(PVOID&)o_LoadSampleMetadata, (PVOID)h_LoadSampleMetadata);

	o_SetSource = module.Offset(0xF600).RCast<bool (*)(void*, void*, unsigned int, int)>();
	HookAttach(&(PVOID&)o_SetSource, (PVOID)h_SetSource);

	o_sub_1800294C0 = module.Offset(0x294C0).RCast<void* (*)(void*, void*)>();
	HookAttach(&(PVOID&)o_sub_1800294C0, (PVOID)h_sub_1800294C0);
}

ON_DLL_LOAD_CLIENT("client.dll", AudioHooks, (CModule module))
{
	o_MilesLog = module.Offset(0x57DAD0).RCast<void (*)(int, const char*)>();
	HookAttach(&(PVOID&)o_MilesLog, (PVOID)h_MilesLog);

	MilesStopAll = module.Offset(0x580850).RCast<MilesStopAll_Type>();
}
