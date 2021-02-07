#include "SkillExperienceBuffer.h"
#include "version.h"
#include <cmath>

const std::string PapyrusClassName = "SleepToGainExperience";

struct Settings
{
	std::uint32_t enableSleepTimeRequirement : 1;
	std::uint32_t : 31;
	float percentExpRequiresSleep;
	float minDaysSleepNeeded;
	float interuptedPenaltyPercent;

	Settings() { ZeroMemory(this, sizeof(Settings)); }
};

Settings g_settings;
SkillExperienceBuffer g_experienceBuffer;


void PlayerCharacter_AdvanceSkill_Hooked(RE::PlayerCharacter* _this, RE::ActorValue skillId, float points, std::uint32_t unk1, std::uint32_t unk2)
{
	static const std::string ConsoleMenu = "Console";
	static const std::string ConsoleNativeUIMenu = "Console Native UI Menu";

	auto menuManager = RE::UI::GetSingleton();

	if (
		skillId >= FirstSkillId && skillId <= LastSkillId && _this == RE::PlayerCharacter::GetSingleton() &&
		menuManager && !menuManager->IsMenuOpen(ConsoleMenu) && !menuManager->IsMenuOpen(ConsoleNativeUIMenu))

	{
		g_experienceBuffer.addExperience(skillId, g_settings.percentExpRequiresSleep * points);

		// call the original function but with points = 0
		// not sure what effect not calling it might have
		PlayerCharacter_AdvanceSkill(_this, skillId, (1.0f - g_settings.percentExpRequiresSleep) * points, unk1, unk2);
	} else
		PlayerCharacter_AdvanceSkill(_this, skillId, points, unk1, unk2);

	logger::debug("AdvanceSkill_Hooked end");
}


void Serialization_Revert(SKSE::SerializationInterface*) { ZeroMemory(&g_experienceBuffer, sizeof(g_experienceBuffer)); }


constexpr std::uint32_t serializationDataVersion = 1;

void Serialization_Save(SKSE::SerializationInterface* serializationInterface)
{
	logger::info("Serialization_Save begin");

	if (serializationInterface->OpenRecord('DATA', serializationDataVersion)) {
		serializationInterface->WriteRecordData(&g_experienceBuffer, sizeof(g_experienceBuffer));
	}

	logger::info("Serialization_Save end");
}


void Serialization_Load(SKSE::SerializationInterface* serializationInterface)
{
	logger::info("Serialization_Load begin");

	std::uint32_t type;
	std::uint32_t version;
	std::uint32_t length;

	bool error = false;

	while (!error && serializationInterface->GetNextRecordInfo(type, version, length)) {
		if (type == 'DATA') {
			if (version == serializationDataVersion) {
				if (length == sizeof(g_experienceBuffer)) {
					serializationInterface->ReadRecordData(&g_experienceBuffer, length);
					logger::info("read data");
				}
#ifndef SKILLEXPERIENCEBUFFER_NOEXPERIMENTAL
				else if (length == sizeof(g_experienceBuffer.expBuf)) {
					serializationInterface->ReadRecordData(&g_experienceBuffer, length);
					logger::info("read data in old format");
				}
#endif
				else {
					logger::info("empty or invalid data");
				}
			} else {
				error = true;
				logger::info(FMT_STRING("version mismatch! read data version is {}, expected {}"), version, serializationDataVersion);
			}
		} else {
			logger::info(FMT_STRING("unhandled type {:x}"), type);
			error = true;
		}
	}

	logger::info("Serialization_Load end");
}


void Messaging_Callback(SKSE::MessagingInterface::Message* msg)
{
	//logger::info("Messaging_Callback begin");

	if (msg->type == SKSE::MessagingInterface::kDataLoaded) {
		// All forms are loaded
		logger::info("kMessage_DataLoaded");

		auto g_dataHandler = RE::TESDataHandler::GetSingleton();

		if ((g_dataHandler)->LookupModByName("SleepToGainExperience.esp") || (g_dataHandler)->LookupModByName("SleepToGainExperience.esl")) {
			REL::Relocation<std::uintptr_t> PCvtbl(RE::Offset::PlayerCharacter::Vtbl);

			PlayerCharacter_AdvanceSkill = PCvtbl.write_vfunc(0xF7, PlayerCharacter_AdvanceSkill_Hooked);
		} else
			logger::info("game plugin not enabled");
	}

	//logger::info("Messaging_Callback end");
}


float GetBufferedPointsBySkill(RE::StaticFunctionTag*, RE::BSFixedString skillName)
{
	auto skillId = RE::ActorValueList::ResolveActorValueByName(skillName.data());

	if (skillId >= FirstSkillId && skillId <= LastSkillId)
		return g_experienceBuffer.getExperience(skillId);

	return 0.0f;
}


void FlushBufferedExperience(RE::StaticFunctionTag*, float daysSlept, bool interupted)
{
	g_experienceBuffer.flushExperience((g_settings.enableSleepTimeRequirement ? fminf(1.0f, daysSlept / g_settings.minDaysSleepNeeded) : 1.0f) * (interupted ? g_settings.interuptedPenaltyPercent : 1.0f));
}


void FlushBufferedExperienceBySkill(RE::StaticFunctionTag*, RE::BSFixedString skillName, float daysSlept, bool interupted)
{
	auto skillId = RE::ActorValueList::ResolveActorValueByName(skillName.data());

	if (skillId >= FirstSkillId && skillId <= LastSkillId)
		g_experienceBuffer.flushExperienceBySkill(skillId, (g_settings.enableSleepTimeRequirement ? fminf(1.0f, daysSlept / g_settings.minDaysSleepNeeded) : 1.0f) * (interupted ? g_settings.interuptedPenaltyPercent : 1.0f));
}


void MultiplyBufferedExperience(RE::StaticFunctionTag*, float value) { g_experienceBuffer.multExperience(value); }


void MultiplyBufferedExperienceBySkill(RE::StaticFunctionTag*, RE::BSFixedString skillName, float value)
{
	auto skillId = RE::ActorValueList::ResolveActorValueByName(skillName.data());

	if (skillId >= FirstSkillId && skillId <= LastSkillId)
		g_experienceBuffer.multExperienceBySkill(skillId, value);
}


void ClearBufferedExperience(RE::StaticFunctionTag*) { g_experienceBuffer.clear(); }


void ClearBufferedExperienceBySkill(RE::StaticFunctionTag*, RE::BSFixedString skillName)
{
	auto skillId = RE::ActorValueList::ResolveActorValueByName(skillName.data());

	if (skillId >= FirstSkillId && skillId <= LastSkillId)
		g_experienceBuffer.clearBySkill(skillId);
}


bool RegisterFuncs(RE::BSScript::IVirtualMachine* registry)
{
	registry->RegisterFunction("GetBufferedPointsBySkill", PapyrusClassName, GetBufferedPointsBySkill);

	registry->RegisterFunction("FlushBufferedExperience", PapyrusClassName, FlushBufferedExperience);

	registry->RegisterFunction("FlushBufferedExperienceBySkill", PapyrusClassName, FlushBufferedExperienceBySkill);

	registry->RegisterFunction("MultiplyBufferedExperience", PapyrusClassName, MultiplyBufferedExperience);

	registry->RegisterFunction("MultiplyBufferedExperienceBySkill", PapyrusClassName, MultiplyBufferedExperienceBySkill);

	registry->RegisterFunction("ClearBufferedExperience", PapyrusClassName, ClearBufferedExperience);

	registry->RegisterFunction("ClearBufferedExperienceBySkill", PapyrusClassName, ClearBufferedExperienceBySkill, registry);

	logger::info("ALL Papyrus Functions Registery!");

	return true;
}


extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Query(const SKSE::QueryInterface* a_skse, SKSE::PluginInfo* a_info)
{
#ifndef NDEBUG
	auto sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
#else
	auto path = logger::log_directory();
	if (!path) {
		return false;
	}

	*path /= "SleepToGainExperience.log"sv;
	auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
#endif

	auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));

#ifndef NDEBUG
	log->set_level(spdlog::level::trace);
#else
	log->set_level(spdlog::level::info);
	log->flush_on(spdlog::level::trace);
#endif

	spdlog::set_default_logger(std::move(log));
	spdlog::set_pattern("%g(%#): [%^%l%$] %v"s);

	logger::info(FMT_STRING("SleepToGainExperience v{}"), MYFP_VERSION_VERSTRING);

	a_info->infoVersion = SKSE::PluginInfo::kVersion;
	a_info->name = "SleepToGainExperience";
	a_info->version = MYFP_VERSION_MAJOR;

	if (a_skse->IsEditor()) {
		logger::critical("Loaded in editor, marking as incompatible"sv);
		return false;
	}

	const auto ver = a_skse->RuntimeVersion();
	if (ver < SKSE::RUNTIME_1_5_39) {
		logger::critical(FMT_STRING("Unsupported runtime version {}"), ver.string());
		return false;
	}

	return true;
}


extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface* a_skse)
{
	logger::info("SleepToGainExperience loaded");

	SKSE::Init(a_skse);

	char stringBuffer[16];

	// configuration
	constexpr const char* configFileName = "Data\\SKSE\\Plugins\\SleepToGainExperience.ini";
	constexpr const char* sectionName = "General";

	// general
	g_settings.enableSleepTimeRequirement = GetPrivateProfileInt(sectionName, "bEnableSleepTimeRequirement", 0, configFileName);
	g_settings.minDaysSleepNeeded = fabsf((float)GetPrivateProfileInt(sectionName, "iMinHoursSleepNeeded", 7, configFileName)) / 24.0f;
	GetPrivateProfileString(sectionName, "fPercentRequiresSleep", "1.0", stringBuffer, sizeof(stringBuffer), configFileName);
	g_settings.percentExpRequiresSleep = fminf(1.0f, fabsf(strtof(stringBuffer, nullptr)));
	GetPrivateProfileString(sectionName, "fInteruptedPenaltyPercent", "1.0", stringBuffer, sizeof(stringBuffer), configFileName);
	g_settings.interuptedPenaltyPercent = fminf(1.0f, fabsf(strtof(stringBuffer, nullptr)));


	auto g_message = SKSE::GetMessagingInterface();

	if (!g_message) {
		logger::error("Messaging Interface Not Found!");
		return false;
	}

	if (g_message->RegisterListener(Messaging_Callback)) {
		logger::info("Register Event Call Back!");
	}


	auto g_serialization = SKSE::GetSerializationInterface();

	if (!g_serialization) {
		logger::error("Serialization Interface Not Found!");
		return false;
	}

	g_serialization->SetUniqueID('StGE');

	g_serialization->SetRevertCallback(Serialization_Revert);
	g_serialization->SetSaveCallback(Serialization_Save);
	g_serialization->SetLoadCallback(Serialization_Load);

	auto papyrus = SKSE::GetPapyrusInterface();

	if (!papyrus) {
		logger::error("Papyrus Interface Not Found!");
		return false;
	}

	if (papyrus->Register(RegisterFuncs)) {
		logger::info("Papyrus Functions Register");
	}


	return true;
}
