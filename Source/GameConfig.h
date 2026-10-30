#pragma once

#include <array>
#include <initializer_list>

namespace riot::config
{
inline constexpr int width = 1280;
inline constexpr int height = 800;
inline constexpr bool startFullscreen = true;
inline constexpr int lanes = 3;
inline constexpr int columns = 10;
inline constexpr float firstColumnX = 206.0f;
inline constexpr float columnSpacing = 78.0f;
inline constexpr std::array<float, lanes> laneY{444.0f, 340.0f, 236.0f};
inline constexpr float goalX = 1084.0f;
inline constexpr float dangerX = 956.0f;
inline constexpr float spawnX = -24.0f;
inline constexpr float walkerSpeed = 53.0f;
inline constexpr float walkerSpacing = 22.0f;
inline constexpr float pushDamagePerSecond = 3.5f;
inline constexpr float stumbleSeconds = 1.45f;
inline constexpr float prepSeconds = 15.0f;
inline constexpr float betweenWavesSeconds = 8.0f;
inline constexpr float startingBudget = 260.0f;
inline constexpr float budgetPerSecond = 10.0f;
inline constexpr float budgetCap = 420.0f;
inline constexpr float maximumMood = 100.0f;
inline constexpr float arrivalMood = 10.0f;
inline constexpr float proximityMoodPerSecond = 1.1f;
inline constexpr float moodRecoveryPerSecond = 0.65f;
inline constexpr int maximumVans = 2;
inline constexpr int repairCost = 20;
inline constexpr float repairHealth = 85.0f;
// Boredom: a blocked or queued rioter loses one second of patience per second (times the level's
// patienceDrain). Each rioter starts with patienceSeconds, give or take patienceSpread (0.35 = 35%).
inline constexpr float patienceSeconds = 12.0f;
inline constexpr float patienceSpread = 0.35f;
inline constexpr float boredAt = 0.4f;          // fraction of patience left when they start grumbling
inline constexpr float leaveSpeed = 40.0f;      // pixels per second while trudging home
inline constexpr float nearTargetMargin = 40.0f;  // "Nearly there!" this far before dangerX
// Thrown bricks and smoke bombs (harmless; rates are per level). Only rioters on screen and marching throw.
inline constexpr float throwFromX = 60.0f;
inline constexpr float throwShortest = 80.0f;   // a throw lands this far ahead of the thrower ...
inline constexpr float throwLongest = 220.0f;   // ... up to this far
inline constexpr float throwReach = 260.0f;     // bricks are aimed at the nearest barrier or van this close ahead
inline constexpr float fixedStep = 1.0f / 60.0f;

struct GadgetStats
{
    const char* name;
    int cost;
    float health;
    float halfWidth;
};

inline constexpr std::array<GadgetStats, 3> gadgets{{
    {"CONE", 12, 1.0f, 12.0f},
    {"BARRIER", 55, 205.0f, 22.0f},
    {"VAN", 150, 690.0f, 34.0f},
}};

// The start story shows its short text when the game is drawn below this fraction of 1280x800 (960x600).
inline constexpr float shortStartBelowScale = 0.75f;

struct Wave
{
    int walkers;
    float seconds;
    float spawnWindow;
};

inline constexpr int maxWavesPerLevel = 8;

struct WaveList
{
    std::array<Wave, maxWavesPerLevel> items{};
    int count = 0;

    constexpr const Wave& operator[](int index) const { return items[index]; }
    constexpr int size() const { return count; }
};

constexpr WaveList makeWaves(std::initializer_list<Wave> list)
{
    WaveList result;
    for (const Wave& wave : list)
        result.items[result.count++] = wave;
    return result;
}

// How long each wave lasts and how long its crowd takes to arrive. Every level shares this pacing.
inline constexpr WaveList wavePacing = makeWaves({
    {0, 28.0f, 12.0f},
    {0, 30.0f, 14.0f},
    {0, 32.0f, 16.0f},
    {0, 34.0f, 18.0f},
    {0, 36.0f, 20.0f},
    {0, 38.0f, 22.0f},
});

// A level's waves from just the crowd sizes: one number per wave, in order. A level with more waves than
// wavePacing reuses the last pacing entry.
constexpr WaveList withCrowds(std::initializer_list<int> crowds)
{
    WaveList result;
    for (const int walkers : crowds)
    {
        Wave wave = wavePacing[result.count < wavePacing.count ? result.count : wavePacing.count - 1];
        wave.walkers = walkers;
        result.items[result.count++] = wave;
    }
    return result;
}
}
