#pragma once

#include "GameConfig.h"
#include "Progression.h"

#include <array>

// Everything that changes from level to level (and all of the story copy) lives in this file.
// To tweak a level, edit its entry in `levels`. To add one, add an entry, bump `levelCount`
// and pick a TargetArt (a new building style is a small addition to TargetArt.cpp).
namespace riot
{
// Which placeholder building TargetArt.cpp draws at the right-hand end of the street.
enum class TargetArt
{
    Chippy,
    Bakery,
    CouncilOffices,
    Pizzeria,
    VillageHall,
    TownHall,
};

struct Caption
{
    const char* big;   // up to 8 characters
    const char* fine;  // up to 18 characters (not "small": windows.h #defines that)
};

struct TargetLook
{
    TargetArt art;
    const char* sign;     // one or two lines, separated by '\n'
    const char* tagline;  // small print under the sign
    Caption upper;        // top-lane window
    Caption lower;        // bottom-lane window
};

struct LevelDef
{
    const char* cause;         // intro card and HUD
    const char* target;        // intro card: "Target: <target>"
    const char* summaryLabel;  // win card: "<summaryLabel>: <summaryValue>"
    const char* summaryValue;
    const char* victory;       // win card subtitle
    const char* defeat;        // lose card subtitle
    TargetLook look;
    config::WaveList waves;    // crowd per wave: config::withCrowds({...})
    std::array<const char*, 4> placards{};       // this level's own slogans (see flavour::levelPlacardPercent)
    std::array<const char*, 2> nearTarget{};     // shouted near the target, e.g. "My curry sauce!"
    std::array<GoalDef, goalsPerLevel> goals{};  // bonus goals: one star each
    float patienceDrain = 1.0f;  // how fast blocked rioters lose patience; lower means a more stubborn crowd
    // Thrown things are just for show and never do any damage (see the end of Flavour.h). Per minute of a wave.
    float bricksPerMinute = 0.0f;
    float smokeBombsPerMinute = 0.0f;
    const char* crowdNote = nullptr;  // intro card: "Heads up: <crowdNote>"
    float startingBudget = config::startingBudget;
    int maxVans = config::maximumVans;
    float prepSeconds = config::prepSeconds;
};

inline constexpr int levelCount = 6;

// Crowd sizes grow every level, but the final wave stays at 57: the damage a crowd does grows with the square
// of its length, so a bigger finale quickly makes a level unwinnable. Keep every level winnable: the model
// tests run a simple barrier-line bot through each one.
inline constexpr std::array<LevelDef, levelCount> levels{{
    {
        .cause = "No more curry sauce at the chippy!",
        .target = "The chippy",
        .summaryLabel = "Curry sauce saved",
        .summaryValue = "1 bottle",
        .victory = "They've gone home for a sandwich.",
        .defeat = "They have asked to speak to the chip manager.",
        .look = {TargetArt::Chippy, "THE CHIPPY", "FISH / CHIPS / CHAOS",
                 {"SAUCE", "SOLD OUT!"}, {"OPEN", "ISH"}},
        .waves = config::withCrowds({12, 18, 27, 36, 45, 57}),
        .placards = {"CURRY SAUCE NOW!", "CURRY SAWCE NOW!", "SAUCE IS LIFE"},
        .nearTarget = {"My curry sauce!", "I can smell the sauce!"},
        .goals = {{{GoalType::NoBarriersLost}, {GoalType::MoodBelow, 15}}},
        .patienceDrain = 1.15f,
        .bricksPerMinute = 6.0f,
        .crowdNote = "They've brought bricks. They can't throw for toffee.",
    },
    {
        .cause = "The last sausage roll has been sold!",
        .target = "The village bakery",
        .summaryLabel = "Sausage rolls saved",
        .summaryValue = "1",
        .victory = "They've gone home to look for a pie instead.",
        .defeat = "The last sausage roll has been eaten. Messily.",
        .look = {TargetArt::Bakery, "VILLAGE\nBAKERY", "ROLLS / PIES / PANIC",
                 {"PIES", "ALL GONE"}, {"ROLLS", "1 LEFT!"}},
        .waves = config::withCrowds({15, 21, 30, 38, 47, 57}),
        .placards = {"WHERE'S MY SAUSAGE ROLL?", "SOSSIJ ROLLS 4 ALL", "FLAKY PASTRY RIGHTS"},
        .nearTarget = {"My sausage roll!", "Is that a pasty?"},
        .goals = {{{GoalType::NoVans}, {GoalType::BudgetLeft, 275}}},
        .patienceDrain = 1.05f,
        .bricksPerMinute = 8.0f,
        .smokeBombsPerMinute = 1.5f,
        .crowdNote = "New: smoke bombs! Very fizzy, totally harmless.",
    },
    {
        .cause = "Bin day has been moved to Thursday!",
        .target = "The council offices",
        .summaryLabel = "Council forms saved",
        .summaryValue = "400 (all in triplicate)",
        .victory = "They've gone home to put the bins out. On Thursday.",
        .defeat = "The forms have been shredded. Bin day is still a mystery.",
        .look = {TargetArt::CouncilOffices, "COUNCIL\nOFFICES", "FORMS / BINS / DESPAIR",
                 {"FORMS", "IN TRIPLICATE"}, {"BIN DAY", "THURSDAY NOW"}},
        .waves = config::withCrowds({18, 24, 32, 40, 48, 57}),
        .placards = {"BINS ARE A HUMAN RIGHT", "BINS ON MUNDAY", "THURSDAY IS WRONG"},
        .nearTarget = {"My bins!", "Where do I complain?"},
        .goals = {{{GoalType::NoBarriersLost}, {GoalType::NoVans}}},
        .patienceDrain = 1.0f,
        .bricksPerMinute = 10.0f,
        .smokeBombsPerMinute = 2.5f,
        .crowdNote = "More smoke bombs. Someone found a bulk-buy offer.",
    },
    {
        .cause = "Someone put pineapple on the town's official pizza!",
        .target = "Luigi's Pizzeria",
        .summaryLabel = "Pizzas saved",
        .summaryValue = "1 (pineapple-free)",
        .victory = "They've gone home to argue about toppings.",
        .defeat = "Someone has ordered a Hawaiian. Luigi has left the building.",
        .look = {TargetArt::Pizzeria, "LUIGI'S\nPIZZERIA", "NO PINEAPPLE. EVER.",
                 {"MENU", "NO PINEAPPLE"}, {"OPEN", "SORRY, LUIGI"}},
        .waves = config::withCrowds({21, 27, 34, 42, 49, 57}),
        .placards = {"NO PINEAPPLE", "NO PINAPPLE", "FRUIT IS NOT A TOPPING"},
        .nearTarget = {"My pizza!", "Hold the pineapple!"},
        .goals = {{{GoalType::MoodBelow, 10}, {GoalType::BudgetLeft, 225}}},
        .patienceDrain = 0.95f,
        .bricksPerMinute = 12.0f,
        .smokeBombsPerMinute = 3.5f,
        .crowdNote = "Even more smoke. Luigi can't see his oven.",
    },
    {
        .cause = "The village hall has run out of biscuits!",
        .target = "The village hall tea urn",
        .summaryLabel = "Biscuits protected",
        .summaryValue = "2 packets (one slightly broken)",
        .victory = "They've gone home for a lie down and a cup of tea.",
        .defeat = "The tea urn is empty. The biscuit tin is empty. Everyone is cross.",
        .look = {TargetArt::VillageHall, "VILLAGE\nHALL", "TEA URN. NO BISCUITS.",
                 {"TEA", "ONE URN ONLY"}, {"BISCUITS", "PLEASE?"}},
        .waves = config::withCrowds({24, 29, 36, 43, 50, 57}),
        .placards = {"BISCUITS OR BUST", "BISKITS OR BUST", "CUSTARD CREAMS NOW"},
        .nearTarget = {"My biscuits!", "Put the kettle on!"},
        .goals = {{{GoalType::NoBarriersLost}, {GoalType::MoodBelow, 20}}},
        .patienceDrain = 0.9f,
        .bricksPerMinute = 14.0f,
        .smokeBombsPerMinute = 5.0f,
        .crowdNote = "So much smoke. Nobody can find the biscuits.",
    },
    {
        .cause = "The bank holiday has been cancelled. Nobody knows why.",
        .target = "The town hall",
        .summaryLabel = "Town halls saved",
        .summaryValue = "1 (clock still wrong)",
        .victory = "They've gone home. Or to the pub. Either way, it's a holiday.",
        .defeat = "The bank holiday remains cancelled. The clock is still wrong.",
        .look = {TargetArt::TownHall, "TOWN HALL", "BANK HOLIDAY: NOPE",
                 {"BANK HOL", "CANCELLED"}, {"CLOCK", "SLIGHTLY WRONG"}},
        .waves = config::withCrowds({28, 32, 38, 44, 50, 57}),
        .placards = {"GIVE US OUR DAY OFF", "GIV US OUR DAY OFF", "WHY THO?"},
        .nearTarget = {"My day off!", "I had plans!"},
        .goals = {{{GoalType::NoVans}, {GoalType::NoBarriersLost}}},
        .patienceDrain = 0.85f,
        .bricksPerMinute = 16.0f,
        .smokeBombsPerMinute = 6.5f,
        .crowdNote = "Maximum fizz. Bring a hanky.",
    },
}};

constexpr bool levelsValid()
{
    for (const LevelDef& level : levels)
    {
        if (!level.cause[0] || !level.target[0] || !level.summaryLabel[0] || !level.summaryValue[0] ||
            !level.victory[0] || !level.defeat[0] || level.waves.count < 1 ||
            level.waves.count > config::maxWavesPerLevel || level.maxVans < 0 ||
            level.startingBudget < 0.0f || level.prepSeconds <= 0.0f || !level.nearTarget[0] ||
            level.patienceDrain <= 0.0f || level.bricksPerMinute < 0.0f || level.smokeBombsPerMinute < 0.0f)
            return false;
        for (const GoalDef& goal : level.goals)
            if ((goal.type == GoalType::MoodBelow && (goal.amount < 1 || goal.amount > 100)) ||
                ((goal.type == GoalType::BudgetLeft || goal.type == GoalType::MaxSpend) && goal.amount < 0))
                return false;
    }
    return true;
}
static_assert(levelsValid(), "Check the entries in riot::levels");

// Start screen copy. Gaps are blank lines; the long version keeps the original line breaks.
enum class CopyStyle { Title, Lead, Body, Heading, Bullet, Closer, Gap };

struct CopyLine
{
    CopyStyle style;
    const char* text;
};

inline constexpr auto startCopy = std::to_array<CopyLine>({
    {CopyStyle::Title, "RIOT CONTROL"},
    {CopyStyle::Lead, "Britain is in uproar. Again."},
    {CopyStyle::Gap, ""},
    {CopyStyle::Body, "Over the daftest things imaginable, crowds of very cross"},
    {CopyStyle::Body, "little people are marching on the town, and you're the"},
    {CopyStyle::Body, "only thing standing in their way."},
    {CopyStyle::Gap, ""},
    {CopyStyle::Heading, "YOUR JOB"},
    {CopyStyle::Bullet, "Place cones, barriers and vans to block their path"},
    {CopyStyle::Bullet, "Spend your budget wisely - you can't afford everything"},
    {CopyStyle::Bullet, "Keep building and repairing as the waves roll in"},
    {CopyStyle::Bullet, "Don't let the crowd reach their target"},
    {CopyStyle::Gap, ""},
    {CopyStyle::Heading, "THE RULES"},
    {CopyStyle::Body, "Rioters don't think. They don't go around. They just walk"},
    {CopyStyle::Body, "straight ahead and shove. Hold the line, and keep an eye"},
    {CopyStyle::Body, "on that Crowd Mood meter - if it fills up, it's all over."},
    {CopyStyle::Gap, ""},
    {CopyStyle::Closer, "How many levels can you survive?"},
});

// Used instead when the game is drawn smaller than 960x600 pixels (config::shortStartBelowScale).
inline constexpr auto startCopyShort = std::to_array<CopyLine>({
    {CopyStyle::Title, "RIOT CONTROL"},
    {CopyStyle::Gap, ""},
    {CopyStyle::Body, "The nation is revolting. Over very silly things."},
    {CopyStyle::Body, "Place barriers, hold the line, and don't let the"},
    {CopyStyle::Body, "crowd through."},
});

inline constexpr const char* startButton = "START";

inline constexpr const char* finalHeadline = "YOU SAVED THE NATION";
inline constexpr const char* finalSubline = "The kettle is on. Britain is calm. Mostly.";
}
