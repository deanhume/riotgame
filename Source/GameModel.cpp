#include "GameModel.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <numeric>
#include <string_view>
#include <utility>

namespace riot
{
namespace
{
std::string upper(std::string_view text)
{
    std::string result(text);
    for (char& letter : result)
        letter = static_cast<char>(std::toupper(static_cast<unsigned char>(letter)));
    return result;
}

bool isWall(GadgetType type)
{
    return type == GadgetType::Barrier || type == GadgetType::Van;
}

// One step of a little random number generator: advances `seed` and returns well-mixed bits.
std::uint32_t nextRandom(std::uint32_t& seed)
{
    seed = seed * 1664525u + 1013904223u;
    std::uint32_t value = seed;
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    value ^= value >> 16;
    return value;
}
}

float GameModel::columnX(int column)
{
    return config::firstColumnX + static_cast<float>(column) * config::columnSpacing;
}

const config::GadgetStats& GameModel::stats(GadgetType type)
{
    return config::gadgets.at(static_cast<std::size_t>(type));
}

bool GameModel::canBuild() const
{
    return phase_ == Phase::Prep || phase_ == Phase::Wave || phase_ == Phase::Intermission;
}

int GameModel::cost(GadgetType type) const
{
    int price = stats(type).cost;
    if (type == GadgetType::Cone)
        price = std::max(1, price - progress_.upgradeAmount(UpgradeEffect::ConeDiscount));
    return price;
}

float GameModel::maxHealth(GadgetType type) const
{
    float health = stats(type).health;
    if (type == GadgetType::Barrier)
        health *= 1.0f + static_cast<float>(progress_.upgradeAmount(UpgradeEffect::BarrierHealth)) / 100.0f;
    return health;
}

int GameModel::repairCost() const
{
    return config::repairCost;
}

float GameModel::repairHealth() const
{
    return config::repairHealth + static_cast<float>(progress_.upgradeAmount(UpgradeEffect::RepairHealth));
}

float GameModel::startingBudget() const
{
    return level().startingBudget + static_cast<float>(progress_.upgradeAmount(UpgradeEffect::StartingBudget));
}

std::uint32_t GameModel::random()
{
    return nextRandom(seed_);
}

float GameModel::throwRoll()
{
    return static_cast<float>(nextRandom(throwSeed_) & 0xffffu) / 65536.0f;
}

// Seconds until the next throw: on average 60 / perMinute, give or take half of that.
float GameModel::throwGap(float perMinute)
{
    return perMinute > 0.0f ? 60.0f / perMinute * (0.5f + throwRoll()) : 0.0f;
}

void GameModel::resetLevel()
{
    budget_ = startingBudget();
    mood_ = 0.0f;
    peakMood_ = 0.0f;
    remaining_ = level().prepSeconds;
    spawnTimer_ = 0.0f;
    waveIndex_ = -1;
    spawned_ = 0;
    nextId_ = 0;
    // Every attempt at a level gets the same crowd, so retries and tests are repeatable.
    seed_ = 0x9e3779b9u ^ (static_cast<std::uint32_t>(levelIndex_ + 1) * 0x85ebca6bu);
    throwSeed_ = 0x51ed270bu ^ (static_cast<std::uint32_t>(levelIndex_ + 1) * 0x27d4eb2fu);
    brickTimer_ = 0.0f;
    smokeTimer_ = 0.0f;
    bricksThrown_ = 0;
    smokeBombsThrown_ = 0;
    heldOff_ = 0;
    gaveUp_ = 0;
    reached_ = 0;
    trips_ = 0;
    vansPurchased_ = 0;
    barriersLost_ = 0;
    spent_ = 0;
    result_ = {};
    walkers_.clear();
    gadgets_.clear();
    events_.clear();
}

void GameModel::resetSession()
{
    totalHeldOff_ = 0;
    totalGaveUp_ = 0;
    totalTrips_ = 0;
}

void GameModel::enterIntro()
{
    resetLevel();
    phase_ = Phase::Intro;
    feedback_.clear();
}

void GameModel::enterPrep(const char* lead)
{
    phase_ = Phase::Prep;
    feedback_ = std::string(lead) + " DEFEND " + upper(level().target) + "!";
}

void GameModel::backToTitle()
{
    const Progress keep = progress_;
    const bool changed = progressChanged_;
    *this = GameModel{};
    progress_ = keep;
    progressChanged_ = changed;
}

void GameModel::newGame(int index)
{
    backToTitle();
    levelIndex_ = std::clamp(index, 0, levelCount - 1);
    enterIntro();
}

void GameModel::openMap()
{
    resetLevel();
    phase_ = Phase::Map;
    feedback_.clear();
}

bool GameModel::selectLevel(int index)
{
    if (phase_ != Phase::Map || index < 0 || index >= levelCount || !progress_.levelUnlocked(index))
        return false;
    levelIndex_ = index;
    enterIntro();
    return true;
}

void GameModel::openShop()
{
    if (phase_ != Phase::Map && phase_ != Phase::Won)
        return;
    shopFrom_ = phase_;
    phase_ = Phase::Shop;
    feedback_.clear();
}

void GameModel::closeShop()
{
    if (phase_ != Phase::Shop)
        return;
    feedback_.clear();
    if (shopFrom_ != Phase::Won)
    {
        phase_ = Phase::Map;
        return;
    }
    shopFrom_ = Phase::Map;
    if (lastLevel())
        phase_ = Phase::Finished;
    else
    {
        ++levelIndex_;
        enterIntro();
    }
}

void GameModel::openWardrobe()
{
    if (phase_ == Phase::Map)
    {
        phase_ = Phase::Wardrobe;
        feedback_.clear();
    }
}

void GameModel::closeWardrobe()
{
    if (phase_ == Phase::Wardrobe)
        phase_ = Phase::Map;
}

bool GameModel::buyUpgrade(int index)
{
    if (phase_ != Phase::Shop || index < 0 || index >= upgradeCount)
        return false;
    const UpgradeDef& upgrade = upgrades[index];
    if (progress_.nextTierCost(index) == 0)
        return reject(std::string(upgrade.name) + " IS FULLY UPGRADED.");
    if (!progress_.canBuy(index))
        return reject("NOT ENOUGH POINTS. EARN STARS TO GET MORE.");
    ++progress_.tiers[index];
    progressChanged_ = true;
    feedback_ = std::string(upgrade.name) + ": " + upgradeEffectText(upgrade, progress_.tiers[index]) + ".";
    return true;
}

void GameModel::refundUpgrades()
{
    if (phase_ != Phase::Shop || progress_.pointsSpent() == 0)
        return;
    progress_.tiers = {};
    progressChanged_ = true;
    feedback_ = "ALL POINTS REFUNDED. NO QUESTIONS ASKED.";
}

bool GameModel::equipHat(int index)
{
    if (index < 0 || index >= hatCount || !progress_.unlocked(hats[index].unlock))
        return false;
    if (progress_.hat != index)
        progressChanged_ = true;
    progress_.hat = index;
    return true;
}

bool GameModel::equipCone(int index)
{
    if (index < 0 || index >= coneCount || !progress_.unlocked(cones[index].unlock))
        return false;
    if (progress_.cone != index)
        progressChanged_ = true;
    progress_.cone = index;
    return true;
}

void GameModel::setProgress(const Progress& progress)
{
    progress_ = progress;
    progress_.sanitize();
}

bool GameModel::takeProgressChanged()
{
    return std::exchange(progressChanged_, false);
}

int GameModel::frontierLevel() const
{
    for (int index = 0; index < levelCount; ++index)
        if (progress_.stars[index] == 0)
            return index;
    for (int index = 0; index < levelCount; ++index)
        if (progress_.stars[index] < maxStarsPerLevel)
            return index;
    return 0;
}

bool GameModel::goalOnTrack(int index) const
{
    if (index < 0 || index >= goalsPerLevel)
        return false;
    const GoalDef& goal = level().goals[static_cast<std::size_t>(index)];
    switch (goal.type)
    {
    case GoalType::NoBarriersLost:
        return barriersLost_ == 0;
    case GoalType::BudgetLeft:
        return budget_ >= static_cast<float>(goal.amount);
    case GoalType::MaxSpend:
        return spent_ <= goal.amount;
    case GoalType::MoodBelow:
        return peakMood_ / config::maximumMood * 100.0f < static_cast<float>(goal.amount);
    case GoalType::NoVans:
        return vansPurchased_ == 0;
    }
    return false;
}

void GameModel::beginLevel()
{
    if (phase_ == Phase::Intro)
        enterPrep("SET UP YOUR LINE.");
}

void GameModel::nextLevel()
{
    if (phase_ != Phase::Won)
        return;
    if (lastLevel())
    {
        phase_ = Phase::Finished;
        feedback_.clear();
    }
    else if (progress_.anyAffordable())
        openShop();
    else
    {
        ++levelIndex_;
        enterIntro();
    }
}

void GameModel::retryLevel()
{
    if (phase_ != Phase::Lost)
        return;
    resetLevel();
    enterPrep("TRY AGAIN. BUILD A BETTER LINE.");
}

void GameModel::ready()
{
    if (phase_ == Phase::Prep || phase_ == Phase::Intermission)
        startWave();
}

void GameModel::startWave()
{
    ++waveIndex_;
    phase_ = Phase::Wave;
    remaining_ = level().waves[waveIndex_].seconds;
    spawned_ = 0;
    spawnTimer_ = 0.0f;
    brickTimer_ = throwGap(level().bricksPerMinute);
    smokeTimer_ = throwGap(level().smokeBombsPerMinute);
    feedback_ = "HOLD UNTIL THE WHISTLE! REPAIR WITH [4] OR RIGHT CLICK.";
    events_.push_back({EventType::WaveStart, config::goalX, 0});
}

void GameModel::finishWave()
{
    for (const auto& walker : walkers_)
    {
        // Walkers who already gave up were counted when they turned round.
        if (!walker.leaving)
            ++heldOff_;
        events_.push_back({EventType::Disperse, walker.x, walker.lane, GadgetType::Barrier, walker.id});
    }
    walkers_.clear();
    events_.push_back({EventType::WaveEnd, config::goalX, 0});
    if (waveIndex_ + 1 == waveCount())
        completeLevel();
    else
    {
        phase_ = Phase::Intermission;
        remaining_ = config::betweenWavesSeconds;
        feedback_ = "TEA BREAK! REPAIR YOUR LINE BEFORE THE NEXT CROWD.";
    }
}

void GameModel::completeLevel()
{
    phase_ = Phase::Won;
    remaining_ = 0.0f;
    totalHeldOff_ += heldOff_;
    totalGaveUp_ += gaveUp_;
    totalTrips_ += trips_;
    feedback_ = "THE CROWD HAS GONE HOME. LEVEL CLEARED!";

    result_ = {};
    result_.previousBest = progress_.stars[levelIndex_];
    unsigned goalBits = 0;
    int met = 0;
    for (int i = 0; i < goalsPerLevel; ++i)
    {
        result_.goals[i] = goalOnTrack(i);
        if (result_.goals[i])
        {
            goalBits |= 1u << i;
            ++met;
        }
    }
    result_.stars = starsForGoals[met];

    std::array<bool, hatCount> hadHat{};
    std::array<bool, coneCount> hadCone{};
    for (int i = 0; i < hatCount; ++i)
        hadHat[i] = progress_.unlocked(hats[i].unlock);
    for (int i = 0; i < coneCount; ++i)
        hadCone[i] = progress_.unlocked(cones[i].unlock);
    const int pointsBefore = progress_.pointsEarned();
    progress_.record(levelIndex_, result_.stars, goalBits);
    result_.pointsEarned = progress_.pointsEarned() - pointsBefore;
    for (int i = 0; i < hatCount; ++i)
        if (!hadHat[i] && progress_.unlocked(hats[i].unlock))
            result_.unlocked.push_back(std::string("HAT: ") + hats[i].name);
    for (int i = 0; i < coneCount; ++i)
        if (!hadCone[i] && progress_.unlocked(cones[i].unlock))
            result_.unlocked.push_back(std::string("CONE: ") + cones[i].name);
    progressChanged_ = true;
}

void GameModel::failLevel()
{
    phase_ = Phase::Lost;
    feedback_ = "THE CROWD HAS REACHED " + upper(level().target) + ".";
}

void GameModel::debugWin()
{
    if (!canBuild())
        return;
    waveIndex_ = waveCount() - 1;
    phase_ = Phase::Wave;
    finishWave();
}

void GameModel::debugLose()
{
    if (!canBuild())
        return;
    mood_ = config::maximumMood;
    peakMood_ = mood_;
    failLevel();
}

const Gadget* GameModel::gadgetAt(int lane, int column) const
{
    const auto it = std::find_if(gadgets_.begin(), gadgets_.end(), [=](const Gadget& gadget) {
        return gadget.lane == lane && gadget.column == column;
    });
    return it == gadgets_.end() ? nullptr : &*it;
}

const Walker* GameModel::findWalker(int id) const
{
    const auto it = std::find_if(walkers_.begin(), walkers_.end(), [=](const Walker& walker) {
        return walker.id == id;
    });
    return it == walkers_.end() ? nullptr : &*it;
}

std::string GameModel::placementError(GadgetType type, int lane, int column) const
{
    if (!canBuild())
        return "WAIT FOR THE NEXT LEVEL.";
    if (lane < 0 || lane >= config::lanes || column < 0 || column >= config::columns)
        return "PLACE ON A STREET TILE.";
    if (gadgetAt(lane, column))
        return "TILE OCCUPIED. USE [4] TO REPAIR.";
    if (type == GadgetType::Van && vansPurchased_ >= maxVans())
        return "ONLY " + std::to_string(maxVans()) + (maxVans() == 1 ? " VAN" : " VANS") +
               " PER LEVEL. EVEN IF THEY BREAK.";
    if (budget_ < static_cast<float>(cost(type)))
        return "NOT ENOUGH CASH. BUDGET REFILLS DURING WAVES.";
    const float space = stats(type).halfWidth + config::walkerSpacing * 0.5f + 4.0f;
    for (const auto& walker : walkers_)
        if (!walker.leaving && walker.lane == lane && std::abs(walker.x - columnX(column)) < space)
            return "CROWD IN THE WAY! TRY AN EMPTY TILE.";
    return {};
}

bool GameModel::reject(std::string message)
{
    feedback_ = std::move(message);
    return false;
}

bool GameModel::place(GadgetType type, int lane, int column)
{
    auto error = placementError(type, lane, column);
    if (!error.empty())
        return reject(std::move(error));
    budget_ -= static_cast<float>(cost(type));
    spent_ += cost(type);
    gadgets_.push_back({nextId_++, type, lane, column, maxHealth(type)});
    if (type == GadgetType::Van)
        ++vansPurchased_;
    events_.push_back({EventType::Placed, columnX(column), lane, type});
    feedback_ = std::string(stats(type).name) + " DEPLOYED. ABSOLUTELY OFFICIAL.";
    return true;
}

bool GameModel::repair(int lane, int column)
{
    if (!canBuild())
        return reject("WAIT FOR THE NEXT LEVEL.");
    auto it = std::find_if(gadgets_.begin(), gadgets_.end(), [=](const Gadget& gadget) {
        return gadget.lane == lane && gadget.column == column;
    });
    if (it == gadgets_.end())
        return reject("NO GADGET HERE TO REPAIR.");
    if (it->type == GadgetType::Cone)
        return reject("CONES ARE SINGLE USE. A NOBLE CALLING.");
    if (it->health >= maxHealth(it->type) - 0.01f)
        return reject("ALREADY IN TIP-TOP CONDITION.");
    if (budget_ < static_cast<float>(repairCost()))
        return reject("REPAIR NEEDS $" + std::to_string(repairCost()) + ".");
    budget_ -= static_cast<float>(repairCost());
    spent_ += repairCost();
    it->health = std::min(maxHealth(it->type), it->health + repairHealth());
    events_.push_back({EventType::Repaired, columnX(column), lane, it->type});
    feedback_ = "A BIT OF TAPE. GOOD AS NEW-ISH.";
    return true;
}

void GameModel::spawnWalker(int lane, float x)
{
    Walker walker{nextId_++, lane, x};
    walker.look = random();
    const float spread = static_cast<float>(random() & 0xffffu) / 65535.0f * 2.0f - 1.0f;
    walker.maxPatience = config::patienceSeconds * (1.0f + config::patienceSpread * spread);
    walker.patience = walker.maxPatience;
    walkers_.push_back(walker);
}

void GameModel::moveWalkers(float dt)
{
    std::vector<std::size_t> order(walkers_.size());
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
        return walkers_[a].x > walkers_[b].x;
    });
    std::array<Walker*, config::lanes> front{};
    std::vector<float> damage(gadgets_.size(), 0.0f);
    for (auto index : order)
    {
        auto& walker = walkers_[index];
        if (walker.leaving)
        {
            // Going home: straight back off the left edge, through anyone still marching.
            walker.x -= config::leaveSpeed * dt;
            walker.gone = walker.x < config::spawnX - 30.0f;
            continue;
        }

        walker.blockingGadget = -1;
        const bool stumbling = walker.stumble > 0.0f;
        walker.stumble = std::max(0.0f, walker.stumble - dt);
        float next = walker.x + (stumbling ? 0.0f : config::walkerSpeed * dt);

        auto* ahead = front[walker.lane];
        if (ahead && next >= ahead->x - config::walkerSpacing)
        {
            next = std::max(walker.x, ahead->x - config::walkerSpacing);
            walker.blockingGadget = ahead->blockingGadget;
        }

        const Gadget* contact = nullptr;
        for (auto& gadget : gadgets_)
        {
            if (gadget.lane != walker.lane || gadget.health <= 0.0f)
                continue;
            const float stop = columnX(gadget.column) - stats(gadget.type).halfWidth
                               - config::walkerSpacing * 0.5f;
            if (walker.x <= stop + 0.05f && next >= stop)
            {
                next = stop;
                if (gadget.type == GadgetType::Cone)
                {
                    gadget.health = 0.0f;
                    walker.stumble = config::stumbleSeconds;
                    walker.carryingCone = true;
                    ++trips_;
                    events_.push_back({EventType::Trip, walker.x, walker.lane, gadget.type, walker.id});
                }
                else
                {
                    walker.blockingGadget = gadget.id;
                    contact = &gadget;
                }
            }
        }
        if (contact && !walker.pressing)
            events_.push_back({EventType::Bump, next, walker.lane, contact->type, walker.id});
        walker.pressing = contact != nullptr;

        walker.x = next;
        if (walker.blockingGadget >= 0)
        {
            walker.blockedTime += dt;
            for (std::size_t i = 0; i < gadgets_.size(); ++i)
                if (gadgets_[i].id == walker.blockingGadget)
                    damage[i] += config::pushDamagePerSecond * dt;
            if (walker.stumble <= 0.0f)
            {
                walker.patience -= level().patienceDrain * dt;
                if (!walker.bored && walker.patience < config::boredAt * walker.maxPatience)
                {
                    walker.bored = true;
                    events_.push_back({EventType::Bored, walker.x, walker.lane, GadgetType::Barrier, walker.id});
                }
                if (walker.patience <= 0.0f)
                {
                    walker.leaving = true;
                    walker.pressing = false;
                    walker.blockingGadget = -1;
                    ++heldOff_;
                    ++gaveUp_;
                    events_.push_back({EventType::GiveUp, walker.x, walker.lane, GadgetType::Barrier, walker.id});
                    continue;
                }
            }
        }
        else
            walker.blockedTime = 0.0f;

        if (!walker.nearTarget && walker.x > config::dangerX - config::nearTargetMargin)
        {
            walker.nearTarget = true;
            events_.push_back({EventType::NearTarget, walker.x, walker.lane, GadgetType::Barrier, walker.id});
        }
        if (walker.x >= config::goalX)
        {
            walker.gone = true;
            mood_ += config::arrivalMood;
            ++reached_;
            events_.push_back({EventType::Breach, config::goalX, walker.lane, GadgetType::Barrier, walker.id});
        }
        else
            front[walker.lane] = &walker;
    }

    for (std::size_t i = 0; i < gadgets_.size(); ++i)
    {
        auto& gadget = gadgets_[i];
        const float oldHealth = gadget.health;
        gadget.health -= damage[i];
        if (oldHealth > 0.0f && gadget.health <= 0.0f)
        {
            if (isWall(gadget.type))
                ++barriersLost_;
            events_.push_back({EventType::Break, columnX(gadget.column), gadget.lane, gadget.type});
        }
        else if (static_cast<int>(oldHealth / 65.0f) > static_cast<int>(gadget.health / 65.0f))
            events_.push_back({EventType::Buckle, columnX(gadget.column), gadget.lane, gadget.type});
    }
    std::erase_if(gadgets_, [](const Gadget& gadget) { return gadget.health <= 0.0f; });
    std::erase_if(walkers_, [](const Walker& walker) { return walker.gone; });
}

void GameModel::throwThings(float dt)
{
    const auto attempt = [&](float& timer, float perMinute, Throwable kind) {
        if (perMinute <= 0.0f)
            return;
        timer -= dt;
        // With nobody in a position to throw, try again next tick (without saving throws up for later).
        if (timer <= 0.0f)
            timer = throwOne(kind) ? throwGap(perMinute) : 0.0f;
    };
    attempt(brickTimer_, level().bricksPerMinute, Throwable::Brick);
    attempt(smokeTimer_, level().smokeBombsPerMinute, Throwable::SmokeBomb);
}

// Picks someone in the crowd to lob something forward. Only an event: nothing is damaged or slowed down.
bool GameModel::throwOne(Throwable kind)
{
    std::vector<const Walker*> throwers;
    for (const Walker& walker : walkers_)
        if (!walker.leaving && walker.stumble <= 0.0f && walker.x >= config::throwFromX &&
            walker.x <= config::dangerX - config::nearTargetMargin)
            throwers.push_back(&walker);
    if (throwers.empty())
        return false;
    const float roll = throwRoll() * static_cast<float>(throwers.size());
    const Walker& thrower = *throwers[std::min(throwers.size() - 1, static_cast<std::size_t>(roll))];
    Event event{EventType::Throw, thrower.x, thrower.lane, GadgetType::Barrier, thrower.id};
    event.thrown = kind;
    event.landX = thrower.x + config::throwShortest + (config::throwLongest - config::throwShortest) * throwRoll();
    if (kind == Throwable::Brick)
    {
        // Bricks are lobbed at the nearest barrier or van in range (and bounce off it). The front row is too close
        // to aim, so their bricks sail over.
        float nearest = config::throwReach;
        for (const Gadget& gadget : gadgets_)
        {
            const float front = columnX(gadget.column) - stats(gadget.type).halfWidth;
            const float distance = front - thrower.x;
            if (gadget.lane == thrower.lane && isWall(gadget.type) && gadget.health > 0.0f &&
                distance >= config::throwShortest * 0.5f && distance <= nearest)
            {
                nearest = distance;
                event.landX = front;
                event.column = gadget.column;
            }
        }
        ++bricksThrown_;
    }
    else
        ++smokeBombsThrown_;
    event.landX = std::min(event.landX, config::goalX - config::throwShortest * 0.5f);
    events_.push_back(event);
    return true;
}

void GameModel::tick(float dt)
{
    if (!canBuild() || !std::isfinite(dt) || dt <= 0.0f)
        return;

    remaining_ = std::max(0.0f, remaining_ - dt);
    if (phase_ != Phase::Wave)
    {
        mood_ = std::max(0.0f, mood_ - config::moodRecoveryPerSecond * dt);
        if (remaining_ <= 0.0f)
            startWave();
        return;
    }

    budget_ = std::min(config::budgetCap, budget_ + config::budgetPerSecond * dt);
    spawnTimer_ -= dt;
    const auto& wave = level().waves[waveIndex_];
    while (spawnTimer_ <= 0.0f && spawned_ < wave.walkers)
    {
        const int lane = (spawned_ + waveIndex_) % config::lanes;
        float spawn = config::spawnX;
        for (const auto& walker : walkers_)
            if (walker.lane == lane && !walker.leaving)
                spawn = std::min(spawn, walker.x - config::walkerSpacing);
        spawnWalker(lane, spawn);
        ++spawned_;
        spawnTimer_ += wave.spawnWindow / static_cast<float>(wave.walkers);
    }

    moveWalkers(dt);
    throwThings(dt);
    bool danger = false;
    for (const auto& walker : walkers_)
    {
        if (!walker.leaving && walker.x > config::dangerX)
        {
            danger = true;
            const float proximity = (walker.x - config::dangerX) / (config::goalX - config::dangerX);
            mood_ += config::proximityMoodPerSecond * proximity * dt;
        }
    }
    if (!danger)
        mood_ -= config::moodRecoveryPerSecond * dt;
    mood_ = std::clamp(mood_, 0.0f, config::maximumMood);
    peakMood_ = std::max(peakMood_, mood_);
    if (mood_ >= config::maximumMood)
        failLevel();
    else if (remaining_ <= 0.0f)
        finishWave();
}

std::vector<Event> GameModel::takeEvents()
{
    auto result = std::move(events_);
    events_.clear();
    return result;
}
}
