#include "TargetArt.h"

#include <algorithm>
#include <cmath>
#include <string_view>

namespace riot
{
namespace
{
using namespace palette;

constexpr float centerX = 1171.0f;  // middle of the facade
constexpr float figureX = 1169.0f;  // middle of the figure in the centre window
constexpr std::uint32_t gold = 0xe0b94a;

// Colours and details that differ from building to building. The layout is shared.
struct Scheme
{
    std::uint32_t outline;
    std::uint32_t wall;
    std::uint32_t joints;       // brick or stone lines
    std::uint32_t roofEdge;     // dark strip along the top of the building
    std::uint32_t roofTrim;     // coloured strip on top of that
    std::uint32_t awning;       // stripe colour
    std::uint32_t awningBase;   // the colour between the stripes
    std::uint32_t awningEdge;   // shadow under the awning
    std::uint32_t windowFrame;
    std::uint32_t glass;
    std::uint32_t glassShine;
    std::uint32_t glint;
    std::uint32_t sill;
    std::uint32_t signBoard;
    std::uint32_t signText;
    std::uint32_t tagline;
    bool bricks;                // staggered bricks, otherwise square stone blocks
};

constexpr Scheme chippy{0x846460, 0xf1d59d, 0xddbc89, ink,      coral,    coral,    paper,    0xba6760, 0x9d7960,
                        0x304a56, 0x527681, 0x8daea8, paper,    ink,      paper,    yellow,   true};
constexpr Scheme bakery{0x8a5650, 0xf2bfb8, 0xe2a29b, 0x5e3a34, 0xe8849e, 0xe8849e, paper,    0xb55f78, 0xb27d5e,
                        0x4a3a3f, 0x6d5658, 0xe8c4b0, paper,    0x6b4332, paper,    yellow,   true};
constexpr Scheme council{0x4f5b69, 0xc3cbd3, 0xa3adb8, 0x2b3d57, 0x6c8fc0, 0x37537f, paper,    0x24384f, 0x6c7886,
                         0x2d4150, 0x526b7d, 0x9fc4d8, 0xe6ebf0, 0x2b3d57, paper,    0x9fc4e8, false};
constexpr Scheme pizzeria{0x7a3f33, 0xeab08a, 0xd0906a, 0x58261f, 0xd9473f, 0xd9473f, paper,    0x9b2f2c, 0x8a4d3a,
                          0x3d2b2d, 0x5f4546, 0xf1b99a, paper,    0x8e2a2a, paper,    yellow,   true};
constexpr Scheme hall{0x6a3d38, 0xb7604d, 0xd29a86, 0x454a63, 0x7480a6, 0x4f86b8, paper,    0x37668f, 0xf0e6cf,
                      0x34505c, 0x58798a, 0x9fd0cc, 0xf0e6cf, 0xf0e6cf, ink,      0xac554e, true};
constexpr Scheme townHall{0x6f6a5c, 0xddd6c0, 0xbfb79d, 0x4b4a52, 0xc4a24a, 0x7f2e3a, 0xe9d9a8, 0x58202a, 0x8c8470,
                          0x2d3b4d, 0x51657a, 0x9fb6cc, 0xf0e8d0, 0x274a3b, 0xf5e2a0, paper,    false};

const Scheme& schemeFor(TargetArt art)
{
    switch (art)
    {
    case TargetArt::Bakery: return bakery;
    case TargetArt::CouncilOffices: return council;
    case TargetArt::Pizzeria: return pizzeria;
    case TargetArt::VillageHall: return hall;
    case TargetArt::TownHall: return townHall;
    case TargetArt::Chippy: break;
    }
    return chippy;
}

void disc(PixelArt& art, float cx, float cy, float radius, std::uint32_t color)
{
    constexpr float step = 2.0f;
    for (float dy = -radius; dy < radius; dy += step)
    {
        const float mid = dy + step * 0.5f;
        const float half = std::sqrt(std::max(0.0f, radius * radius - mid * mid));
        art.rect(cx - half, cy + dy, half * 2.0f, step, color);
    }
}

// A thick straight line made of squares: clock hands and the crossed-out calendar.
void stroke(PixelArt& art, float x0, float y0, float x1, float y1, float thickness, std::uint32_t color)
{
    const float length = std::max(std::abs(x1 - x0), std::abs(y1 - y0));
    const int steps = std::max(1, static_cast<int>(std::ceil(length / (thickness * 0.5f))));
    for (int i = 0; i <= steps; ++i)
    {
        const float t = static_cast<float>(i) / static_cast<float>(steps);
        art.rect(x0 + (x1 - x0) * t - thickness * 0.5f, y0 + (y1 - y0) * t - thickness * 0.5f, thickness, thickness,
                 color);
    }
}

void drawWalls(PixelArt& art, const Scheme& scheme)
{
    art.rect(1080, 180, 184, 450, scheme.outline);
    art.rect(1086, 186, 170, 438, scheme.wall);
    for (int i = 0; i < 16; ++i)
    {
        const float y = 196.0f + static_cast<float>(i) * 25.0f;
        art.rect(1086, y, 170, 2, scheme.joints);
        if (scheme.bricks)
            art.rect(1100.0f + static_cast<float>(i % 2) * 46.0f, y + 2, 2, 23, scheme.joints);
        else
            for (int block = 0; block < 4; ++block)
            {
                const float x = 1086.0f + static_cast<float>(block) * 57.0f + (i % 2 ? 28.0f : 0.0f);
                if (x > 1090.0f && x < 1250.0f)
                    art.rect(x, y + 2, 2, 23, scheme.joints);
            }
    }
    art.rect(1070, 615, 202, 15, scheme.roofEdge);
    art.rect(1076, 621, 190, 7, scheme.roofTrim);
}

void drawSign(PixelArt& art, const Scheme& scheme, const TargetLook& look)
{
    const std::string_view sign = look.sign;
    const std::size_t split = sign.find('\n');
    if (split == std::string_view::npos)
    {
        art.rect(1092, 558, 159, 49, scheme.signBoard);
        art.centered(sign, centerX, 580, 2, scheme.signText);
        art.centered(look.tagline, centerX, 566, 1, scheme.tagline);
        return;
    }
    art.rect(1092, 553, 159, 58, scheme.signBoard);
    art.centered(sign.substr(0, split), centerX, 592, 2, scheme.signText);
    art.centered(sign.substr(split + 1), centerX, 575, 2, scheme.signText);
    art.centered(look.tagline, centerX, 560, 1, scheme.tagline);
}

void drawAwning(PixelArt& art, const Scheme& scheme)
{
    art.rect(1082, 519, 178, 31, scheme.awningBase);
    for (int i = 0; i < 9; i += 2)
        art.rect(1082.0f + static_cast<float>(i) * 20.0f, 519, 18, 31, scheme.awning);
    art.rect(1080, 515, 182, 5, scheme.awningEdge);
}

void head(PixelArt& art, float y, std::uint32_t skin)
{
    art.rect(figureX - 11, y + 12, 22, 22, skin);
    art.rect(figureX - 7, y + 23, 3, 3, ink);
    art.rect(figureX + 4, y + 23, 3, 3, ink);
    art.rect(figureX - 3, y + 17, 8, 2, ink);
}

void torso(PixelArt& art, float y, std::uint32_t color)
{
    art.rect(figureX - 16, y - 10, 33, 23, color);
}

void chefHat(PixelArt& art, float y)
{
    art.rect(figureX - 19, y + 32, 38, 10, paper);
    art.rect(figureX - 12, y + 40, 25, 8, paper);
}

// Whoever is working in the middle window.
void drawFigure(PixelArt& art, TargetArt kind, float y, float time)
{
    switch (kind)
    {
    case TargetArt::Chippy:
        torso(art, y, paper);
        head(art, y, 0xe9b891);
        chefHat(art, y);
        art.rect(1200, y - 10, 10, 19, yellow);
        art.rect(1202, y + 9, 6, 4, coral);
        break;
    case TargetArt::Bakery:
        torso(art, y, paper);
        head(art, y, 0xf5c497);
        art.rect(figureX - 13, y + 31, 26, 5, paper);
        art.rect(figureX - 19, y + 36, 38, 11, paper);
        art.rect(figureX - 15, y + 47, 30, 3, paper);
        art.rect(1190, y - 6, 32, 4, 0xb9c2c9);
        for (int i = 0; i < 3; ++i)
        {
            art.rect(1192.0f + static_cast<float>(i) * 10.0f, y - 2, 8, 7, 0xd9a05b);
            art.rect(1193.0f + static_cast<float>(i) * 10.0f, y + 3, 6, 2, 0xf0c27c);
        }
        break;
    case TargetArt::CouncilOffices:
        torso(art, y, 0x7a889c);
        art.rect(figureX - 5, y + 9, 10, 4, paper);
        art.rect(figureX - 2, y - 10, 4, 20, coral);
        head(art, y, 0xf0c8a0);
        art.rect(figureX - 11, y + 30, 22, 4, 0x8b8f96);
        art.frame(figureX - 10, y + 20, 9, 8, ink, 1);
        art.frame(figureX + 1, y + 20, 9, 8, ink, 1);
        art.rect(figureX - 1, y + 24, 2, 1, ink);
        art.rect(1196, y - 6, 22, 28, paper);
        art.rect(1199, y + 16, 16, 2, ink);
        art.rect(1199, y + 11, 16, 2, ink);
        art.rect(1199, y + 6, 10, 2, ink);
        art.rect(1204, y - 3, 10, 7, coral);
        break;
    case TargetArt::Pizzeria:
        torso(art, y, paper);
        art.rect(figureX - 7, y + 8, 14, 5, 0xd9473f);
        head(art, y, 0xe7ac7c);
        art.rect(figureX - 10, y + 18, 20, 4, ink);
        art.rect(figureX - 12, y + 20, 3, 4, ink);
        art.rect(figureX + 9, y + 20, 3, 4, ink);
        chefHat(art, y);
        disc(art, 1206, y + 14, 14, 0xd99b4a);
        disc(art, 1206, y + 14, 11, 0xf3cf6b);
        for (const auto [dx, dy] : {std::pair{-6.0f, 3.0f}, {3.0f, 6.0f}, {-1.0f, -6.0f}, {6.0f, -3.0f}})
            art.rect(1204 + dx, y + 12 + dy, 4, 4, 0xc4452f);
        break;
    case TargetArt::VillageHall:
    {
        // The tea urn is the star of this one.
        art.rect(1122, y - 8, 14, 11, paper);
        art.rect(1136, y - 4, 4, 5, paper);
        art.rect(1146, y - 8, 46, 52, 0xb9c4cb);
        art.rect(1152, y - 4, 8, 44, 0xe3eaee);
        art.rect(1180, y - 4, 8, 44, 0x8d9aa3);
        art.rect(1146, y + 20, 46, 3, 0x8d9aa3);
        art.rect(1142, y + 44, 54, 4, 0x8d9aa3);
        art.rect(1164, y + 48, 10, 2, 0x8d9aa3);
        art.rect(1192, y + 8, 14, 4, 0x6d7a84);
        art.rect(1202, y + 2, 4, 6, 0x6d7a84);
        art.rect(1157, y + 10, 24, 14, paper);
        art.centered("TEA", figureX, y + 14, 1, ink);
        const float fall = std::fmod(time * 1.1f, 1.0f);
        art.rect(1203, y + 1 - fall * 11.0f, 3, 4, blue);
        break;
    }
    case TargetArt::TownHall:
        torso(art, y, 0x7f2e3a);
        art.rect(figureX - 12, y + 7, 24, 2, gold);
        art.rect(figureX - 12, y + 2, 2, 5, gold);
        art.rect(figureX + 10, y + 2, 2, 5, gold);
        art.rect(figureX - 3, y - 2, 6, 6, gold);
        head(art, y, 0xf0c8a0);
        art.rect(figureX - 20, y + 31, 40, 4, 0x1b1b24);
        art.rect(figureX - 12, y + 35, 24, 13, 0x1b1b24);
        art.rect(figureX - 12, y + 35, 24, 3, gold);
        art.rect(1196, y - 6, 22, 28, paper);
        art.rect(1196, y + 16, 22, 6, coral);
        stroke(art, 1200, y - 2, 1214, y + 12, 2, ink);
        stroke(art, 1200, y + 12, 1214, y - 2, 2, ink);
        break;
    }
}

void drawWindows(PixelArt& art, const Scheme& scheme, const TargetLook& look, float time)
{
    for (int lane = 0; lane < config::lanes; ++lane)
    {
        const float y = config::laneY[lane];
        art.rect(1106, y - 19, 123, 76, scheme.windowFrame);
        art.rect(1112, y - 12, 111, 62, scheme.glass);
        art.rect(1117, y + 20, 101, 24, scheme.glassShine);
        art.rect(1100, y - 20, 136, 9, scheme.sill);
        art.rect(1118, y + 16, 4, 28, scheme.glint);
        if (lane == 1)
            drawFigure(art, look.art, y, time);
        else
        {
            const Caption& caption = lane == 0 ? look.upper : look.lower;
            art.centered(caption.big, 1168, y + 10, 2, yellow);
            art.centered(caption.fine, 1168, y - 3, 1, paper);
        }
    }
}

// Whatever sits on the roof, up to the top of the playfield.
void drawRoof(PixelArt& art, TargetArt kind, float time)
{
    switch (kind)
    {
    case TargetArt::Chippy:
        break;
    case TargetArt::Bakery:
        // A giant sausage roll.
        art.rect(1112, 638, 118, 18, 0xe0a75e);
        art.rect(1118, 632, 106, 30, 0xe0a75e);
        art.rect(1118, 656, 106, 6, 0xf0c27c);
        art.rect(1118, 632, 106, 4, 0xb97a3c);
        for (int i = 0; i < 7; ++i)
            art.rect(1128.0f + static_cast<float>(i) * 14.0f, 640, 4, 14, 0xb97a3c);
        art.rect(1108, 642, 4, 10, 0xd08a8a);
        art.rect(1230, 642, 4, 10, 0xd08a8a);
        break;
    case TargetArt::CouncilOffices:
        // A flagpole with a plain pennant, and a telly aerial.
        art.rect(1226, 630, 4, 54, 0x8d939b);
        art.rect(1224, 682, 8, 4, gold);
        art.rect(1230, 660, 28, 14, 0xe9c46a);
        art.rect(1238, 664, 6, 6, paper);
        art.rect(1100, 630, 3, 26, 0x8d939b);
        art.rect(1092, 648, 19, 3, 0x8d939b);
        art.rect(1096, 656, 11, 3, 0x8d939b);
        break;
    case TargetArt::Pizzeria:
        // A pizza on posts.
        art.rect(1156, 630, 4, 10, 0x58261f);
        art.rect(1182, 630, 4, 10, 0x58261f);
        disc(art, centerX, 662, 24, 0xd99b4a);
        disc(art, centerX, 662, 20, 0xf3cf6b);
        for (const auto [dx, dy] : {std::pair{-10.0f, 6.0f}, {6.0f, 8.0f}, {-4.0f, -10.0f}, {9.0f, -8.0f}})
            art.rect(centerX + dx - 4, 662 + dy - 4, 8, 8, 0xc4452f);
        break;
    case TargetArt::VillageHall:
    {
        // Bunting.
        constexpr std::array<std::uint32_t, 4> flags{coral, yellow, mint, blue};
        art.rect(1076, 652, 196, 2, ink);
        for (int i = 0; i < 9; ++i)
        {
            const float x = 1082.0f + static_cast<float>(i) * 20.0f;
            const float sway = std::sin(time * 2.5f + static_cast<float>(i)) > 0.0f ? 1.0f : 0.0f;
            const std::uint32_t color = flags[static_cast<std::size_t>(i) % flags.size()];
            art.rect(x, 649, 14, 3, color);
            art.rect(x + 2 + sway, 646, 10, 3, color);
            art.rect(x + 4 + sway, 643, 6, 3, color);
            art.rect(x + 6 + sway, 640, 2, 3, color);
        }
        break;
    }
    case TargetArt::TownHall:
    {
        // A clock tower whose clock has never been right.
        art.rect(1146, 630, 50, 46, 0x6f6a5c);
        art.rect(1149, 630, 44, 43, 0xe4ddc8);
        art.rect(1144, 674, 54, 4, 0x4b4a52);
        art.rect(1152, 678, 38, 3, 0x4b4a52);
        art.rect(1160, 681, 22, 3, 0x4b4a52);
        art.rect(1167, 684, 8, 2, 0x4b4a52);
        disc(art, centerX, 652, 16, 0x4b4a52);
        disc(art, centerX, 652, 13, paper);
        art.rect(1170, 662, 2, 3, ink);
        art.rect(1170, 639, 2, 3, ink);
        art.rect(1160, 651, 3, 2, ink);
        art.rect(1180, 651, 3, 2, ink);
        const float minute = time * 0.7f;
        const float hour = -time * 0.12f + 2.0f;
        stroke(art, centerX, 652, centerX + std::sin(minute) * 10.0f, 652 + std::cos(minute) * 10.0f, 2, ink);
        stroke(art, centerX, 652, centerX + std::sin(hour) * 7.0f, 652 + std::cos(hour) * 7.0f, 3, coral);
        // Stone pillars down both sides.
        for (const float x : {1088.0f, 1244.0f})
        {
            art.rect(x, 190, 10, 322, 0xece5cf);
            art.rect(x - 2, 504, 14, 6, gold);
            art.rect(x - 2, 190, 14, 6, gold);
        }
        break;
    }
    }
}
}

void drawTarget(PixelArt& art, const TargetLook& look, float time)
{
    const Scheme& scheme = schemeFor(look.art);
    drawWalls(art, scheme);
    if (look.art == TargetArt::TownHall)
        drawRoof(art, look.art, time);
    drawSign(art, scheme, look);
    drawAwning(art, scheme);
    drawWindows(art, scheme, look, time);
    if (look.art != TargetArt::TownHall)
        drawRoof(art, look.art, time);
}
}
