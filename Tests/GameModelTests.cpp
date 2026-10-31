#include "GameModel.h"
#include "IntroSequence.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

using namespace riot;

void check(bool condition, const std::string& message)
{
    if (!condition)
        throw std::runtime_error(message);
}

void advance(GameModel& game, float seconds)
{
    for (int i = 0; i < static_cast<int>(std::ceil(seconds / config::fixedStep)); ++i)
    {
        game.tick(config::fixedStep);
        game.takeEvents();
    }
}

// Title -> intro card -> prep for the given (0-based) level.
void begin(GameModel& game, int level = 0)
{
    game.newGame(level);
    game.beginLevel();
}

int crowdSize(const LevelDef& level)
{
    int total = 0;
    for (int i = 0; i < level.waves.size(); ++i)
        total += level.waves[i].walkers;
    return total;
}

struct BotResult
{
    float peakMood = 0.0f;
    int gaveUp = 0;
    int barriersLost = 0;
    int spent = 0;
    float budgetLeft = 0.0f;
};

// A simple player: three barriers in one column, replaced or repaired every tick until the whistle is close.
// It never uses vans or cones, so it is a floor for what a real player can do.
BotResult defend(GameModel& game, int column = 5, bool repairs = true, bool careful = false)
{
    check(game.phase() == Phase::Prep, "The bot starts in prep");
    for (int lane = 0; lane < config::lanes; ++lane)
        check(game.place(GadgetType::Barrier, lane, column), "Initial three-lane defense");
    game.ready();
    BotResult result;
    for (int tick = 0; tick < 60 * 280 && game.canBuild(); ++tick)
    {
        const float runToGoal = (config::goalX - GameModel::columnX(column) + config::gadgets[1].halfWidth +
                                 config::walkerSpacing * 0.5f) / config::walkerSpeed;
        const bool mustHold = careful || game.phase() != Phase::Wave || game.remaining() > runToGoal + 0.5f;
        for (int offset = 0; offset < config::lanes && mustHold && repairs; ++offset)
        {
            const int lane = (offset + tick) % config::lanes;
            const auto* barrier = game.gadgetAt(lane, column);
            if (!barrier)
                game.place(GadgetType::Barrier, lane, column);
            else if (barrier->health <= game.maxHealth(GadgetType::Barrier) - game.repairHealth())
                game.repair(lane, column);
        }
        game.tick(config::fixedStep);
        game.takeEvents();
        result.peakMood = std::max(result.peakMood, game.mood());
    }
    result.gaveUp = game.gaveUp();
    result.barriersLost = game.barriersLost();
    result.spent = game.spent();
    result.budgetLeft = game.budget();
    return result;
}

bool contains(const std::vector<std::string>& list, const std::string& item)
{
    return std::find(list.begin(), list.end(), item) != list.end();
}

void clearDefend(GameModel& game)
{
    check(game.phase() == Phase::DefendIntro, "A riot is followed by a defend intro");
    game.beginLevel();
    int direction = 1;
    for (int tick = 0; tick < 60 * 100 && game.phase() == Phase::Defend; ++tick)
    {
        if (game.defence().inReach())
        {
            check(game.swipe(direction), "A correctly timed baton swipe is ready");
            direction = -direction;
        }
        game.tick(config::fixedStep);
        game.takeDefendEvents();
    }
    check(game.phase() == Phase::DefendWon && game.defence().lives() == defendConfig::lives,
          "Timed baton swipes can win every defend level without losing lives");
    check(game.defence().sliced() == game.defence().totalThrows(), "Every throw can be sliced, including smoke");
}

int main()
{
    try
    {
        IntroSequence opening;
        check(opening.active() && opening.scene().scene == intro::Scene::Studio,
              "Launch intro starts in the fictional studio");
        check(std::string(opening.caption()) == intro::scenes[0].captions[0].text,
              "The first presenter caption appears immediately");
        opening.tick(intro::scenes[0].captions[1].start);
        check(std::string(opening.caption()) == intro::scenes[0].captions[1].text,
              "The second caption replaces the first without overlap");
        opening.tick(-1);
        opening.tick(std::numeric_limits<float>::quiet_NaN());
        check(opening.sceneTime() == intro::scenes[0].captions[1].start,
              "Invalid intro deltas do not corrupt the timeline");
        opening.tick(intro::scenes[0].seconds - opening.sceneTime());
        check(opening.scene().scene == intro::Scene::Crowd && opening.sceneTime() == 0,
              "The crowd scene starts at the studio boundary");
        check(opening.cameraPosition() == 1.0f, "Camera arrives at the crowd without resetting to the studio");
        opening.tick(intro::scenes[1].seconds);
        check(opening.scene().scene == intro::Scene::Police, "The calm police follow the crowd");
        opening.tick(intro::scenes[2].seconds);
        check(opening.scene().scene == intro::Scene::Title, "The title card follows the police");
        opening.tick(intro::scenes[3].seconds);
        check(!opening.active(), "The whole intro completes automatically");
        opening.tick(100);
        check(!opening.active() && std::string(opening.caption()).empty(),
              "A completed intro cannot animate or display captions");
        IntroSequence fastForward;
        fastForward.tick(intro::duration() + 1);
        check(!fastForward.active(), "A large tick safely crosses multiple scene boundaries");
        IntroSequence camera;
        for (std::size_t i = 0; i + 1 < intro::scenes.size(); ++i)
        {
            const auto& scene = intro::scenes[i];
            camera.tick(scene.seconds - scene.panSeconds);
            check(std::abs(camera.cameraPosition() - static_cast<float>(i)) < 0.0001f,
                  "Each panel holds still long enough to read before panning");
            camera.tick(scene.panSeconds * 0.5f);
            check(std::abs(camera.pan() - 0.5f) < 0.0001f, "The camera eases halfway between adjacent panels");
            camera.tick(scene.panSeconds * 0.5f);
            check(std::abs(camera.cameraPosition() - static_cast<float>(i + 1)) < 0.0001f,
                  "Each pan ends exactly where the next panel begins");
        }
        camera.tick(camera.scene().seconds - intro::menuWipeSeconds * 0.5f);
        check(camera.menuWipe() > 0.49f && camera.menuWipe() < 0.51f,
              "The title wipes to the menu only at the end of the sequence");
        camera.skip();
        check(camera.menuWipe() == 0.0f && camera.pan() == 0.0f,
              "Skipping bypasses transitions immediately");
        float introOffset = 0;
        for (const auto& scene : intro::scenes)
        {
            IntroSequence skipped;
            skipped.tick(introOffset + scene.seconds * 0.5f);
            check(skipped.active() && skipped.scene().scene == scene.scene, "Each intro scene can be reached");
            skipped.skip();
            const float stopped = skipped.sceneTime();
            skipped.tick(100);
            skipped.skip();
            check(!skipped.active() && skipped.sceneTime() == stopped && std::string(skipped.caption()).empty(),
                  "Skip immediately ends every scene and cannot restart the intro");
            introOffset += scene.seconds;
        }

        DefendModel shield;
        shield.reset(0);
        shield.tick(30);
        check(!shield.incoming() && shield.resolved() == 0, "Defend intro waits for the player");
        shield.begin();
        shield.raiseShield(true);
        bool broken = false;
        for (int tick = 0; tick < 60 * 40 && shield.phase() == DefendPhase::Active; ++tick)
        {
            shield.tick(config::fixedStep);
            if (shield.blocked() == defendConfig::shieldBlocks && !broken)
            {
                broken = true;
                check(shield.shieldHealth() == 0 && !shield.shielding() && shield.lives() == 3,
                      "The fifth block breaks the shield but still protects the officer");
            }
        }
        check(broken && shield.phase() == DefendPhase::Lost && shield.lives() == 0,
              "Holding a broken shield cannot stop three missed bricks");
        const int stoppedAt = shield.resolved();
        shield.tick(10);
        check(shield.resolved() == stoppedAt && !shield.swipe(1), "A finished defence freezes gameplay");

        DefendModel timing;
        timing.reset(0);
        timing.begin();
        while (!timing.incoming())
            timing.tick(config::fixedStep);
        check(timing.swipe(-1) && !timing.swipe(1), "Baton swipes have a recovery cooldown");
        while (timing.resolved() == 0)
            timing.tick(config::fixedStep);
        check(timing.lives() == 2 && timing.sliced() == 0, "An early swipe does not protect against a later brick");
        while (!timing.incoming() || !timing.inReach())
            timing.tick(config::fixedStep);
        timing.raiseShield(true);
        check(timing.swipe(1) && timing.sliced() == 1 && timing.shieldHealth() == defendConfig::shieldBlocks,
              "A timed swipe cuts the brick without using shield health");
        check(!timing.shielding(), "A swing lowers the shield even when Space is held");

        DefendModel smoke;
        smoke.reset(0);
        smoke.begin();
        for (int tick = 0; tick < 60 * 25 && smoke.resolved() < 5; ++tick)
        {
            smoke.raiseShield(smoke.incoming() && smoke.incoming()->kind == DefendThing::Brick);
            smoke.tick(config::fixedStep);
        }
        check(smoke.resolved() == 5 && smoke.smoke() > 0 && smoke.lives() == 3,
              "Missing a smoke bomb obscures the view but does not cost a life");
        smoke.raiseShield(true);
        for (int tick = 0; tick < 60 * 4; ++tick)
            smoke.tick(config::fixedStep);
        check(smoke.smoke() == 0, "Smoke clears after a limited time");
        smoke.reset(5);
        check(smoke.lives() == 3 && smoke.shieldHealth() == defendConfig::shieldBlocks &&
                  smoke.smoke() == 0 && smoke.resolved() == 0 && !smoke.incoming(),
              "Retry starts with a fresh shield, lives and clear view");

        // Level data.
        int previousCrowd = 0;
        for (const auto& level : levels)
        {
            check(level.waves.size() >= 5 && level.waves.size() <= 8, "Each level has five to eight waves");
            for (int i = 1; i < level.waves.size(); ++i)
                check(level.waves[i].walkers > level.waves[i - 1].walkers, "Each wave is bigger than the last");
            check(crowdSize(level) >= previousCrowd, "A later level is never easier than the one before");
            previousCrowd = crowdSize(level);
        }

        // Campaign flow gates.
        GameModel flow;
        check(flow.phase() == Phase::Title, "A new model sits on the title screen");
        check(!flow.place(GadgetType::Barrier, 0, 0), "Cannot build on the title screen");
        flow.ready();
        flow.beginLevel();
        flow.nextLevel();
        flow.retryLevel();
        flow.closeShop();
        check(!flow.selectLevel(0), "Levels are picked on the map, not the title");
        check(flow.phase() == Phase::Title, "Title only leaves through the map or newGame");
        flow.openMap();
        check(flow.phase() == Phase::Map, "Start opens the level map");
        check(!flow.selectLevel(1) && flow.phase() == Phase::Map, "Level 2 is locked until level 1 is cleared");
        check(flow.frontierLevel() == 0, "A fresh save points the map at level 1");
        check(flow.selectLevel(0), "Level 1 is always open");
        check(flow.phase() == Phase::Intro && flow.levelIndex() == 0, "Picking a level opens its intro card");
        check(!flow.place(GadgetType::Barrier, 0, 0), "Cannot build behind an intro card");
        flow.ready();
        flow.nextLevel();
        flow.retryLevel();
        advance(flow, 30);
        check(flow.phase() == Phase::Intro && flow.remaining() == levels[0].prepSeconds,
              "An intro card does not start the prep clock");
        flow.beginLevel();
        check(flow.phase() == Phase::Prep, "Dismissing the intro starts prep");
        flow.nextLevel();
        flow.retryLevel();
        check(flow.phase() == Phase::Prep, "Next and retry do nothing mid-level");
        flow.newGame(99);
        check(flow.levelIndex() == levelCount - 1, "Out-of-range levels are clamped");
        flow.newGame(-3);
        check(flow.levelIndex() == 0, "Negative levels are clamped");
        flow.beginLevel();
        flow.openMap();
        check(flow.phase() == Phase::Map && flow.gadgets().empty() && !flow.canBuild(),
              "Leaving a level for the map abandons it");

        // Original single-level behaviour, level 1.
        GameModel game;
        check(!game.place(GadgetType::Barrier, 0, 0), "Cannot build before the game starts");
        begin(game);
        check(game.place(GadgetType::Barrier, 0, 0), "Prep allows a barrier");
        const float cash = game.budget();
        check(!game.place(GadgetType::Cone, 0, 0), "Occupied cell must reject placement");
        check(!game.repair(0, 0), "Full health repair must not spend cash");
        check(game.budget() == cash, "Rejected actions do not spend budget");
        advance(game, 1);
        check(game.budget() == cash, "Prep does not earn income");
        game.ready();
        advance(game, 8);
        check(game.gadgetAt(0, 0) && game.gadgetAt(0, 0)->health < config::gadgets[1].health,
              "Walkers reach and damage barriers");
        check(game.budget() > cash, "Wave income trickles in");
        const float beforeRepair = game.gadgetAt(0, 0)->health;
        const float repairCash = game.budget();
        check(game.repair(0, 0), "Damaged barrier can be repaired");
        check(std::abs(game.budget() - (repairCash - config::repairCost)) < 0.01f, "Repair charges its exact price");
        check(std::abs(game.gadgetAt(0, 0)->health -
                       std::min(config::gadgets[1].health, beforeRepair + config::repairHealth)) < 0.01f,
              "Repair restores health without exceeding the maximum");
        advance(game, 21);
        check(game.phase() == Phase::Intermission, "Wave ends on its timer");
        check(game.walkers().empty(), "Survivors disperse between waves");
        check(game.heldOff() + game.reached() == levels[0].waves[0].walkers, "Wave totals account for all walkers");

        GameModel unguarded;
        begin(unguarded);
        unguarded.ready();
        advance(unguarded, 240);
        check(unguarded.phase() == Phase::Lost, "An undefended target must lose");
        const float finalBudget = unguarded.budget();
        advance(unguarded, 10);
        check(unguarded.budget() == finalBudget, "Economy freezes on the end screen");
        check(!unguarded.place(GadgetType::Cone, 1, 1), "Cannot place after losing");

        GameModel approaching;
        begin(approaching);
        approaching.ready();
        advance(approaching, 8);
        const float placementCash = approaching.budget();
        check(!approaching.place(GadgetType::Van, 0, 3), "Cannot place an obstacle on a moving walker");
        check(approaching.budget() == placementCash, "Crowd-overlap rejection does not charge");
        advance(approaching, 12);
        check(approaching.mood() > 0 && approaching.reached() == 0, "Proximity raises mood before a breach");
        advance(approaching, 8.1f);
        check(approaching.phase() == Phase::Intermission, "First undefended wave still reaches its whistle");
        const float moodAtWhistle = approaching.mood();
        advance(approaching, 2);
        check(approaching.mood() < moodAtWhistle, "Mood recovers after the danger zone clears");

        GameModel cone;
        begin(cone);
        check(cone.place(GadgetType::Cone, 0, 0), "Cone placement");
        cone.ready();
        advance(cone, 5);
        check(!cone.gadgetAt(0, 0), "Cone is destroyed after one trip");
        check(cone.trips() == 1, "A cone trips only one walker");
        check(cone.heldOff() == 0, "Tripping does not remove or defeat a walker");
        bool stumbling = false;
        for (const auto& walker : cone.walkers())
            stumbling = stumbling || walker.stumble > 0;
        check(stumbling, "Cone causes a temporary stumble");

        GameModel crowd;
        begin(crowd);
        crowd.place(GadgetType::Van, 0, 0);
        crowd.ready();
        advance(crowd, 5);
        const float earlyHealth = crowd.gadgetAt(0, 0)->health;
        advance(crowd, 1);
        const float earlyDamage = earlyHealth - crowd.gadgetAt(0, 0)->health;
        advance(crowd, 6);
        const float lateHealth = crowd.gadgetAt(0, 0)->health;
        advance(crowd, 1);
        const float lateDamage = lateHealth - crowd.gadgetAt(0, 0)->health;
        check(lateDamage > earlyDamage * 1.5f, "A queued crowd damages barriers faster than the first walker");
        for (const auto& walker : crowd.walkers())
        {
            if (walker.lane == 0)
                check(walker.x <= GameModel::columnX(0) - config::gadgets[2].halfWidth,
                      "Walkers must not tunnel through the van");
            if (walker.lane == 0 && walker.blockingGadget >= 0)
                check(!crowd.place(GadgetType::Cone, 0, 0), "Occupied lane cells stay unavailable under pressure");
        }
        check(crowd.place(GadgetType::Van, 1, 8), "A second van can be bought with wave income");
        advance(crowd, 15);
        check(!crowd.place(GadgetType::Van, 2, 8), "A third van is forbidden");
        check(crowd.vansPurchased() == 2, "Van purchase limit is tracked across the level");

        // Losing and retrying.
        GameModel retry;
        begin(retry, 2);
        check(retry.place(GadgetType::Barrier, 1, 4), "Setup before the failed attempt");
        retry.ready();
        advance(retry, 240);
        check(retry.phase() == Phase::Lost, "A thin defence loses level 3");
        check(retry.totalHeldOff() == 0, "A lost level adds nothing to the run totals");
        retry.nextLevel();
        check(retry.phase() == Phase::Lost, "A lost level cannot be skipped");
        retry.retryLevel();
        check(retry.phase() == Phase::Prep && retry.levelIndex() == 2, "Retry replays the same level in prep");
        check(retry.walkers().empty() && retry.gadgets().empty() && retry.waveIndex() == -1,
              "Retry clears the old battlefield");
        check(retry.heldOff() == 0 && retry.reached() == 0 && retry.trips() == 0 && retry.mood() == 0 &&
              retry.budget() == retry.level().startingBudget && retry.vansPurchased() == 0 &&
              retry.remaining() == retry.level().prepSeconds,
              "Retry resets score, mood, economy and purchase limits");

        // Every level can be won by the bot wherever it puts its line (each level resets its own state, so they
        // run independently). Checking several columns keeps the crowd sizes from being tuned to one lucky spot.
        std::string lost;
        std::string unreachable;
        for (int index = 0; index < levelCount; ++index)
        {
            BotResult worst;
            int stars = 0;
            int mostLeft = 0;
            std::array<bool, goalsPerLevel> goalSeen{};
            for (const int column : {4, 5, 6, 7, 9})
            {
                // Column 9 is the careful bot: it keeps repairing right up to the whistle.
                const bool careful = column == 9;
                GameModel solo;
                begin(solo, index);
                const BotResult result = defend(solo, column, true, careful);
                if (solo.phase() == Phase::Won)
                {
                    stars = std::max(stars, solo.result().stars);
                    for (int goal = 0; goal < goalsPerLevel; ++goal)
                        goalSeen[goal] = goalSeen[goal] || solo.result().goals[goal];
                }
                if (careful)
                    continue;
                worst.peakMood = std::max(worst.peakMood, result.peakMood);
                worst.gaveUp += result.gaveUp;
                worst.barriersLost = std::max(worst.barriersLost, result.barriersLost);
                worst.spent = std::max(worst.spent, result.spent);
                worst.budgetLeft = column == 4 ? result.budgetLeft : std::min(worst.budgetLeft, result.budgetLeft);
                mostLeft = std::max(mostLeft, static_cast<int>(result.budgetLeft));
                if (solo.phase() != Phase::Won)
                    lost += " " + std::to_string(index + 1) + "(column " + std::to_string(column) + ")";
            }
            for (int goal = 0; goal < goalsPerLevel; ++goal)
                if (!goalSeen[goal])
                    unreachable += " " + std::to_string(index + 1) + ":" + goalText(levels[index].goals[goal]);
            std::cout << "Level " << index + 1 << ": " << crowdSize(levels[index])
                      << " rioters, bot's worst crowd mood " << static_cast<int>(worst.peakMood) << "%, "
                      << worst.gaveUp / 4 << " gave up, up to " << worst.barriersLost << " barriers lost, $"
                      << static_cast<int>(worst.budgetLeft) << "-" << mostLeft << " left, best " << stars
                      << " stars\n";
        }
        check(lost.empty(), "A steady three-barrier defence must be able to win every level; lost:" + lost);
        check(unreachable.empty(), "Every bonus goal must be reachable by one of the bots; not reached:" + unreachable);

        // A line that is never repaired or replaced does not hold for long.
        std::string held;
        for (int index = 1; index < levelCount; ++index)
        {
            GameModel lazy;
            begin(lazy, index);
            defend(lazy, 5, false);
            if (lazy.phase() == Phase::Won)
                held += " " + std::to_string(index + 1);
        }
        check(held.empty(), "An unrepaired line must lose from level 2 on; it held:" + held);

        // Playing through the campaign in order carries the totals over.
        GameModel campaign;
        campaign.newGame();
        int expectedHeldOff = 0;
        for (int index = 0; index < levelCount; ++index)
        {
            const std::string name = "Level " + std::to_string(index + 1);
            check(campaign.phase() == Phase::Intro && campaign.levelIndex() == index,
                  name + " opens on its intro card");
            check(campaign.gadgets().empty() && campaign.walkers().empty() && campaign.mood() == 0 &&
                  campaign.budget() == campaign.startingBudget(),
                  name + " starts with a clean street and a fresh budget");
            campaign.beginLevel();
            defend(campaign);
            check(campaign.phase() == Phase::Won, name + " can be won in sequence");
            check(campaign.heldOff() + campaign.reached() == crowdSize(campaign.level()),
                  name + " accounts for every spawned walker");
            check(campaign.gaveUp() <= campaign.heldOff(), name + " counts rioters who gave up as held off");
            expectedHeldOff += campaign.heldOff();
            check(campaign.totalHeldOff() == expectedHeldOff, name + " adds its score to the run total");
            check(campaign.lastLevel() == (index == levelCount - 1), "Only the last level is final");
            campaign.nextLevel();
            const int starsBefore = campaign.progress().totalStars();
            check(!campaign.progress().defended[index], "The riot win does not complete the defend interlude");
            check(campaign.campaignNumber() == index * 2 + 2, "Defend levels use the even campaign numbers");
            clearDefend(campaign);
            check(campaign.progress().defended[index] && campaign.progress().totalStars() == starsBefore,
                  "Defend completion is saved without awarding stars or points");
            campaign.nextLevel();
            if (campaign.phase() == Phase::Shop)
            {
                check(campaign.shopFrom() == Phase::DefendWon && campaign.progress().anyAffordable(),
                      name + " only detours through the shop when an upgrade is affordable");
                campaign.closeShop();
            }
        }
        check(campaign.phase() == Phase::Finished, "Winning the last level finishes the game");
        check(!campaign.place(GadgetType::Cone, 0, 0), "Cannot build on the final screen");
        const Progress earned = campaign.progress();
        campaign.backToTitle();
        check(campaign.phase() == Phase::Title && campaign.levelIndex() == 0 && campaign.totalHeldOff() == 0 &&
              campaign.totalTrips() == 0,
              "Playing again returns to a fresh title screen");
        check(campaign.progress().stars == earned.stars, "The title screen keeps the stars earned");

        // Developer shortcuts use the normal completion paths.
        GameModel shortcut;
        begin(shortcut, levelCount - 1);
        shortcut.debugLose();
        check(shortcut.phase() == Phase::Lost, "Debug loss ends the level");
        shortcut.retryLevel();
        shortcut.debugWin();
        check(shortcut.phase() == Phase::Won && shortcut.lastLevel(), "Debug win ends the level");
        shortcut.nextLevel();
        check(shortcut.phase() == Phase::DefendIntro, "Even the final riot is followed by a defence");
        shortcut.beginLevel();
        shortcut.debugLose();
        check(shortcut.phase() == Phase::DefendLost, "Debug loss works during defence");
        shortcut.retryLevel();
        check(shortcut.phase() == Phase::Defend && shortcut.defence().lives() == 3,
              "Defend retries restart the interlude, not the riot");
        shortcut.debugWin();
        shortcut.nextLevel();
        check(shortcut.phase() == Phase::Finished, "Finishing the final defence ends the campaign");

        // Boredom: rioters stuck behind a wall slowly give up and go home, a few at a time.
        GameModel bored;
        begin(bored);
        check(bored.place(GadgetType::Van, 0, 2) && bored.place(GadgetType::Barrier, 1, 2) &&
                  bored.place(GadgetType::Barrier, 2, 2),
              "A wall for the boredom test");
        bored.ready();
        int giveUps = 0;
        int boredomEvents = 0;
        float firstGiveUp = -1.0f;
        float lastGiveUp = -1.0f;
        int leaver = -1;
        float leaverX = 0.0f;
        float shortest = 1e9f;
        float longest = 0.0f;
        for (int tick = 0; tick < 60 * 40 && bored.phase() == Phase::Wave; ++tick)
        {
            bored.tick(config::fixedStep);
            for (const auto& walker : bored.walkers())
            {
                shortest = std::min(shortest, walker.maxPatience);
                longest = std::max(longest, walker.maxPatience);
            }
            for (const auto& event : bored.takeEvents())
            {
                if (event.type == EventType::Bored)
                    ++boredomEvents;
                if (event.type != EventType::GiveUp)
                    continue;
                const Walker* quitter = bored.findWalker(event.walker);
                check(quitter && quitter->leaving && quitter->bored, "A give-up event names a bored walker leaving");
                const float now = static_cast<float>(tick) * config::fixedStep;
                firstGiveUp = giveUps == 0 ? now : firstGiveUp;
                lastGiveUp = now;
                if (giveUps++ == 0)
                {
                    leaver = event.walker;
                    leaverX = quitter->x;
                }
            }
            if (leaver >= 0 && bored.findWalker(leaver) && bored.findWalker(leaver)->x < leaverX - 20.0f)
                leaverX = -1e9f;
        }
        check(giveUps >= 3, "A crowd held at a wall starts to give up");
        check(boredomEvents >= giveUps, "Rioters grumble before they give up");
        check(lastGiveUp - firstGiveUp > 2.0f, "The crowd thins out gradually, not all at once");
        check(longest - shortest > config::patienceSeconds * 0.3f, "Patience varies between rioters");
        check(leaverX == -1e9f, "A rioter who gives up walks back the way they came");
        check(bored.phase() == Phase::Intermission, "The boredom test reaches the whistle");
        check(bored.gaveUp() == giveUps && bored.heldOff() + bored.reached() == levels[0].waves[0].walkers,
              "Rioters who give up count as held off, once each");

        GameModel stubborn;
        begin(stubborn);
        stubborn.ready();
        advance(stubborn, 28.5f);
        check(stubborn.gaveUp() == 0, "Rioters with nothing in their way never get bored");

        // Bricks and smoke bombs: lobbed forward by marching rioters, aimed at walls in range, and smoke bombs
        // only turn up after the first level.
        for (const int index : {0, 1, levelCount - 1})
        {
            const std::string name = "Level " + std::to_string(index + 1);
            GameModel lobbing;
            begin(lobbing, index);
            for (int lane = 0; lane < config::lanes; ++lane)
                check(lobbing.place(GadgetType::Barrier, lane, 6), "A line for the bricks to bounce off");
            lobbing.ready();
            int bricks = 0;
            int smokeBombs = 0;
            int aimed = 0;
            for (int tick = 0; tick < 60 * 280 && lobbing.canBuild(); ++tick)
            {
                for (int lane = 0; lane < config::lanes; ++lane)
                    if (!lobbing.gadgetAt(lane, 6))
                        lobbing.place(GadgetType::Barrier, lane, 6);
                    else if (lobbing.gadgetAt(lane, 6)->health < 100.0f)
                        lobbing.repair(lane, 6);
                lobbing.tick(config::fixedStep);
                for (const auto& event : lobbing.takeEvents())
                {
                    if (event.type != EventType::Throw)
                        continue;
                    check(lobbing.phase() == Phase::Wave, "Only a crowd in the middle of a wave throws things");
                    const Walker* thrower = lobbing.findWalker(event.walker);
                    check(thrower && !thrower->leaving && thrower->stumble <= 0.0f && thrower->lane == event.lane,
                          "Throws come from a marching rioter, in their own lane");
                    check(event.landX > event.x && event.landX < config::goalX, "Throws land ahead, on the street");
                    if (event.thrown == Throwable::SmokeBomb)
                    {
                        ++smokeBombs;
                        check(event.column < 0, "Smoke bombs are not aimed at anything");
                        continue;
                    }
                    ++bricks;
                    if (event.column < 0)
                        continue;
                    ++aimed;
                    const Gadget* wall = lobbing.gadgetAt(event.lane, event.column);
                    check(wall && wall->type != GadgetType::Cone &&
                              event.landX == GameModel::columnX(event.column) - config::gadgets[1].halfWidth &&
                              event.landX - event.x <= config::throwReach,
                          "An aimed brick comes down on the front of a wall within reach");
                }
            }
            check(bricks > 0 && aimed > 0, name + "'s crowd lobs bricks at the barriers");
            check(lobbing.bricksThrown() == bricks && lobbing.smokeBombsThrown() == smokeBombs,
                  name + " counts every throw");
            check((smokeBombs > 0) == (index > 0), name + (index > 0 ? " has smoke bombs" : " has no smoke bombs yet"));
        }

        // Stars, bonus goals and points.
        GameModel career;
        career.openMap();
        check(career.selectLevel(0), "Career starts on level 1");
        career.takeProgressChanged();
        career.beginLevel();
        check(career.goalOnTrack(0) && career.goalOnTrack(1), "Level 1 goals start on track");
        career.debugWin();
        check(career.result().stars == 3 && career.result().goals[0] && career.result().goals[1],
              "A clean win meets both goals for three stars");
        check(career.result().previousBest == 0 && career.result().pointsEarned == 3 * pointsPerStar,
              "New stars pay out upgrade points");
        check(contains(career.result().unlocked, "HAT: BOBBY HELMET") &&
                  contains(career.result().unlocked, "CONE: FRIENDLY CONE") &&
                  contains(career.result().unlocked, "CONE: COOL CONE"),
              "Three stars and goals met unlock cosmetics");
        check(career.takeProgressChanged() && !career.takeProgressChanged(), "A win asks to be saved once");
        check(career.progress().stars[0] == 3 && !career.progress().levelUnlocked(1),
              "Clearing the riot alone cannot unlock the next pair");
        GameModel resumed;
        resumed.setProgress(parseProgress(serialize(career.progress())));
        resumed.openMap();
        check(resumed.frontierLevel() == 0 && resumed.selectLevel(0) && resumed.phase() == Phase::DefendIntro,
              "Reloading resumes the mandatory defend interlude without replaying the riot");
        check(!resumed.selectLevel(1), "A pending defence cannot be skipped on the map");
        resumed.beginLevel();
        resumed.debugLose();
        resumed.retryLevel();
        check(resumed.progress().stars[0] == 3 && resumed.defence().lives() == 3 &&
                  resumed.defence().shieldHealth() == defendConfig::shieldBlocks,
              "A defence retry keeps riot stars and restores shield and lives");
        career.nextLevel();
        clearDefend(career);
        check(career.progress().levelUnlocked(1) && !career.progress().levelUnlocked(2),
              "Completing the defence unlocks the next riot only");
        career.openMap();
        check(career.frontierLevel() == 1, "The map then points at level 2");
        career.selectLevel(0);
        career.beginLevel();
        career.debugWin();
        check(career.result().previousBest == 3 && career.result().pointsEarned == 0 &&
                  career.result().unlocked.empty(),
              "Replaying for the same stars pays nothing more");

        career.openMap();
        check(career.selectLevel(1), "The next riot is open after completing the pair");
        career.beginLevel();
        check(career.place(GadgetType::Van, 0, 5), "A van for the goal test");
        check(!career.goalOnTrack(0), "Placing a van fails a no-vans goal at once");
        career.debugWin();
        check(career.result().stars == 1 && !career.result().goals[0] && !career.result().goals[1],
              "Clearing a level without its goals is one star");
        check(career.progress().totalStars() == 4 && career.progress().points() == 4 * pointsPerStar,
              "Points come from total best stars");

        // The upgrade shop.
        career.nextLevel();
        clearDefend(career);
        career.nextLevel();
        check(career.phase() == Phase::Shop && career.shopFrom() == Phase::DefendWon,
              "A completed pair with points opens the shop");
        check(career.buyUpgrade(0) && career.progress().tiers[0] == 1, "Buy reinforced barriers");
        check(std::abs(career.maxHealth(GadgetType::Barrier) - config::gadgets[1].health * 1.15f) < 0.01f,
              "Reinforced barriers have more health");
        check(career.buyUpgrade(0) && !career.buyUpgrade(0) && career.progress().tiers[0] == 2,
              "Upgrade tiers cost more each time");
        check(career.buyUpgrade(3) && career.cost(GadgetType::Cone) == config::gadgets[0].cost - 3,
              "Cheaper cones");
        check(career.progress().points() == 0 && !career.progress().anyAffordable(), "Points are spent");
        career.refundUpgrades();
        check(career.progress().points() == 4 && career.progress().pointsSpent() == 0, "Refunds return every point");
        check(career.buyUpgrade(1) && career.buyUpgrade(2), "Buy a bigger budget and faster repairs");
        check(career.repairHealth() == config::repairHealth + 20.0f, "Faster repairs patch more health");
        career.closeShop();
        check(career.phase() == Phase::Intro && career.levelIndex() == 2 &&
                  career.budget() == career.level().startingBudget + 40.0f,
              "Leaving the shop goes on to the next level with the bigger budget");
        career.openMap();
        career.openShop();
        career.closeShop();
        check(career.phase() == Phase::Map, "The shop opened from the map returns to the map");

        // Cosmetics.
        check(career.equipHat(1) && career.progress().hat == 1, "An unlocked hat can be worn");
        check(!career.equipHat(4) && career.progress().hat == 1, "A locked hat cannot");
        check(career.equipCone(3) && !career.equipCone(4), "Unlocked cone designs only");
        career.openWardrobe();
        check(career.phase() == Phase::Wardrobe, "The wardrobe opens from the map");
        career.closeWardrobe();
        check(career.phase() == Phase::Map, "The wardrobe returns to the map");

        // Saving and loading.
        const Progress saved = career.progress();
        const Progress parsed = parseProgress(serialize(saved));
        check(parsed.stars == saved.stars && parsed.goalsMet == saved.goalsMet && parsed.tiers == saved.tiers &&
                  parsed.hat == saved.hat && parsed.cone == saved.cone && parsed.defended == saved.defended,
              "Progress survives a save and load");
        const Progress damaged = parseProgress(
            "stars=9,x,-2\ngoals=255\ndefended=8,1,-1\nupgrade.barriers=7\nupgrade.nonsense=2\nhat=crown\ncone=no-such-cone\n");
        check(damaged.stars[0] == 3 && damaged.stars[1] == 0 && damaged.stars[2] == 0 && damaged.goalsMet[0] == 3,
              "A damaged save clamps its stars and goals");
        check(damaged.pointsSpent() == 0 && damaged.hat == 0 && damaged.cone == 0,
              "A damaged save refunds unaffordable upgrades and takes off locked cosmetics");
        check(damaged.defended[0] == 1 && damaged.defended[1] == 0 && damaged.defended[2] == 0,
              "A defend completion requires a cleared riot and is clamped to a boolean");
        const Progress legacy = parseProgress("stars=3,2\ngoals=3,1\n");
        check(legacy.stars[0] == 3 && legacy.stars[1] == 2 && !legacy.defended[0] && legacy.levelUnlocked(1),
              "Older saves keep stars and cleared riots, with their new interludes still to play");
        const auto folder = std::filesystem::temp_directory_path() / "RiotControlTests";
        std::filesystem::remove_all(folder);
        Progress loaded;
        check(!loadProgress(folder / "progress.txt", loaded), "A missing save loads nothing");
        check(saveProgress(folder / "progress.txt", saved), "Progress can be saved");
        check(loadProgress(folder / "progress.txt", loaded) && loaded.stars == saved.stars &&
                  loaded.tiers == saved.tiers && loaded.hat == saved.hat,
              "Progress can be loaded back");
        std::filesystem::remove_all(folder);
        GameModel restored;
        restored.setProgress(loaded);
        restored.openMap();
        check(!restored.takeProgressChanged() && restored.selectLevel(2), "Loaded progress unlocks the map");

        std::cout << "Gameplay checks passed.\n";
        return EXIT_SUCCESS;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAILED: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
