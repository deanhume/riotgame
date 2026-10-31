#pragma once

#include "Levels.h"

#include <array>
#include <cstdint>

// Rioter personality: what they say, and what they carry. All of it is data, so add or tweak entries freely.
// Every sign and flag here is invented and daft on purpose: no real flags, symbols, slogans or groups.
namespace riot::flavour
{
// ---- Speech bubbles ---------------------------------------------------------------------------------------------
enum class Moment
{
    Walking,     // idle chatter on the march
    Blocked,     // just walked into a barrier or van
    Tripped,     // fell over a cone
    NearTarget,  // close to the target (the level's LevelDef::nearTarget lines are added to these)
    Bored,       // patience running low
    GivingUp,    // turning round to go home
    Throwing,    // just lobbed a brick or a smoke bomb
};
inline constexpr int momentCount = 7;

struct Phrase
{
    Moment moment;
    const char* text;
};

inline constexpr auto phrases = std::to_array<Phrase>({
    {Moment::Walking, "Oi!"},
    {Moment::Walking, "Right then."},
    {Moment::Walking, "I'm not happy about this."},
    {Moment::Walking, "Mind my placard."},
    {Moment::Walking, "Are we nearly there?"},
    {Moment::Blocked, "Move it!"},
    {Moment::Blocked, "Excuse me?!"},
    {Moment::Blocked, "Rude."},
    {Moment::Blocked, "Push harder!"},
    {Moment::Tripped, "Ow!"},
    {Moment::Tripped, "Who put that there?"},
    {Moment::Tripped, "Ooh, a free cone."},
    {Moment::NearTarget, "Nearly there!"},
    {Moment::Bored, "Is this worth it?"},
    {Moment::Bored, "I'll just pop home."},
    {Moment::Bored, "My feet hurt."},
    {Moment::GivingUp, "Bit cold, this. Going home."},
    {Moment::GivingUp, "I've left the oven on."},
    {Moment::GivingUp, "Right. I'm off."},
    {Moment::Throwing, "Catch!"},
    {Moment::Throwing, "Fore!"},
    {Moment::Throwing, "Incoming!"},
    {Moment::Throwing, "Butterfingers!"},
});

// Chance that a moment produces a bubble when there is room for one (idle chatter rolls this on each attempt).
inline constexpr std::array<float, momentCount> chance{
    0.6f,   // Walking
    0.35f,  // Blocked
    0.55f,  // Tripped
    0.5f,   // NearTarget
    0.45f,  // Bored
    0.8f,   // GivingUp
    0.3f,   // Throwing
};
inline constexpr int maxBubbles = 3;             // on screen at once
inline constexpr float bubbleSeconds = 1.6f;
inline constexpr float bubbleGap = 0.35f;        // at least this long between two new bubbles
inline constexpr float quietSeconds = 4.0f;      // a rioter who has just spoken waits this long to speak again
inline constexpr float chatterEvery = 2.5f;      // average seconds between attempts at idle chatter

// ---- Props ------------------------------------------------------------------------------------------------------
enum class PropKind { Nothing, Placard, Flag, Inflatable };

struct PropOdds
{
    PropKind kind;
    int weight;
};

// What a rioter carries. A rioter who trips over a cone also steals it and wears it as a hat.
inline constexpr auto propOdds = std::to_array<PropOdds>({
    {PropKind::Placard, 55},
    {PropKind::Flag, 20},
    {PropKind::Nothing, 15},
    {PropKind::Inflatable, 10},  // a sad, deflated dinosaur
});

// Placards show the level's own slogans (LevelDef::placards) this often, and the generic ones otherwise.
inline constexpr int levelPlacardPercent = 55;

inline constexpr auto genericPlacards = std::to_array<const char*>({
    "I'M VERY CROSS",
    "THIS IS A BIT MUCH",
    "HONK IF YOU'RE ANNOYED",
    "MY MUM SAYS I CAN",
    "WE WANT JUSTICE (AND CHIPS)",
    "DOWN WITH THIS SORT OF THING",
    // Misspelt on purpose.
    "I AM VERY CROSSS",
    "WE WANT JUSTISE (AND CHIPS)",
    "DOWN WITH THIS SORT OF FING",
});

// Placard board and lettering colours; each placard picks one of each.
inline constexpr auto boards = std::to_array<std::uint32_t>({0xe8c18c, 0xf6f1e3, 0xffe17a, 0xf6b9c9, 0xbfe0f2});
inline constexpr auto inks = std::to_array<std::uint32_t>({0x2a2a33, 0xb23b3b, 0x2f4fa0});

enum class FlagArt { SausageRoll, Teapot, NoPineapple, BinLid, Neutral, SausageDog };
enum class FlagField { Plain, Rainbow };

// Top to bottom; light colours keep the oversized cartoon dog readable at game scale.
inline constexpr auto rainbowStripes = std::to_array<std::uint32_t>({
    0xf47c71, 0xffad61, 0xffdf78, 0x8ddd8a, 0x78c8ed, 0x9aabeb, 0xc79de0,
});

struct FlagDef
{
    FlagArt art;
    FlagField field;
    std::uint32_t colour;          // plain field, or hem colour on striped flags
    const char* text = nullptr;    // lettering, if the flag has any
};

// Colour variations are just extra entries.
inline constexpr auto flags = std::to_array<FlagDef>({
    {FlagArt::SausageRoll, FlagField::Plain, 0x7fc8a9},
    {FlagArt::SausageRoll, FlagField::Plain, 0xf2d06b},
    {FlagArt::Teapot, FlagField::Plain, 0xf6b3c4},
    {FlagArt::Teapot, FlagField::Plain, 0xa9d3ee},
    {FlagArt::NoPineapple, FlagField::Plain, 0xf6f1e3},
    {FlagArt::BinLid, FlagField::Plain, 0x7aa36f},
    {FlagArt::BinLid, FlagField::Plain, 0xc9a2d8},
    {FlagArt::Neutral, FlagField::Plain, 0xd9c7a2, "NEUTRAL (BUT ANNOYED)"},
    // Rainbow stripes with a huge sausage dog in the middle: a fictional, daft emblem.
    {FlagArt::SausageDog, FlagField::Rainbow, 0x9ccbec},
    {FlagArt::SausageDog, FlagField::Rainbow, 0xf3a9c0},
});

struct Prop
{
    PropKind kind = PropKind::Nothing;
    const char* text = nullptr;  // placard slogan
    int flag = 0;                // index into flags
    int board = 0;               // index into boards
    int ink = 0;                 // index into inks
    int lift = 0;                // 0-2: how high it is held, so a queue's signs don't all line up
};

// Spreads one rioter's random `look` into independent choices.
constexpr std::uint32_t mix(std::uint32_t value, std::uint32_t salt)
{
    value ^= salt * 0x9e3779b9u;
    value ^= value >> 16;
    value *= 0x85ebca6bu;
    value ^= value >> 13;
    value *= 0xc2b2ae35u;
    value ^= value >> 16;
    return value;
}

constexpr Prop chooseProp(std::uint32_t look, const LevelDef& level)
{
    Prop prop;
    int total = 0;
    for (const PropOdds& odds : propOdds)
        total += odds.weight;
    int roll = static_cast<int>(mix(look, 1) % static_cast<std::uint32_t>(total));
    for (const PropOdds& odds : propOdds)
    {
        if (roll < odds.weight)
        {
            prop.kind = odds.kind;
            break;
        }
        roll -= odds.weight;
    }
    prop.lift = static_cast<int>(mix(look, 2) % 3);
    if (prop.kind == PropKind::Placard)
    {
        std::uint32_t own = 0;
        while (own < level.placards.size() && level.placards[own])
            ++own;
        const bool local = own > 0 && static_cast<int>(mix(look, 3) % 100) < levelPlacardPercent;
        prop.text = local ? level.placards[mix(look, 4) % own] : genericPlacards[mix(look, 4) % genericPlacards.size()];
        prop.board = static_cast<int>(mix(look, 5) % boards.size());
        prop.ink = static_cast<int>(mix(look, 6) % inks.size());
    }
    else if (prop.kind == PropKind::Flag)
        prop.flag = static_cast<int>(mix(look, 7) % flags.size());
    return prop;
}

// ---- Outfits ----------------------------------------------------------------------------------------------------
// Plenty of the crowd turn up in black knitted balaclavas (made by their nans, probably).
inline constexpr int balaclavaPercent = 60;
inline constexpr std::uint32_t balaclava = 0x1b1b22;
inline constexpr std::uint32_t balaclavaKnit = 0x34343f;  // a little stitching highlight

constexpr bool wearsBalaclava(std::uint32_t look)
{
    return static_cast<int>(mix(look, 8) % 100) < balaclavaPercent;
}

// ---- Thrown things ----------------------------------------------------------------------------------------------
// Bricks bonk off barriers and smoke bombs fizz like flares, but neither ever does any damage: they are just for
// show. How often each level's crowd throws them is in LevelDef (bricksPerMinute, smokeBombsPerMinute).
inline constexpr auto smokeColours = std::to_array<std::uint32_t>({
    0xff8fb8, 0xffa95e, 0x9be27a, 0xb99cf0, 0x7cc9f2, 0xffdf6e,
});
inline constexpr float smokeBurnSeconds = 6.0f;      // how long a smoke bomb fizzes once it lands
inline constexpr float smokePuffsPerSecond = 14.0f;
inline constexpr float smokePuffSeconds = 2.8f;      // how long one puff of smoke hangs about
inline constexpr float smokeSparksPerSecond = 28.0f;
inline constexpr float throwArmSeconds = 0.35f;      // how long the thrower's arm stays up
}
