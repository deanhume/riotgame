#pragma once

#include <array>

// Star ratings, bonus goals, upgrades and cosmetics. Everything here is data: add or tweak entries freely.
// The ids are written to the save file, so rename an id only if you are happy for players to lose that entry.
namespace riot
{
// ---- Bonus goals ------------------------------------------------------------------------------------------------
// Each level picks goalsPerLevel of these in Levels.h (LevelDef::goals), with its own amounts.
enum class GoalType
{
    NoBarriersLost,  // no metal barrier or van breaks (cones are meant to, so they don't count)
    BudgetLeft,      // finish the level with at least `amount` dollars in the bank
    MaxSpend,        // spend no more than `amount` dollars on the whole level (gadgets and repairs)
    MoodBelow,       // the crowd mood never reaches `amount` percent
    NoVans,          // win without placing a police van
};

struct GoalDef
{
    GoalType type = GoalType::NoVans;
    int amount = 0;
};

inline constexpr int goalsPerLevel = 2;
// Stars for clearing a level having met 0, 1 or 2 of its bonus goals in the same run.
inline constexpr std::array<int, goalsPerLevel + 1> starsForGoals{1, 2, 3};
inline constexpr int maxStarsPerLevel = starsForGoals[goalsPerLevel];
// Upgrade points come from a level's best star count, so replaying a level only pays out for new stars.
inline constexpr int pointsPerStar = 1;

// ---- Upgrades ---------------------------------------------------------------------------------------------------
// Upgrades apply to every level. Each tier's amount is the total effect at that tier (not an increment).
enum class UpgradeEffect
{
    BarrierHealth,   // metal barriers get +amount percent health
    StartingBudget,  // +amount dollars at the start of every level
    RepairHealth,    // each repair restores +amount extra health
    ConeDiscount,    // cones cost `amount` dollars less
};

struct UpgradeTier
{
    int cost;    // upgrade points to buy this tier
    int amount;
};

inline constexpr int maxUpgradeTiers = 3;

struct UpgradeDef
{
    const char* id;
    const char* name;
    const char* blurb;
    UpgradeEffect effect;
    std::array<UpgradeTier, maxUpgradeTiers> tiers;
    int tierCount;
};

// To add the tea trolley later: add an UpgradeEffect, an entry here, and apply it in GameModel.
inline constexpr auto upgrades = std::to_array<UpgradeDef>({
    {"barriers", "REINFORCED BARRIERS", "Metal barriers take more shoving.", UpgradeEffect::BarrierHealth,
     {{{1, 15}, {2, 30}, {3, 50}}}, 3},
    {"budget", "BIGGER BUDGET", "More cash at the start of every level.", UpgradeEffect::StartingBudget,
     {{{1, 40}, {2, 80}, {3, 120}}}, 3},
    {"repairs", "FASTER REPAIRS", "Each repair patches more, so fewer clicks.", UpgradeEffect::RepairHealth,
     {{{1, 20}, {2, 40}, {3, 65}}}, 3},
    {"cones", "CHEAPER CONES", "A bulk discount at the cone warehouse.", UpgradeEffect::ConeDiscount,
     {{{1, 3}, {2, 6}}}, 2},
});
inline constexpr int upgradeCount = static_cast<int>(upgrades.size());

// ---- Cosmetics --------------------------------------------------------------------------------------------------
// Purely visual. Each one unlocks by total stars, by clearing a level, or by meeting a type of bonus goal.
enum class UnlockType
{
    Always,
    TotalStars,    // amount = stars needed across all levels
    LevelCleared,  // amount = level number (1-based)
    GoalMet,       // meet any bonus goal of type `goal`
};

struct Unlock
{
    UnlockType type = UnlockType::Always;
    int amount = 0;
    GoalType goal = GoalType::NoVans;
};

enum class HatArt { Cap, Bobby, ConeHat, PartyHat, Crown };
enum class ConeArt { Classic, Stripy, Gold, Sunglasses, Face };

struct HatDef
{
    const char* id;
    const char* name;
    HatArt art;
    Unlock unlock;
};

struct ConeDef
{
    const char* id;
    const char* name;
    ConeArt art;
    Unlock unlock;
};

// The first entry of each list is the default and must always be unlocked.
inline constexpr auto hats = std::to_array<HatDef>({
    {"cap", "PEAKED CAP", HatArt::Cap, {}},
    {"bobby", "BOBBY HELMET", HatArt::Bobby, {UnlockType::TotalStars, 3}},
    {"cone", "CONE HAT", HatArt::ConeHat, {UnlockType::GoalMet, 0, GoalType::NoVans}},
    {"party", "PARTY HAT", HatArt::PartyHat, {UnlockType::LevelCleared, 6}},
    {"crown", "TINY CROWN", HatArt::Crown, {UnlockType::TotalStars, 18}},
});

inline constexpr auto cones = std::to_array<ConeDef>({
    {"classic", "CLASSIC", ConeArt::Classic, {}},
    {"stripy", "STRIPY", ConeArt::Stripy, {UnlockType::TotalStars, 6}},
    {"shades", "COOL CONE", ConeArt::Sunglasses, {UnlockType::GoalMet, 0, GoalType::MoodBelow}},
    {"face", "FRIENDLY CONE", ConeArt::Face, {UnlockType::GoalMet, 0, GoalType::NoBarriersLost}},
    {"gold", "GOLD CONE", ConeArt::Gold, {UnlockType::TotalStars, 15}},
});
inline constexpr int hatCount = static_cast<int>(hats.size());
inline constexpr int coneCount = static_cast<int>(cones.size());
}
