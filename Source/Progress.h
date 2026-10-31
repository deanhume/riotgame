#pragma once

#include "Levels.h"
#include "Progression.h"

#include <array>
#include <filesystem>
#include <string>
#include <string_view>

namespace riot
{
// Everything that survives between sessions: best stars, goals, upgrades, cosmetics and carried cash.
// Saved as a small key=value text file (see serialize()).
struct Progress
{
    std::array<int, levelCount> stars{};
    std::array<unsigned, levelCount> goalsMet{};  // bit i: the level's goal i has been met at least once
    std::array<int, levelCount> defended{};  // mandatory interlude cleared after the corresponding riot
    std::array<int, upgradeCount> tiers{};
    int hat = 0;
    int cone = 0;
    int carryCash = 0;

    int totalStars() const;
    int pointsEarned() const;
    int pointsSpent() const;
    int points() const { return pointsEarned() - pointsSpent(); }
    bool levelUnlocked(int index) const;
    bool goalTypeMet(GoalType type) const;
    bool unlocked(const Unlock& unlock) const;
    int upgradeAmount(UpgradeEffect effect) const;
    // Points for the next tier of an upgrade, or 0 when it is maxed out.
    int nextTierCost(int upgrade) const;
    bool canBuy(int upgrade) const;
    bool anyAffordable() const;
    // Keeps the best star count and remembers the goals met.
    void record(int level, int stars, unsigned goals);
    // Clamps everything into range, refunds upgrades the points cannot pay for, and unequips locked cosmetics.
    void sanitize();
};

std::string goalText(const GoalDef& goal);                           // "Lose no barriers or officers"
std::string goalShortText(GoalType type);                            // "NO OFFICERS"
std::string unlockText(const Unlock& unlock);                        // "3 STARS"
std::string upgradeEffectText(const UpgradeDef& upgrade, int tier);  // "+15% BARRIER HEALTH"; tier 0 = none

std::string serialize(const Progress& progress);
// Never throws. Unknown keys and bad values are skipped, so a damaged file only loses the damaged parts.
Progress parseProgress(std::string_view text);
// False if there is no save or it cannot be read; `progress` is then left alone.
bool loadProgress(const std::filesystem::path& path, Progress& progress);
// Writes a temporary file and renames it over the old one, so a failed save keeps the previous one.
bool saveProgress(const std::filesystem::path& path, const Progress& progress);

constexpr bool unlockValid(const Unlock& unlock)
{
    switch (unlock.type)
    {
    case UnlockType::Always:
        return true;
    case UnlockType::TotalStars:
        return unlock.amount > 0 && unlock.amount <= levelCount * maxStarsPerLevel;
    case UnlockType::LevelCleared:
        return unlock.amount >= 1 && unlock.amount <= levelCount;
    case UnlockType::GoalMet:
        for (const LevelDef& level : levels)
            for (const GoalDef& goal : level.goals)
                if (goal.type == unlock.goal)
                    return true;
        return false;
    }
    return false;
}

constexpr bool progressionValid()
{
    if (hats[0].unlock.type != UnlockType::Always || cones[0].unlock.type != UnlockType::Always)
        return false;
    for (const HatDef& hat : hats)
        if (!unlockValid(hat.unlock))
            return false;
    for (const ConeDef& cone : cones)
        if (!unlockValid(cone.unlock))
            return false;
    for (const UpgradeDef& upgrade : upgrades)
    {
        if (upgrade.tierCount < 1 || upgrade.tierCount > maxUpgradeTiers)
            return false;
        for (int tier = 0; tier < upgrade.tierCount; ++tier)
            if (upgrade.tiers[tier].cost < 1)
                return false;
    }
    return true;
}
// A GoalMet unlock needs at least one level with that type of goal, or it could never be earned.
static_assert(progressionValid(), "Check the upgrades and cosmetics in Progression.h");
}
