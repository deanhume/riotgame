#pragma once

#include "Flavour.h"
#include "GameModel.h"
#include "PixelArt.h"
#include "2d/Scene.h"
#include "base/EventKeyboard.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace riot
{
class GameScene final : public ax::Scene
{
public:
    // Command line switches (see README): --level N, --debug and --save PATH.
    struct Options
    {
        int startLevel = 0;  // 1-based; 0 starts at the title screen
        bool debug = false;  // F8 clears the current level, F9 fails it
        std::filesystem::path savePath;  // empty: progress is not loaded or saved
    };

    static GameScene* create(const Options& options = {});
    bool init() override;
    void update(float dt) override;

private:
    struct Button
    {
        float x, y, w, h;
    };

    // Everything a click on a card screen can do. Screens register their buttons while drawing (see button()),
    // so what is clickable is always exactly what was drawn last frame.
    enum class Command { Primary, Map, Title, Shop, Wardrobe, Level, Buy, Refund, Hat, Cone, Back, Resume, Quit };
    struct Hotspot
    {
        Button box;
        Command command;
        int arg;
    };

    void click(ax::Vec2 point, bool repair);
    void key(ax::EventKeyboard::KeyCode key);
    void perform(Command command, int arg = 0);
    void primaryAction();
    void goBack();
    void changeScreen();
    void toggleFullscreen();
    void resetScene();
    void saveIfChanged();
    bool shortStart() const;
    bool onCard() const;
    float roll();
    void drawFrame();
    void drawStreet(PixelArt& art);
    void drawCrowd(PixelArt& art, int lane);
    void drawEffects(PixelArt& art);
    void drawBubbles(PixelArt& art);
    void drawHud(PixelArt& art);
    void drawGoalTracker(PixelArt& art);
    void drawSystemHints(PixelArt& art);
    void drawOverlay(PixelArt& art);
    void drawTitle(PixelArt& art);
    void drawMap(PixelArt& art);
    void drawShop(PixelArt& art);
    void drawWardrobe(PixelArt& art);
    void drawIntro(PixelArt& art);
    void drawReport(PixelArt& art);
    void drawFinished(PixelArt& art);
    // Draws a yellow button and registers it as a hotspot; disabled buttons are grey and not clickable.
    void button(PixelArt& art, const Button& box, std::string_view label, int scale, Command command, int arg = 0,
                bool enabled = true);
    void processEvents();
    void updateCrowd(float dt);
    void say(int walker, flavour::Moment moment);
    void sound(const char* name, float volume = 0.32f);
    void burst(float x, float y, std::uint32_t color, int count);
    void launch(const Event& event);
    void updateThrown(float dt);
    void drawLyingThrown(PixelArt& art, int lane);
    void drawSmoke(PixelArt& art);
    std::pair<int, int> cellAt(ax::Vec2 point) const;

    struct Particle
    {
        ax::Vec2 position;
        ax::Vec2 velocity;
        float life;
        float size;
        std::uint32_t color;
    };
    struct Popup
    {
        std::string text;
        ax::Vec2 position;
        float life;
        std::uint32_t color;
    };
    // Short-lived reactions drawn on one rioter.
    struct WalkerFx
    {
        float squash = 0.0f;  // counts down through a squash-and-stretch wobble
        float squashLength = 1.0f;
        float squashPower = 0.0f;
        float surprise = 0.0f;
        float exclaim = 0.0f;
        float sweat = 0.0f;
        float throwing = 0.0f;  // throwing arm stays up until this runs out
        float quiet = 0.0f;   // won't speak again until this runs out
        bool queued = false;  // was held up last frame
    };
    // A brick or smoke bomb in the air or lying in the road. Purely for show: the model never lets these hurt
    // anything.
    struct Thrown
    {
        Throwable kind;
        ax::Vec2 position;
        ax::Vec2 velocity;
        float ground;        // the road surface in this lane
        float wallX = -1.0f; // front of the wall it is aimed at; < 0 when aimed at the road
        int lane;
        int column;
        float spin = 0.0f;   // in turns
        float spinRate;
        float life = 0.0f;   // seconds lying in the road (bricks) or burning (smoke bombs)
        int bounces = 0;
        bool landed = false;
        bool hitWall = false;
        std::uint32_t colour;
        float puffDue = 0.0f;
        float sparkDue = 0.0f;
    };
    // A drifting cloud of coloured smoke.
    struct Puff
    {
        ax::Vec2 position;
        ax::Vec2 velocity;
        float age = 0.0f;
        float lifetime;
        float radius;
        float grow;
        std::uint32_t colour;
    };
    // How brightly a smoke bomb is fizzing (0 once it's burnt out, or for a brick).
    static float flareStrength(const Thrown& item);
    struct Bubble
    {
        int walker;
        std::string text;
        float life;
        ax::Vec2 anchor;  // the rioter's feet; kept if the rioter disappears
    };

    GameModel model_;
    Options options_;
    Phase shownPhase_ = Phase::Title;
    ax::DrawNode* world_ = nullptr;
    ax::DrawNode* ui_ = nullptr;
    ax::Vec2 pointer_{-100, -100};
    std::vector<Particle> particles_;
    std::vector<Popup> popups_;
    std::vector<Hotspot> hotspots_;
    std::unordered_map<int, WalkerFx> fx_;
    std::vector<Bubble> bubbles_;
    std::vector<Thrown> thrown_;
    std::vector<Puff> smoke_;
    float accumulator_ = 0.0f;
    float time_ = 0.0f;
    float shake_ = 0.0f;
    float soundCooldown_ = 0.0f;
    float bumpCooldown_ = 0.0f;
    float clonkCooldown_ = 0.0f;
    float fizzCooldown_ = 0.0f;
    float banner_ = 0.0f;
    float confettiTimer_ = 0.0f;
    float bubbleGap_ = 0.0f;
    float chatterTimer_ = flavour::chatterEvery;
    float screenTime_ = 0.0f;  // seconds since the current phase began
    int starsShown_ = 0;
    int selected_ = 1;
    std::uint32_t random_ = 781;
    bool paused_ = false;
    bool muted_ = false;
    bool audioFailed_ = false;
    bool saveFailed_ = false;
};
}
