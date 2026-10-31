#include "GameScene.h"

#include <algorithm>
#include <cmath>

namespace riot
{
namespace
{
ax::Vec2 projectilePosition(const DefendThrow& projectile)
{
    const float t = std::clamp(projectile.age / projectile.flight, 0.0f, 1.0f);
    return {640.0f + static_cast<float>(projectile.from) * 210.0f * (1.0f - t),
            490.0f - 215.0f * t + std::sin(t * 3.141593f) * 150.0f};
}

void pixelLine(PixelArt& art, ax::Vec2 from, ax::Vec2 to, float thickness, std::uint32_t color, float alpha = 1.0f)
{
    const float distance = std::max(std::abs(to.x - from.x), std::abs(to.y - from.y));
    const int steps = std::max(1, static_cast<int>(distance / 5.0f));
    for (int i = 0; i <= steps; ++i)
    {
        const float t = static_cast<float>(i) / static_cast<float>(steps);
        const ax::Vec2 point = from + (to - from) * t;
        art.rect(std::round(point.x / 4.0f) * 4.0f, std::round(point.y / 4.0f) * 4.0f,
                 thickness, thickness, color, alpha);
    }
}

void projectileArt(PixelArt& art, DefendThing kind, ax::Vec2 point, float unit)
{
    using namespace palette;
    if (kind == DefendThing::Brick)
    {
        art.rect(point.x - 6 * unit, point.y - 3 * unit, 12 * unit, 6 * unit, ink);
        art.rect(point.x - 5 * unit, point.y - 2 * unit, 10 * unit, 4 * unit, 0xb65850);
        art.rect(point.x - 5 * unit, point.y + unit, 10 * unit, unit, coral);
        art.rect(point.x - unit, point.y - 2 * unit, unit, 3 * unit, 0x803e42);
        art.rect(point.x + 3 * unit, point.y, 2 * unit, unit, 0xd98972);
    }
    else
    {
        art.rect(point.x - 3 * unit, point.y - 5 * unit, 6 * unit, 10 * unit, ink);
        art.rect(point.x - 2 * unit, point.y - 4 * unit, 4 * unit, 8 * unit, mint);
        art.rect(point.x - 2 * unit, point.y - unit, 4 * unit, 3 * unit, paper);
        art.rect(point.x - unit, point.y + 5 * unit, 2 * unit, unit, yellow);
        art.puff(point.x + 2 * unit, point.y + 8 * unit, 3 * unit, 0xa39acd, 0.55f);
    }
}
}

void GameScene::processDefendEvents()
{
    for (const auto& event : model_.takeDefendEvents())
    {
        const auto point = projectilePosition(event.projectile);
        defendFx_.push_back({event.outcome, event.projectile.kind, point});
        switch (event.outcome)
        {
        case DefendOutcome::Sliced:
            sound("trip");
            break;
        case DefendOutcome::Blocked:
            sound(model_.defence().shieldHealth() == 0 ? "break" : "bonk");
            shake_ = 0.2f;
            break;
        case DefendOutcome::Hit:
            sound("thud");
            shake_ = 0.6f;
            break;
        case DefendOutcome::Smoked:
            sound("fizz");
            break;
        }
    }
}

void GameScene::drawDefend(PixelArt& art)
{
    using namespace palette;
    const auto& defence = model_.defence();
    art.rect(0, 0, 1280, 800, dark);
    art.rect(0, 350, 1280, 360, 0xeccdb0);
    art.rect(0, 625, 1280, 85, 0xe6bcb0);
    for (int i = 0; i < 7; ++i)
    {
        const float x = static_cast<float>(i) * 200.0f - 30.0f;
        const float roof = 570.0f + static_cast<float>(i % 3) * 27.0f;
        const std::uint32_t wall = i % 2 ? 0x92a6a0 : 0xb99292;
        art.rect(x, 442, 188, roof - 442, wall);
        art.rect(x - 4, roof, 196, 10, 0x746e7c);
        art.rect(x + 135, roof + 10, 18, 30, 0x9e8683);
        for (int w = 0; w < 4; ++w)
        {
            art.rect(x + 16 + w * 41.0f, 485, 24, 42, ink);
            art.rect(x + 20 + w * 41.0f, 509, 16, 13, 0xd8b98f);
        }
    }
    art.rect(0, 430, 1280, 20, paper);
    for (int row = 0; row < 31; ++row)
    {
        const float depth = static_cast<float>(row) / 30.0f;
        const float left = 450.0f * (1.0f - depth);
        const float right = 830.0f + 450.0f * depth;
        const float y = 418.0f - static_cast<float>(row) * 12.0f;
        art.rect(0, y, 1280, 12, row % 3 ? 0xc7bfa5 : 0xb0b19d);
        art.rect(left, y, right - left, 12, row % 4 ? 0x495d68 : 0x4e656e);
        art.rect(left - 8, y, 8, 12, paper);
        art.rect(right, y, 8, 12, paper);
        if (row % 7 < 3)
            art.rect(636 - depth * 4, y, 8 + depth * 8, 12, 0xa3b0a5);
    }
    art.rect(458, 565, 364, 31, ink);
    art.centered("POLICE LINE - PLEASE FORM A QUEUE", 640, 577, 1, yellow);

    for (int i = 0; i < 13; ++i)
    {
        WalkerPose pose{};
        pose.look = static_cast<std::uint32_t>(i + 11) * 0x9e3779b9u;
        pose.time = time_ + static_cast<float>(i);
        pose.balaclava = i % 3 != 0;
        pose.prop.kind = flavour::PropKind::Nothing;
        if (i % 4 == 0)
        {
            pose.prop.kind = flavour::PropKind::Placard;
            pose.prop.text = model_.level().placards[0];
        }
        pose.throwing = defence.incoming() && defence.incoming()->age < 0.35f &&
                        i == (defence.incoming()->from + 1) * 4 + 2;
        art.walker(370.0f + static_cast<float>(i) * 44.0f, 433.0f + static_cast<float>(i % 2) * 8.0f,
                   pose, 2.2f);
    }

    const bool ready = defence.inReach() && model_.phase() == Phase::Defend;
    art.frame(490, 200, 300, 240, ready ? mint : muted, ready ? 5.0f : 2.0f);
    art.centered(ready ? "SWIPE NOW!" : "WAIT FOR THE BRICK", 640, 177, 2, ready ? mint : paper);
    if (const auto& incoming = defence.incoming())
    {
        const float t = std::clamp(incoming->age / incoming->flight, 0.0f, 1.0f);
        projectileArt(art, incoming->kind, projectilePosition(*incoming), 1.7f + t * t * 11.0f);
        art.centered(incoming->kind == DefendThing::Brick ? "BRICK!" : "SMOKE BOMB!",
                     640, 612, 2, incoming->kind == DefendThing::Brick ? coral : mint);
    }

    for (const auto& effect : defendFx_)
    {
        const float age = 0.65f - effect.life;
        if (effect.outcome == DefendOutcome::Sliced)
        {
            for (float side : {-1.0f, 1.0f})
            {
                const float x = effect.position.x + side * (18.0f + age * 360.0f);
                const float y = effect.position.y + age * 140.0f - age * age * 700.0f;
                art.rect(x - 24, y, 48, 38, ink);
                art.rect(x - 20, y + 4, 40, 30, effect.kind == DefendThing::Brick ? coral : mint);
                art.rect(x - 20, y + 24, 40, 6, paper);
            }
            pixelLine(art, {400, 210}, {880, 440}, 8, paper, effect.life / 0.65f);
            art.centered("SLICED!", 640, 540, 4, mint);
        }
        else if (effect.outcome == DefendOutcome::Blocked)
            art.centered(defence.shieldHealth() ? "BLOCKED!" : "SHIELD BROKEN!", 640, 540, 3, yellow);
        else if (effect.outcome == DefendOutcome::Hit)
        {
            art.frame(8, 55, 1264, 650, coral, 16);
            art.centered("OUCH! ONE LIFE LOST", 640, 540, 3, coral);
        }
    }

    if (defence.smoke() > 0.0f && model_.phase() == Phase::Defend)
    {
        const float opacity = std::min(0.9f, defence.smoke() / defendConfig::smokeSeconds);
        art.rect(0, 55, 1280, 650, 0x9295b6, opacity * 0.7f);
        for (int i = 0; i < 18; ++i)
        {
            const float x = static_cast<float>((i * 193) % 1400) - 60.0f + std::sin(time_ + i) * 32;
            const float y = 110.0f + static_cast<float>((i * 137) % 500);
            art.puff(x, y, 100.0f + static_cast<float>(i % 3) * 35.0f,
                     i % 2 ? 0xa39acd : 0xc5c5d9, opacity * 0.7f);
        }
        art.centered("SMOKE! KEEP YOUR TIMING.", 640, 650, 2, paper);
    }

    if (defence.shieldHealth() > 0)
    {
        const bool raised = defence.shielding();
        const float x = raised ? 396.0f : 88.0f;
        const float y = raised ? 75.0f : -250.0f;
        art.rect(x, y, 420, 470, 0x7394a3, 0.35f);
        art.frame(x, y, 420, 470, ink, 14);
        art.frame(x + 14, y + 14, 392, 442, 0x7394a3, 18);
        art.frame(x + 32, y + 130, 356, 298, 0xc0e1dc, 12);
        art.rect(x + 44, y + 142, 332, 274, 0xa6c9c8, 0.2f);
        art.rect(x + 55, y + 180, 16, 215, paper, 0.4f);
        art.rect(x + 32, y + 32, 356, 76, ink);
        art.centered("POLICE", x + 210, y + 52, 5, paper);
        for (int crack = 0; crack < defendConfig::shieldBlocks - defence.shieldHealth(); ++crack)
        {
            const float cx = x + 92 + static_cast<float>(crack) * 61;
            pixelLine(art, {cx, y + 400}, {cx - 24, y + 310}, 4, ink);
            pixelLine(art, {cx - 24, y + 310}, {cx + 15, y + 260}, 4, ink);
            pixelLine(art, {cx - 15, y + 338}, {cx + 28, y + 354}, 4, ink);
        }
        art.rect(x + 155, y + 2, 110, 30, 0xe0aa84);
        art.rect(x + 174, y - 25, 72, 38, ink);
    }
    else
    {
        art.rect(110, 56, 165, 72, ink);
        art.text("BROKEN", 128, 84, 3, coral);
    }

    const float swing = defence.swing() / defendConfig::swipeSeconds;
    const float offset = swing > 0 ? std::sin((1.0f - swing) * 3.141593f) : 0.0f;
    const float direction = static_cast<float>(defence.swingDirection());
    const ax::Vec2 grip{1050.0f - offset * 340.0f, 105.0f + offset * 90.0f};
    const ax::Vec2 tip{980.0f - offset * (400.0f + direction * 180.0f), 400.0f + offset * 90.0f};
    pixelLine(art, grip, tip, 28, ink);
    pixelLine(art, {grip.x + 7, grip.y + 15}, {tip.x + 7, tip.y - 4}, 8, 0x516776);
    art.rect(grip.x - 20, grip.y - 20, 72, 48, 0xe0aa84);
    art.rect(grip.x + 6, grip.y - 60, 82, 45, ink);

    art.rect(0, 704, 1280, 96, dark);
    art.text("LEVEL " + std::to_string(model_.campaignNumber()) + "/12 - DEFEND", 28, 761, 2, yellow);
    art.text("LIVES", 28, 720, 2, paper);
    for (int i = 0; i < defendConfig::lives; ++i)
    {
        art.rect(112 + static_cast<float>(i) * 40, 717, 28, 26, i < defence.lives() ? coral : ink);
        art.text("+", 118 + static_cast<float>(i) * 40, 723, 2, i < defence.lives() ? paper : muted);
    }
    art.text("SHIELD " + std::to_string(defence.shieldHealth()) + "/" + std::to_string(defendConfig::shieldBlocks),
             315, 722, 2, defence.shieldHealth() ? mint : coral);
    art.text("THROWS " + std::to_string(defence.resolved()) + "/" + std::to_string(defence.totalThrows()),
             610, 722, 2, paper);
    art.text(defence.cooldown() > 0 ? "BATON RECOVERING" : "BATON READY", 950, 722, 2,
             defence.cooldown() > 0 ? muted : mint);
    art.centered("HOLD SPACE: SHIELD   LEFT / RIGHT: SWIPE   ENTER / P: PAUSE", 640, 63, 2, paper);
}

void GameScene::drawDefendCard(PixelArt& art)
{
    using namespace palette;
    const auto phase = model_.phase();
    if (phase == Phase::Defend && !paused_)
        return;
    art.rect(0, 55, 1280, 650, dark, 0.65f);
    art.rect(186, 152, 928, 508, dark);
    art.rect(176, 162, 928, 508, paper);
    art.frame(186, 172, 908, 488, ink, 3);
    if (paused_)
    {
        art.centered("PAUSED FOR A CUPPA", 640, 560, 4, ink);
        art.centered("ENTER OR P TO RESUME. Q QUITS TO THE MAP.", 640, 440, 2, ink);
        button(art, {260, 240, 340, 62}, "RESUME", 2, Command::Resume);
        button(art, {680, 240, 340, 62}, "QUIT TO MAP", 2, Command::Quit);
        return;
    }
    const auto& defence = model_.defence();
    art.centered("LEVEL " + std::to_string(model_.campaignNumber()) + " - DEFEND", 640, 600, 4, ink);
    if (phase == Phase::DefendIntro)
    {
        art.centered("YOUR TURN ON THE FRONT LINE", 640, 535, 3, 0xac554e);
        art.centered("Distant protestors have a few more things to lob.", 640, 482, 2, ink);
        art.centered("Hold SPACE to block. Five hits break your shield.", 640, 440, 2, ink);
        art.centered("Tap LEFT or RIGHT when the timing box turns mint.", 640, 404, 2, ink);
        art.centered("Your baton splits it! Swiping lowers your shield.", 640, 368, 2, ink);
        art.centered("Miss a brick: lose a life. Miss smoke: lose your view.", 640, 332, 2, ink);
        art.centered("3 LIVES. " + std::to_string(defence.totalThrows()) + " THROWS. MAKE THEM COUNT.",
                     640, 282, 2, 0xac554e);
    }
    else
    {
        const bool won = phase == Phase::DefendWon;
        art.centered(won ? "SHIFT SURVIVED!" : "OH, CRUMBS.", 640, 505, 5, won ? 0x387d6d : 0xb65850);
        art.centered(won ? "They've run out of things to throw. Time for tea." :
                          "Three bonks. Take a breather and try this shift again.", 640, 455, 2, ink);
        art.centered("SLICED " + std::to_string(defence.sliced()) + "   BLOCKED " + std::to_string(defence.blocked()) +
                     "   LIVES LEFT " + std::to_string(defence.lives()), 640, 385, 2, ink);
        art.centered(won ? "Riot stars kept. The next part of your shift is ready." :
                          "Your riot result is safe. Fresh shield and lives on retry.", 640, 325, 2, 0xac554e);
    }
    const std::string action = phase == Phase::DefendIntro ? "START DEFEND >" :
                               phase == Phase::DefendLost ? "TRY AGAIN >" :
                               model_.lastLevel() ? "FINISH >" :
                               model_.progress().anyAffordable() ? "UPGRADES >" : "NEXT RIOT >";
    button(art, {236, 192, 260, 62}, "< MAP", 2, Command::Map);
    button(art, {636, 192, 400, 62}, action, 2, Command::Primary);
}
}
