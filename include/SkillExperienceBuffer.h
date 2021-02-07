#pragma once

#define SKILLEXPERIENCEBUFFER_NOEXPERIMENTAL


using Actor_AdvanceSkill_Method = void (*)(RE::PlayerCharacter*, RE::ActorValue, float, std::uint32_t, std::uint32_t);
extern REL::Relocation<Actor_AdvanceSkill_Method> PlayerCharacter_AdvanceSkill;

constexpr RE::ActorValue FirstSkillId = RE::ActorValue::kOneHanded;
constexpr RE::ActorValue LastSkillId = RE::ActorValue::kEnchanting;
constexpr std::uint32_t SkillCount = (LastSkillId - FirstSkillId) + 1;

float GetExperienceForLevel(RE::ActorValueInfo* info, std::uint32_t level);

struct SkillExperienceBuffer
{
	float expBuf[SkillCount];
#ifndef SKILLEXPERIENCEBUFFER_NOEXPERIMENTAL
	float flushedExpBuf[SkillCount];
	std::uint16_t skillStartLevel[SkillCount];
#endif

	SkillExperienceBuffer()
	{
		ZeroMemory(this, sizeof(SkillExperienceBuffer));
	}

	float getExperience(RE::ActorValue skillId) const;

	void addExperience(RE::ActorValue skillId, float points);

	void flushExperience(float percent);
	void flushExperienceBySkill(RE::ActorValue skillId, float percent);

	void multExperience(float mult);
	void multExperienceBySkill(RE::ActorValue skillId, float mult);

	void clear();
	void clearBySkill(RE::ActorValue skillId);
};
