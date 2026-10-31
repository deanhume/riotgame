#pragma once

#include "GameConfig.h"
#include "DefendModel.h"
#include "Levels.h"
#include "Progress.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace riot
{
// Each pair: Intro -> Prep -> (Wave <-> Intermission) -> Won -> DefendIntro -> Defend -> DefendWon
// -> [Shop] -> next pair (or Finished after the sixth defence). A loss retries only that part of the pair.
enum class Phase
{
    Title, Map, Shop, Wardrobe, Intro, Prep, Wave, Intermission, Won, Lost,
    DefendIntro, Defend, DefendWon, DefendLost, Finished
};
enum class GadgetType { Cone, Barrier, Van };
enum class Throwable { Brick, SmokeBomb };
enum class EventType
{
    Placed,
    Repaired,
    Trip,        // a walker fell over a cone (and pinched it)
    Bump,        // a walker walked into a barrier or van
    NearTarget,  // a walker got close enough to smell victory
    Bored,       // a walker's patience is running low
    GiveUp,      // a walker has had enough and is going home
    Throw,       // a walker lobbed a brick or a smoke bomb (just for show: they never do any damage)
    Buckle,
    Break,
    Breach,
    Disperse,
    WaveStart,
    WaveEnd,
};

struct Gadget
{
    int id;
    GadgetType type;
    int lane;
    int column;
    float health;
};

struct Walker
{
    int id;
    int lane;
    float x;
    std::uint32_t look = 0;  // random bits for outfit, prop and voice; fixed for the walker's life
    float patience = config::patienceSeconds;
    float maxPatience = config::patienceSeconds;
    float stumble = 0.0f;
    float blockedTime = 0.0f;
    int blockingGadget = -1;   // the gadget holding this walker up, directly or through the queue
    bool pressing = false;     // shoving the gadget directly (front of the queue)
    bool bored = false;
    bool leaving = false;      // given up: walking back off the left edge, ignoring everything
    bool nearTarget = false;
    bool carryingCone = false; // tripped over a cone and kept it
    bool gone = false;
};

struct Event
{
    EventType type;
    float x;
    int lane;
    GadgetType gadget = GadgetType::Barrier;
    int walker = -1;
    // Throw only: what was thrown, where it comes down, and the column of the wall it is aimed at (-1: the road).
    Throwable thrown = Throwable::Brick;
    float landX = 0.0f;
    int column = -1;
};

struct LevelResult
{
    int stars = 0;
    std::array<bool, goalsPerLevel> goals{};
    int previousBest = 0;
    int pointsEarned = 0;
    std::vector<std::string> unlocked;  // cosmetics this win unlocked
};

class GameModel
{
public:
    // Developer and test path: jumps straight to the intro card of a 0-based level, ignoring locks.
    // Keeps the saved progress but clears the session totals.
    void newGame(int index = 0, bool defend = false);
    void openMap();
    bool selectLevel(int index);
    void openShop();
    void closeShop();
    void openWardrobe();
    void closeWardrobe();
    bool buyUpgrade(int index);
    void refundUpgrades();
    bool equipHat(int index);
    bool equipCone(int index);
    void beginLevel();
    void ready();
    // Riot win -> defend intro. Defend win -> shop, next riot intro, or the final screen.
    void nextLevel();
    void retryLevel();
    void backToTitle();
    void tick(float dt);
    // Developer shortcuts: end the current level through the normal win / lose path.
    void debugWin();
    void debugLose();
    void raiseShield(bool raised) { defend_.raiseShield(raised); }
    bool swipe(int direction) { return phase_ == Phase::Defend && defend_.swipe(direction); }
    const DefendModel& defence() const { return defend_; }
    std::vector<DefendEvent> takeDefendEvents() { return defend_.takeEvents(); }
    bool defending() const;
    bool playing() const { return canBuild() || phase_ == Phase::Defend; }
    int campaignNumber() const { return levelIndex_ * 2 + (defending() ? 2 : 1); }
    bool place(GadgetType type, int lane, int column);
    bool repair(int lane, int column);
    std::string placementError(GadgetType type, int lane, int column) const;
    const Gadget* gadgetAt(int lane, int column) const;
    const Walker* findWalker(int id) const;
    std::vector<Event> takeEvents();

    const Progress& progress() const { return progress_; }
    // Replaces the saved progress (after loading). Sanitised; does not count as a change to save.
    void setProgress(const Progress& progress);
    // True once after anything that should be saved.
    bool takeProgressChanged();
    const LevelResult& result() const { return result_; }
    // The level the map suggests: the first one not yet cleared, else the first without three stars.
    int frontierLevel() const;
    // Whether bonus goal `index` of the current level is met so far (or was met, once the level is over).
    bool goalOnTrack(int index) const;

    // Prices and health after upgrades.
    int cost(GadgetType type) const;
    float maxHealth(GadgetType type) const;
    int repairCost() const;
    float repairHealth() const;
    float startingBudget() const;

    Phase phase() const { return phase_; }
    // Where the shop was opened: DefendWon (on to the next riot) or Map.
    Phase shopFrom() const { return shopFrom_; }
    int levelIndex() const { return levelIndex_; }
    const LevelDef& level() const { return levels[static_cast<std::size_t>(levelIndex_)]; }
    bool lastLevel() const { return levelIndex_ + 1 == levelCount; }
    int waveCount() const { return level().waves.size(); }
    int maxVans() const { return level().maxVans; }
    float budget() const { return budget_; }
    float mood() const { return mood_; }
    float peakMood() const { return peakMood_; }
    float remaining() const { return remaining_; }
    int waveIndex() const { return waveIndex_; }
    // Per-level counters; reset whenever a level is (re)started.
    int heldOff() const { return heldOff_; }
    int gaveUp() const { return gaveUp_; }
    int reached() const { return reached_; }
    int trips() const { return trips_; }
    int vansPurchased() const { return vansPurchased_; }
    int barriersLost() const { return barriersLost_; }
    int spent() const { return spent_; }
    int bricksThrown() const { return bricksThrown_; }
    int smokeBombsThrown() const { return smokeBombsThrown_; }
    // Session totals over the levels cleared since the title screen.
    int totalHeldOff() const { return totalHeldOff_; }
    int totalGaveUp() const { return totalGaveUp_; }
    int totalTrips() const { return totalTrips_; }
    const std::string& feedback() const { return feedback_; }
    const std::vector<Walker>& walkers() const { return walkers_; }
    const std::vector<Gadget>& gadgets() const { return gadgets_; }
    bool canBuild() const;
    static float columnX(int column);
    static const config::GadgetStats& stats(GadgetType type);

private:
    void resetLevel();
    void resetSession();
    void enterIntro();
    void enterDefend();
    void finishDefend();
    void advanceCampaign();
    void enterPrep(const char* lead);
    void startWave();
    void finishWave();
    void completeLevel();
    void failLevel();
    void moveWalkers(float dt);
    void throwThings(float dt);
    bool throwOne(Throwable kind);
    float throwGap(float perMinute);
    float throwRoll();
    void spawnWalker(int lane, float x);
    std::uint32_t random();
    bool reject(std::string message);

    Phase phase_ = Phase::Title;
    Phase shopFrom_ = Phase::Map;
    int levelIndex_ = 0;
    float budget_ = config::startingBudget;
    float mood_ = 0.0f;
    float peakMood_ = 0.0f;
    float remaining_ = config::prepSeconds;
    float spawnTimer_ = 0.0f;
    int waveIndex_ = -1;
    int spawned_ = 0;
    int nextId_ = 0;
    std::uint32_t seed_ = 1;
    // Thrown things roll their own dice, so tuning them never changes the crowd or the balance.
    std::uint32_t throwSeed_ = 1;
    float brickTimer_ = 0.0f;
    float smokeTimer_ = 0.0f;
    int bricksThrown_ = 0;
    int smokeBombsThrown_ = 0;
    int heldOff_ = 0;
    int gaveUp_ = 0;
    int reached_ = 0;
    int trips_ = 0;
    int vansPurchased_ = 0;
    int barriersLost_ = 0;
    int spent_ = 0;
    int totalHeldOff_ = 0;
    int totalGaveUp_ = 0;
    int totalTrips_ = 0;
    Progress progress_;
    DefendModel defend_;
    bool progressChanged_ = false;
    LevelResult result_;
    std::string feedback_;
    std::vector<Walker> walkers_;
    std::vector<Gadget> gadgets_;
    std::vector<Event> events_;
};
}
