#pragma once

#include <optional>
#include <vector>

namespace riot
{
namespace defendConfig
{
inline constexpr int lives = 3;
inline constexpr int shieldBlocks = 5;
inline constexpr float swipeSeconds = 0.18f;
inline constexpr float swipeCooldown = 0.55f;
inline constexpr float hitWindow = 0.30f;
inline constexpr float smokeSeconds = 3.5f;
inline constexpr float firstThrowDelay = 1.0f;
}

enum class DefendPhase { Ready, Active, Won, Lost };
enum class DefendThing { Brick, SmokeBomb };
enum class DefendOutcome { Sliced, Blocked, Hit, Smoked };

struct DefendThrow
{
    DefendThing kind = DefendThing::Brick;
    float age = 0.0f;
    float flight = 1.6f;
    int from = 0;
};

struct DefendEvent
{
    DefendOutcome outcome;
    DefendThrow projectile;
};

class DefendModel
{
public:
    void reset(int riotIndex);
    void begin();
    void tick(float dt);
    void raiseShield(bool raised);
    bool swipe(int direction);
    void debugFinish(bool win);
    std::vector<DefendEvent> takeEvents();

    DefendPhase phase() const { return phase_; }
    const std::optional<DefendThrow>& incoming() const { return incoming_; }
    int lives() const { return lives_; }
    int shieldHealth() const { return shield_; }
    bool shielding() const { return shieldRaised_ && shield_ > 0 && swing_ <= 0.0f; }
    int totalThrows() const { return total_; }
    int resolved() const { return resolved_; }
    int sliced() const { return sliced_; }
    int blocked() const { return blocked_; }
    float smoke() const { return smoke_; }
    float swing() const { return swing_; }
    float cooldown() const { return cooldown_; }
    int swingDirection() const { return direction_; }
    bool inReach() const;

private:
    void resolve(DefendOutcome outcome);

    DefendPhase phase_ = DefendPhase::Ready;
    int level_ = 0;
    int lives_ = defendConfig::lives;
    int shield_ = defendConfig::shieldBlocks;
    int total_ = 14;
    int launched_ = 0;
    int resolved_ = 0;
    int sliced_ = 0;
    int blocked_ = 0;
    int direction_ = 1;
    float due_ = defendConfig::firstThrowDelay;
    float swing_ = 0.0f;
    float cooldown_ = 0.0f;
    float smoke_ = 0.0f;
    bool shieldRaised_ = false;
    std::optional<DefendThrow> incoming_;
    std::vector<DefendEvent> events_;
};
}
