#include "GameScene.h"

#include "audio/AudioEngine.h"

#include <algorithm>
#include <cmath>

namespace riot
{
namespace
{
void presenter(PixelArt& art, float x, bool frazzled, float time)
{
    using namespace palette;
    const float nod = std::floor(std::sin(time * 2.4f + (frazzled ? 1.0f : 0.0f)) * 2.0f) * 2.0f;
    art.rect(x - 54, 350, 108, 86, ink);
    art.rect(x - 10, 350, 20, 72, paper);
    art.rect(x - 4, 366, 8, 40, frazzled ? coral : blue);
    art.rect(x - 34, 426 + nod, 68, 64, 0xe0aa84);
    art.rect(x - 38, 482 + nod, 76, 14, frazzled ? 0x9e7756 : dark);
    if (frazzled)
    {
        art.rect(x - 30, 496, 12, 12, 0x9e7756);
        art.rect(x + 10, 490, 14, 14, 0x9e7756);
        art.rect(x + 39, 457, 8, 12, blue);
    }
    const bool blink = std::fmod(time + (frazzled ? 0.7f : 0.0f), intro::animation::blinkPeriod) <
                       intro::animation::blinkSeconds;
    art.rect(x - 22, 455 + nod, 8, blink ? 2 : 8, ink);
    art.rect(x + 14, 455 + nod, 8, blink ? 2 : 8, ink);
    art.rect(x - 8, 440 + nod, 16, 4, ink);
    art.rect(x - 53, 350, 30, 18, 0xe0aa84);
    art.rect(x + 23, 350, 30, 18, 0xe0aa84);
}

void streetBackdrop(PixelArt& art)
{
    art.rect(0, 170, 1280, 550, 0xeccdb0);
    for (int i = 0; i < 9; ++i)
    {
        const float x = static_cast<float>(i) * 150.0f - 30;
        const float roof = 570.0f + static_cast<float>(i % 3) * 25;
        art.rect(x, 350, 140, roof - 350, i % 2 ? 0x92a6a0 : 0xb99292);
        art.rect(x - 4, roof, 148, 10, 0x746e7c);
        for (int window = 0; window < 3; ++window)
        {
            art.rect(x + 16 + window * 40.0f, 450, 24, 42, palette::ink);
            art.rect(x + 20 + window * 40.0f, 470, 16, 16, 0xd8b98f);
        }
    }
    art.rect(0, 330, 1280, 26, palette::paper);
    art.rect(0, 156, 1280, 174, 0x495d68);
    for (int i = 0; i < 19; ++i)
        art.rect(static_cast<float>(i) * 72, 218, 38, 5, 0xa3b0a5);
}
void comedyThrows(PixelArt& art, float time, bool police)
{
    using namespace palette;
    for (int i = 0; i < 3; ++i)
    {
        const float t = std::fmod(time + static_cast<float>(i) * 0.55f, intro::animation::throwPeriod) /
                        intro::animation::throwPeriod;
        const float start = police ? 30.0f : 480.0f;
        const float impact = police ? 500.0f + static_cast<float>(i) * 190 : 1090.0f;
        const float x = t < 0.7f ? start + (impact - start) * t / 0.7f : impact - (t - 0.7f) * 320;
        const float y = t < 0.7f ? 340 + std::sin(t / 0.7f * 3.141593f) * 180 :
                                  340 + std::sin((t - 0.7f) / 0.3f * 3.141593f) * 65 - (t - 0.7f) * 180;
        art.rect(x - 14, y - 8, 28, 16, ink);
        art.rect(x - 10, y - 4, 20, 10, muted);
        art.rect(x - 10, y + 3, 20, 3, paper);
        if (t >= 0.7f && t < 0.85f)
            art.text("BONK!", impact - 30, 385, 2, paper);
    }
    const float smoke = std::fmod(time, intro::animation::smokePeriod);
    if (smoke > 1.0f)
        for (int i = 0; i < 8; ++i)
        {
            const float age = smoke - 1.0f;
            const float x = 850 + static_cast<float>(i % 4) * 55 + age * 32;
            const float y = 250 + static_cast<float>(i / 4) * 65 + age * 24;
            art.puff(x, y, 25 + age * 24, flavour::smokeColours[static_cast<std::size_t>(i) %
                     flavour::smokeColours.size()], std::max(0.0f, 0.65f - age * 0.14f));
        }
}

void drawCinematicPanel(PixelArt& art, const intro::SceneDef& scene, float time)
{
    using namespace palette;
    art.rect(0, 0, 1280, 800, dark);
    switch (scene.scene)
    {
    case intro::Scene::Studio:
    {
        art.rect(130, 196, 1020, 500, ink);
        art.rect(142, 208, 996, 476, 0x725f54);
        art.rect(165, 230, 866, 430, dark);
        art.rect(177, 242, 842, 406, 0x516776);
        art.rect(190, 258, 816, 374, blue);
        art.rect(204, 500, 788, 117, ink);
        art.centered(intro::channel, 600, 560, 4, paper);
        art.centered(scene.heading, 600, 520, 2, mint);
        presenter(art, 425, false, time);
        presenter(art, 750, true, time);
        art.rect(230, 300, 740, 56, 0x9e7756);
        art.rect(244, 345, 712, 14, paper);
        art.rect(477 + std::floor(std::sin(time * 3) * 4), 360, 65, 8, paper);
        art.rect(791 + std::floor(std::sin(time * 4) * 6), 360, 65, 8, paper);
        const float mug = std::min(time * intro::animation::mugSlidePixelsPerSecond, 50.0f);
        art.rect(891 + mug, 359, 24, 24, mint);
        art.frame(914 + mug, 364, 12, 16, mint, 4);
        art.rect(208, 600, 74, 25, coral);
        art.text("LIVE", 220, 608, 2, paper);
        art.rect(200, 264, 796, 29, ink);
        {
            std::string ticker;
            for (const char* headline : intro::headlines)
                ticker += std::string(headline) + "   /   ";
            const float scroll = time * intro::animation::tickerCharactersPerSecond;
            const auto first = static_cast<std::size_t>(scroll) % ticker.size();
            const std::string repeated = ticker + ticker;
            art.text(repeated.substr(first, 63), 214 - (scroll - std::floor(scroll)) * 12, 276, 2, yellow);
        }
        for (int line = 0; line < 94; ++line)
            art.rect(190, 258 + static_cast<float>(line) * 4, 816, 1, dark, 0.12f);
        art.rect(1060, 525, 46, 46, dark);
        art.rect(1076, 525, 12, 46, muted);
        art.rect(1060, 440, 46, 46, dark);
        for (int line = 0; line < 12; ++line)
            art.rect(1053, 280 + static_cast<float>(line) * 9, 60, 3, dark);
        break;
    }
    case intro::Scene::Crowd:
        streetBackdrop(art);
        for (int i = 0; i < 23; ++i)
        {
            WalkerPose pose{};
            pose.look = static_cast<std::uint32_t>(i + 1) * 0x9e3779b9u;
            pose.time = time + static_cast<float>(i);
            pose.balaclava = flavour::wearsBalaclava(pose.look);
            pose.prop = flavour::chooseProp(pose.look, levels[0]);
            pose.throwing = i % 5 == 0 && std::fmod(time + static_cast<float>(i) * 0.2f,
                                                                intro::animation::throwPeriod) < 0.45f;
            art.walker(90.0f + static_cast<float>(i % 12) * 76,
                       i < 12 ? 312.0f : 235.0f, pose, 3);
        }
        art.gadget(GadgetType::Van, 1115, 258, 1.0f, 0, 1.4f);
        comedyThrows(art, time, false);
        break;
    case intro::Scene::Police:
    {
        streetBackdrop(art);
        art.gadget(GadgetType::Van, 245, 300, 1.0f, 0, 1.8f);
        for (int i = 0; i < 6; ++i)
        {
            const float x = 485.0f + static_cast<float>(i) * 116;
            art.officer(x, 322, 3, HatArt::Bobby);
            art.rect(x - 15, 290, 30, 50, palette::blue, 0.6f);
            art.frame(x - 15, 290, 30, 50, palette::paper, 3);
            art.gadget(GadgetType::Barrier, x, 257, 1.0f, 0, 1.2f);
        }
        art.rect(744, 433, 266, 44, paper);
        art.rect(775, 421, 12, 16, paper);
        art.text(intro::officerBubble, 760, 449, 2, ink);
        const float sip = std::fmod(time, 3.0f) > 1.4f ? 14.0f : 0.0f;
        art.rect(617, 340 + sip, 24, 24, mint);
        art.frame(641, 344 + sip, 10, 16, mint, 4);
        art.rect(929, 348, 12, 8, paper);
        art.rect(933, 350, 2, 5, ink);
        comedyThrows(art, time, true);
        break;
    }
    case intro::Scene::Title:
    {
        const float pulse = 0.75f + std::sin(time * intro::animation::titlePulseSpeed) * 0.15f;
        const float shake = time < intro::animation::titleShakeSeconds ? std::sin(time * 95) * 8 : 0.0f;
        art.frame(120, 292, 1040, 260, mint, 6);
        const auto title = PixelArt::wrap(scene.heading, 960, 8);
        float y = 450.0f;
        for (const auto& line : title)
        {
            art.centered(line, 640 + shake, y, 8, y == 450.0f ? yellow : paper);
            art.rect(240, y - 13, 800, 3, mint, pulse);
            y -= 100;
        }
        break;
    }
    }

    if (scene.scene != intro::Scene::Studio && scene.scene != intro::Scene::Title)
    {
        art.rect(0, 697, 1280, 57, ink);
        art.centered(scene.heading, 640, 717, 3, yellow);
    }
}
}

void GameScene::finishCinematic()
{
    cinematic_.skip();
    ax::AudioEngine::stopAll();
    resetScene();
    screenTime_ = 0.0f;
    time_ = 0.0f;
    world_->setPosition(0, 0);
}

void GameScene::updateCinematicAudio()
{
    const auto index = cinematic_.sceneIndex();
    const auto scene = cinematic_.scene().scene;
    const bool entered = index != cinematicSoundScene_;
    if (entered)
    {
        ax::AudioEngine::stopAll();
        cinematicSoundScene_ = index;
        cinematicSoundBeat_ = -1;
        if (scene == intro::Scene::Studio)
            sound("news", 0.24f);
        else if (scene == intro::Scene::Title)
            sound("sting", 0.28f);
    }
    if (scene != intro::Scene::Crowd && scene != intro::Scene::Police)
        return;
    const int beat = static_cast<int>(cinematic_.sceneTime() / intro::animation::throwPeriod);
    if (beat == cinematicSoundBeat_)
        return;
    cinematicSoundBeat_ = beat;
    sound("clonk", 0.18f);
    if (scene == intro::Scene::Crowd)
        sound("murmur", 0.13f);
    if (beat % 2 == 1)
        sound("fizz", 0.10f);
}

void GameScene::drawCinematic(PixelArt& art)
{
    using namespace palette;
    const auto index = cinematic_.sceneIndex();
    const float camera = cinematic_.cameraPosition();
    PixelArt current(ui_, 1.0f, (static_cast<float>(index) - camera) * config::width, true);
    drawCinematicPanel(current, intro::scenes[index], cinematic_.sceneTime());
    if (cinematic_.pan() > 0.0f && index + 1 < intro::scenes.size())
    {
        PixelArt next(ui_, 1.0f, (static_cast<float>(index + 1) - camera) * config::width, true);
        drawCinematicPanel(next, intro::scenes[index + 1], 0.0f);
    }
    // The title wipes away to the actual menu, rather than fading through a blank frame.
    if (cinematic_.menuWipe() > 0.0f)
    {
        ui_->clear();
        drawTitle(art);
        hotspots_.clear();
        PixelArt outgoing(ui_, 1.0f, -cinematic_.menuWipe() * config::width, true);
        drawCinematicPanel(outgoing, cinematic_.scene(), cinematic_.sceneTime());
    }
    art.rect(30, 68, 1220, 83, ink);
    const auto lines = PixelArt::wrap(cinematic_.caption(), 1150, 2);
    float y = lines.size() > 1 ? 117.0f : 100.0f;
    for (const auto& line : lines)
    {
        art.centered(line, 640, y, 2, paper);
        y -= 26;
    }
    art.text(intro::skipHint, 1020, 28, 1, muted);
    art.text(muted_ ? "[M] SOUND OFF" : "[M] SOUND ON", 30, 28, 1, muted);
}
}
