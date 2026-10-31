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
    float fadeSeconds;
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
inline constexpr float menuFadeSeconds = 1.0f;
inline constexpr float musicVolume = 0.28f;
inline constexpr float logoHoldSeconds = 3.0f;
inline constexpr float logoFadeSeconds = 0.6f;
inline constexpr const char* logoFile = "branding/big-beaver-games-logo.png";
namespace animation
{
inline constexpr float blinkPeriod = 2.8f;
inline constexpr float blinkSeconds = 0.13f;
inline constexpr float tickerCharactersPerSecond = 11.0f;
inline constexpr float throwPeriod = 1.1f;
inline constexpr float smokePeriod = 2.2f;
inline constexpr float titleShakeSeconds = 0.22f;
inline constexpr float titlePulseSpeed = 3.0f;
inline constexpr float mugSlidePixelsPerSecond = 5.0f;
}

inline constexpr std::array<SceneDef, 4> scenes{{
    {Scene::Crowd, 9.0f, 1.0f, "",
     {{{0.0f, 0.0f, ""},
       {0.0f, 0.0f, ""}}}},
    {Scene::Studio, 9.0f, 1.0f, "LIVE FROM THE STUDIO",
     {{{0.0f, 4.5f, "Good evening. Scenes of chaos tonight as the nation reaches boiling point."},
       {4.5f, 9.0f, "That's right, Gary. And it's all over the daftest things imaginable."}}}},
    {Scene::Police, 7.0f, 1.0f, "",
     {{{0.0f, 0.0f, ""},
       {0.0f, 0.0f, ""}}}},
    {Scene::Title, 6.0f, 0.0f, "CAN YOU STOP THE RIOTS?",
     {{{0.0f, 6.0f, "Britain is in uproar. Again."}, {0.0f, 0.0f, ""}}}},
}};

constexpr float duration()
{
    float total = logoHoldSeconds + logoFadeSeconds;
    for (const auto& scene : scenes)
        total += scene.seconds;
    return total;
}

constexpr bool valid()
{
    for (const auto& scene : scenes)
    {
        if (scene.seconds <= 0.0f || scene.fadeSeconds < 0.0f || scene.fadeSeconds >= scene.seconds ||
            !scene.heading ||
            ((scene.scene == Scene::Studio || scene.scene == Scene::Title) && !scene.heading[0]))
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
    bool showingLogo() const { return active_ && logoTime_ < intro::logoHoldSeconds + intro::logoFadeSeconds; }
    float logoOpacity() const
    {
        return showingLogo() ? 1.0f - std::clamp((logoTime_ - intro::logoHoldSeconds) / intro::logoFadeSeconds,
                                               0.0f, 1.0f) : 0.0f;
    }
    float fade() const
    {
        if (!active_ || showingLogo() || scene_ + 1 == intro::scenes.size() || scene().fadeSeconds == 0.0f)
            return 0.0f;
        const float t = std::clamp((time_ - (scene().seconds - scene().fadeSeconds)) / scene().fadeSeconds, 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }
    float menuFade() const
    {
        if (!active_ || scene_ + 1 != intro::scenes.size())
            return 0.0f;
        return std::clamp((time_ - (scene().seconds - intro::menuFadeSeconds)) / intro::menuFadeSeconds, 0.0f, 1.0f);
    }
    const char* caption() const
    {
        if (active_ && !showingLogo())
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
        if (showingLogo())
        {
            const float remaining = intro::logoHoldSeconds + intro::logoFadeSeconds - logoTime_;
            const float consumed = std::min(dt, remaining);
            logoTime_ += consumed;
            dt -= consumed;
            if (dt <= 0.0f)
                return;
        }
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
    float logoTime_ = 0.0f;
    bool active_ = true;
};
}
