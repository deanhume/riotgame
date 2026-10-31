#pragma once

#include "Flavour.h"
#include "GameModel.h"
#include "Progression.h"
#include "2d/DrawNode.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace riot
{
namespace palette
{
inline constexpr std::uint32_t ink = 0x182939;
inline constexpr std::uint32_t dark = 0x111e2c;
inline constexpr std::uint32_t paper = 0xfff0ce;
inline constexpr std::uint32_t muted = 0x93adb6;
inline constexpr std::uint32_t mint = 0x79dfc0;
inline constexpr std::uint32_t yellow = 0xffce61;
inline constexpr std::uint32_t coral = 0xf47c71;
inline constexpr std::uint32_t blue = 0x6aafd8;
}

// Everything needed to draw one rioter in one frame.
struct WalkerPose
{
    std::uint32_t look = 0;    // outfit, colours and walk timing
    float time = 0.0f;
    bool blocked = false;      // shoving: jiggles on the spot
    float stumble = 0.0f;      // flat on their face
    bool leaving = false;      // faces left, going home
    float squash = 0.0f;       // 0-1: squashed by a bump, springing back
    bool surprised = false;    // wide eyes and an "o" mouth
    bool bored = false;        // droopy eyes
    bool exclaim = false;      // "!" above the head
    bool sweat = false;        // a nervous drop
    bool cone = false;         // wearing a stolen cone as a hat
    bool balaclava = false;    // a black knitted balaclava instead of hair (see flavour::wearsBalaclava)
    bool throwing = false;     // throwing arm up: has just lobbed a brick or smoke bomb
    ConeArt coneStyle = ConeArt::Classic;
    flavour::Prop prop;        // a placard with no text is a blank, scribbled board
};

class PixelArt
{
public:
    explicit PixelArt(ax::DrawNode* node, float opacity = 1.0f, float offsetX = 0.0f, bool clipToScreen = false)
        : node_(node), opacity_(opacity), offsetX_(offsetX), clipToScreen_(clipToScreen) {}
    void rect(float x, float y, float w, float h, std::uint32_t color, float alpha = 1.0f);
    void frame(float x, float y, float w, float h, std::uint32_t color, float thickness = 2.0f);
    void text(std::string_view value, float x, float y, int scale, std::uint32_t color);
    void centered(std::string_view value, float x, float y, int scale, std::uint32_t color);
    // A tiny 3x5 capitals font for placards and flags. `y` is the bottom of the letters.
    void micro(std::string_view value, float x, float y, int scale, std::uint32_t color);
    void walker(float x, float y, const WalkerPose& pose, float scale = 2.0f);
    void gadget(GadgetType type, float x, float y, float health, float time, float scale = 1.0f,
                ConeArt cone = ConeArt::Classic);
    void wrench(float x, float y, std::uint32_t color);
    // A brick centred on (x, y), 12x6 pixels lying flat; `spin` (in turns) tumbles it through four poses.
    void brick(float x, float y, float spin, float alpha = 1.0f);
    // A smoke bomb canister centred on (x, y), tumbling like a brick. Lying still (spin 0) it points right, and
    // its fizzing end is at (x + 8, y).
    void smokeBomb(float x, float y, float spin, std::uint32_t colour, float alpha = 1.0f);
    // The flickering, fizzing end of a lit smoke bomb, like a flare.
    void flare(float x, float y, std::uint32_t colour, float time, float strength = 1.0f);
    // A chunky, round puff of smoke.
    void puff(float x, float y, float radius, std::uint32_t colour, float alpha);
    void officer(float x, float y, float scale, HatArt hat = HatArt::Cap, bool sweat = false);
    // Five-pointed star, 9 units across, centred on (x, y).
    void star(float x, float y, float unit, std::uint32_t color);
    // Padlock 8 units wide and 10 tall, standing on (x, y).
    void padlock(float x, float y, float unit, std::uint32_t color);
    static float textWidth(std::string_view value, int scale);
    static float microWidth(std::string_view value, int scale);
    // Word-wraps to maxWidth pixels (an explicit '\n' always breaks) and balances the line lengths.
    static std::vector<std::string> wrap(std::string_view value, float maxWidth, int scale);
    // The same for the micro font at scale 1.
    static std::vector<std::string> microWrap(std::string_view value, float maxWidth);

private:
    void flag(const flavour::FlagDef& def, float poleX, float top, float unit, bool trailLeft, float time);

    ax::DrawNode* node_;
    float opacity_;
    float offsetX_;
    bool clipToScreen_;
};
}
