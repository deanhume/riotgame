#include "DefendModel.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace riot
{
void DefendModel::reset(int riotIndex)
{
    *this = DefendModel{};
    level_ = std::clamp(riotIndex, 0, 5);
    total_ = 14 + level_ * 2;
}

void DefendModel::begin()
{
    if (phase_ == DefendPhase::Ready)
        phase_ = DefendPhase::Active;
}

void DefendModel::raiseShield(bool raised)
{
    shieldRaised_ = raised && phase_ == DefendPhase::Active;
}

bool DefendModel::swipe(int direction)
{
    if (phase_ != DefendPhase::Active || cooldown_ > 0.0f)
        return false;
    direction_ = direction < 0 ? -1 : 1;
    swing_ = defendConfig::swipeSeconds;
    cooldown_ = defendConfig::swipeCooldown;
    if (inReach())
        resolve(DefendOutcome::Sliced);
    return true;
}

bool DefendModel::inReach() const
{
    return incoming_ && incoming_->age >= incoming_->flight - defendConfig::hitWindow;
}

void DefendModel::resolve(DefendOutcome outcome)
{
    events_.push_back({outcome, *incoming_});
    incoming_.reset();
    ++resolved_;
    switch (outcome)
    {
    case DefendOutcome::Sliced:
        ++sliced_;
        break;
    case DefendOutcome::Blocked:
        --shield_;
        ++blocked_;
        break;
    case DefendOutcome::Hit:
        --lives_;
        break;
    case DefendOutcome::Smoked:
        smoke_ = defendConfig::smokeSeconds;
        break;
    }
    due_ = 0.65f - static_cast<float>(level_) * 0.04f;
    if (lives_ <= 0)
        phase_ = DefendPhase::Lost;
    else if (resolved_ == total_)
        phase_ = DefendPhase::Won;
    if (phase_ != DefendPhase::Active)
        shieldRaised_ = false;
}

void DefendModel::tick(float dt)
{
    if (phase_ != DefendPhase::Active || !std::isfinite(dt) || dt <= 0.0f)
        return;
    dt = std::min(dt, 0.1f);
    smoke_ = std::max(0.0f, smoke_ - dt);
    cooldown_ = std::max(0.0f, cooldown_ - dt);
    if (incoming_)
    {
        incoming_->age += dt;
        if (swing_ > 0.0f && inReach())
            resolve(DefendOutcome::Sliced);
        else if (incoming_->age >= incoming_->flight)
        {
            if (shielding())
                resolve(DefendOutcome::Blocked);
            else
                resolve(incoming_->kind == DefendThing::Brick ? DefendOutcome::Hit : DefendOutcome::Smoked);
        }
    }
    else if (launched_ < total_)
    {
        due_ -= dt;
        if (due_ <= 0.0f)
        {
            // A repeatable barrage: every fifth throw is smoke, with a rotating distant thrower.
            incoming_ = DefendThrow{launched_ % 5 == 4 ? DefendThing::SmokeBomb : DefendThing::Brick,
                                   0.0f, 1.65f - static_cast<float>(level_) * 0.08f,
                                   (launched_ * 7 + level_) % 3 - 1};
            ++launched_;
        }
    }
    swing_ = std::max(0.0f, swing_ - dt);
}

void DefendModel::debugFinish(bool win)
{
    if (phase_ != DefendPhase::Active)
        return;
    incoming_.reset();
    shieldRaised_ = false;
    if (win)
    {
        resolved_ = total_;
        phase_ = DefendPhase::Won;
    }
    else
    {
        lives_ = 0;
        phase_ = DefendPhase::Lost;
    }
}

std::vector<DefendEvent> DefendModel::takeEvents()
{
    return std::exchange(events_, {});
}
}
