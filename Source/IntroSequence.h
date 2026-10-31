#pragma once

#include <array>
#include <algorithm>
#include <cmath>
#include <cstddef>

namespace riot
{
namespace intro
{
enum class Scene { Studio, Crowd, Police, Title };

struct Caption
{
    float start;
    float end;
    const char* text;
};

struct SceneDef
{
    Scene scene;
    float seconds;
    float panSeconds;
    const char* heading;
    std::array<Caption, 2> captions;
};

inline constexpr const char* channel = "BRITAIN TONIGHT NEWS";
inline constexpr const char* skipHint = "Press SPACE to skip";
inline constexpr std::array<const char*, 3> headlines{
    "NATION IN UPROAR OVER CURRY SAUCE SHORTAGE",
    "LOCAL MAN STILL IN CONE. CONE NOT INJURED.",
    "EXPERTS ADVISE: DO NOT PANIC (PANIC ADVISED ANYWAY)",
};
inline constexpr const char* officerBubble = "Any minute now.";
inline constexpr float menuWipeSeconds = 0.35f;
namespace animation
{
inline constexpr float blinkPeriod = 2.8f;
inline constexpr float blinkSeconds = 0.13f;
inline constexpr float tickerCharactersPerSecond = 11.0f;
inline constexpr float throwPeriod = 1.7f;
inline constexpr float smokePeriod = 3.4f;
inline constexpr float titleShakeSeconds = 0.22f;
inline constexpr float titlePulseSpeed = 3.0f;
inline constexpr float mugSlidePixelsPerSecond = 5.0f;
}

inline constexpr std::array<SceneDef, 4> scenes{{
    {Scene::Studio, 7.0f, 1.0f, "LIVE FROM THE STUDIO",
     {{{0.0f, 3.5f, "Good evening. Scenes of chaos tonight as the nation reaches boiling point."},
       {3.5f, 7.0f, "That's right, Gary. And it's all over the daftest things imaginable."}}}},
    {Scene::Crowd, 7.0f, 1.1f, "MEANWHILE, ON THE HIGH STREET...",
     {{{0.0f, 7.0f, "Cardboard demands. Invented flags. Absolutely no curry sauce."},
       {0.0f, 0.0f, ""}}}},
    {Scene::Police, 5.0f, 0.9f, "THE POLICE REMAIN EXTREMELY CALM",
     {{{0.0f, 5.0f, "The front line has arrived. Someone has put the kettle on."},
       {0.0f, 0.0f, ""}}}},
    {Scene::Title, 4.0f, 0.0f, "CAN YOU STOP THE RIOTS?",
     {{{0.0f, 4.0f, "Britain is in uproar. Again."}, {0.0f, 0.0f, ""}}}},
}};

constexpr float duration()
{
    float total = 0.0f;
    for (const auto& scene : scenes)
        total += scene.seconds;
    return total;
}

constexpr bool valid()
{
    for (const auto& scene : scenes)
    {
        if (scene.seconds <= 0.0f || scene.panSeconds < 0.0f || scene.panSeconds >= scene.seconds ||
            !scene.heading || !scene.heading[0])
            return false;
        for (const auto& caption : scene.captions)
            if (!caption.text || caption.start < 0.0f || caption.end < caption.start ||
                caption.end > scene.seconds)
                return false;
    }
    return true;
}
static_assert(valid(), "Check intro scene durations and caption ranges");
}

class IntroSequence
{
public:
    bool active() const { return active_; }
    std::size_t sceneIndex() const { return scene_; }
    const intro::SceneDef& scene() const { return intro::scenes[scene_]; }
    float sceneTime() const { return time_; }
    float pan() const
    {
        if (!active_ || scene_ + 1 == intro::scenes.size() || scene().panSeconds == 0.0f)
            return 0.0f;
        const float t = std::clamp((time_ - (scene().seconds - scene().panSeconds)) / scene().panSeconds, 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }
    float cameraPosition() const { return static_cast<float>(scene_) + pan(); }
    float menuWipe() const
    {
        if (!active_ || scene_ + 1 != intro::scenes.size())
            return 0.0f;
        return std::clamp((time_ - (scene().seconds - intro::menuWipeSeconds)) / intro::menuWipeSeconds, 0.0f, 1.0f);
    }
    const char* caption() const
    {
        if (active_)
            for (const auto& caption : scene().captions)
                if (time_ >= caption.start && time_ < caption.end)
                    return caption.text;
        return "";
    }
    void skip() { active_ = false; }
    void tick(float dt)
    {
        if (!active_ || !std::isfinite(dt) || dt <= 0.0f)
            return;
        time_ += dt;
        while (time_ >= scene().seconds)
        {
            time_ -= scene().seconds;
            if (scene_ + 1 == intro::scenes.size())
            {
                time_ = 0.0f;
                active_ = false;
                return;
            }
            ++scene_;
        }
    }

private:
    std::size_t scene_ = 0;
    float time_ = 0.0f;
    bool active_ = true;
};
}
