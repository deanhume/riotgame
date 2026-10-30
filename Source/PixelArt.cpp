#include "PixelArt.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <unordered_map>

namespace riot
{
namespace
{
// Five columns by seven rows for capitals, digits and punctuation, plus two rows below the baseline for the
// descenders of g j p q y. Each row is five bits, bit 4 being the leftmost pixel. Rows run top to bottom.
using Glyph = std::array<unsigned char, 9>;

Glyph glyph(char input)
{
    switch (input)
    {
    case 'A': return {14,17,17,31,17,17,17};
    case 'B': return {30,17,17,30,17,17,30};
    case 'C': return {14,17,16,16,16,17,14};
    case 'D': return {30,17,17,17,17,17,30};
    case 'E': return {31,16,16,30,16,16,31};
    case 'F': return {31,16,16,30,16,16,16};
    case 'G': return {14,17,16,23,17,17,15};
    case 'H': return {17,17,17,31,17,17,17};
    case 'I': return {14,4,4,4,4,4,14};
    case 'J': return {7,2,2,2,18,18,12};
    case 'K': return {17,18,20,24,20,18,17};
    case 'L': return {16,16,16,16,16,16,31};
    case 'M': return {17,27,21,21,17,17,17};
    case 'N': return {17,25,25,21,19,19,17};
    case 'O': return {14,17,17,17,17,17,14};
    case 'P': return {30,17,17,30,16,16,16};
    case 'Q': return {14,17,17,17,21,18,13};
    case 'R': return {30,17,17,30,20,18,17};
    case 'S': return {15,16,16,14,1,1,30};
    case 'T': return {31,4,4,4,4,4,4};
    case 'U': return {17,17,17,17,17,17,14};
    case 'V': return {17,17,17,17,17,10,4};
    case 'W': return {17,17,17,21,21,21,10};
    case 'X': return {17,17,10,4,10,17,17};
    case 'Y': return {17,17,10,4,4,4,4};
    case 'Z': return {31,1,2,4,8,16,31};
    case 'a': return {0,0,14,1,15,17,15};
    case 'b': return {16,16,22,25,17,17,30};
    case 'c': return {0,0,14,17,16,17,14};
    case 'd': return {1,1,13,19,17,17,15};
    case 'e': return {0,0,14,17,31,16,14};
    case 'f': return {6,8,30,8,8,8,8};
    case 'g': return {0,0,15,17,17,17,15,1,14};
    case 'h': return {16,16,22,25,17,17,17};
    case 'i': return {4,0,12,4,4,4,14};
    case 'j': return {2,0,6,2,2,2,2,2,12};
    case 'k': return {16,16,18,20,24,20,18};
    case 'l': return {12,4,4,4,4,4,14};
    case 'm': return {0,0,26,21,21,21,21};
    case 'n': return {0,0,22,25,17,17,17};
    case 'o': return {0,0,14,17,17,17,14};
    case 'p': return {0,0,30,17,17,17,30,16,16};
    case 'q': return {0,0,15,17,17,17,15,1,1};
    case 'r': return {0,0,22,25,16,16,16};
    case 's': return {0,0,15,16,14,1,30};
    case 't': return {0,8,30,8,8,9,6};
    case 'u': return {0,0,17,17,17,19,13};
    case 'v': return {0,0,17,17,17,10,4};
    case 'w': return {0,0,17,17,21,21,10};
    case 'x': return {0,0,17,10,4,10,17};
    case 'y': return {0,0,17,17,17,15,1,1,14};
    case 'z': return {0,0,31,2,4,8,31};
    case '0': return {14,17,19,21,25,17,14};
    case '1': return {4,12,4,4,4,4,14};
    case '2': return {14,17,1,2,4,8,31};
    case '3': return {30,1,1,14,1,1,30};
    case '4': return {2,6,10,18,31,2,2};
    case '5': return {31,16,16,30,1,1,30};
    case '6': return {14,16,16,30,17,17,14};
    case '7': return {31,1,2,4,8,8,8};
    case '8': return {14,17,17,14,17,17,14};
    case '9': return {14,17,17,15,1,1,14};
    case '!': return {4,4,4,4,4,0,4};
    case '?': return {14,17,1,2,4,0,4};
    case '$': return {4,15,20,14,5,30,4};
    case '%': return {25,25,2,4,8,19,19};
    case '+': return {0,4,4,31,4,4,0};
    case '-': return {0,0,0,31,0,0,0};
    case '.': return {0,0,0,0,0,12,12};
    case ',': return {0,0,0,0,0,4,8};
    case ':': return {0,12,12,0,12,12,0};
    case '/': return {1,1,2,4,8,16,16};
    case '>': return {16,8,4,2,4,8,16};
    case '<': return {1,2,4,8,4,2,1};
    case '[': return {14,8,8,8,8,8,14};
    case ']': return {14,2,2,2,2,2,14};
    case '\'': return {4,4,8,0,0,0,0};
    case '(': return {2,4,8,8,8,4,2};
    case ')': return {8,4,2,2,2,4,8};
    case ' ': return {};
    default: return {14,17,1,2,4,0,4};
    }
}

// The micro font: five rows, each `width` bits wide with the leftmost pixel in the highest bit.
struct MicroGlyph
{
    int width;
    std::array<unsigned char, 5> rows;
};

MicroGlyph microGlyph(char input)
{
    switch (std::toupper(static_cast<unsigned char>(input)))
    {
    case 'A': return {3, {2,5,7,5,5}};
    case 'B': return {3, {6,5,6,5,6}};
    case 'C': return {3, {3,4,4,4,3}};
    case 'D': return {3, {6,5,5,5,6}};
    case 'E': return {3, {7,4,6,4,7}};
    case 'F': return {3, {7,4,6,4,4}};
    case 'G': return {3, {3,4,5,5,3}};
    case 'H': return {3, {5,5,7,5,5}};
    case 'I': return {3, {7,2,2,2,7}};
    case 'J': return {3, {1,1,1,5,2}};
    case 'K': return {3, {5,5,6,5,5}};
    case 'L': return {3, {4,4,4,4,7}};
    case 'M': return {5, {17,27,21,17,17}};
    case 'N': return {4, {9,13,11,9,9}};
    case 'O': return {3, {2,5,5,5,2}};
    case 'P': return {3, {6,5,6,4,4}};
    case 'Q': return {3, {2,5,5,6,3}};
    case 'R': return {3, {6,5,6,5,5}};
    case 'S': return {3, {3,4,2,1,6}};
    case 'T': return {3, {7,2,2,2,2}};
    case 'U': return {3, {5,5,5,5,7}};
    case 'V': return {3, {5,5,5,5,2}};
    case 'W': return {5, {17,17,21,27,17}};
    case 'X': return {3, {5,5,2,5,5}};
    case 'Y': return {3, {5,5,2,2,2}};
    case 'Z': return {3, {7,1,2,4,7}};
    case '0': return {3, {7,5,5,5,7}};
    case '1': return {3, {2,6,2,2,7}};
    case '2': return {3, {6,1,2,4,7}};
    case '3': return {3, {6,1,2,1,6}};
    case '4': return {3, {5,5,7,1,1}};
    case '5': return {3, {7,4,6,1,6}};
    case '6': return {3, {3,4,6,5,2}};
    case '7': return {3, {7,1,2,2,2}};
    case '8': return {3, {2,5,2,5,2}};
    case '9': return {3, {2,5,3,1,6}};
    case '!': return {1, {1,1,1,0,1}};
    case '?': return {3, {6,1,2,0,2}};
    case '\'': return {1, {1,1,0,0,0}};
    case '.': return {1, {0,0,0,0,1}};
    case ',': return {2, {0,0,0,1,2}};
    case ':': return {1, {0,1,0,1,0}};
    case '(': return {2, {1,2,2,2,1}};
    case ')': return {2, {2,1,1,1,2}};
    case '-': return {3, {0,0,7,0,0}};
    case '/': return {3, {1,1,2,4,4}};
    case '&': return {3, {2,5,2,5,3}};
    case ' ': return {2, {}};
    default: return {3, {6,1,2,0,2}};
    }
}

std::uint32_t darken(std::uint32_t color, float amount)
{
    const auto channel = [&](int shift) {
        return static_cast<std::uint32_t>(static_cast<float>((color >> shift) & 255) * amount) << shift;
    };
    return channel(16) | channel(8) | channel(0);
}

struct ConeColours
{
    std::uint32_t base;
    std::uint32_t body;
    std::uint32_t stripe;
};

ConeColours coneColours(ConeArt style)
{
    switch (style)
    {
    case ConeArt::Stripy: return {0x9c2f45, 0xe0524f, 0xfdf3f3};
    case ConeArt::Gold: return {0x9a7520, 0xf2c84b, 0xfff3b0};
    default: return {0xa94836, 0xf58c4c, palette::paper};
    }
}

// Greedy word wrap with balanced line lengths, for any font: `measure` gives a string's width in pixels and
// `step` is how much to shrink the width by while looking for a more even layout.
template <typename Measure>
std::vector<std::string> wrapText(std::string_view value, float maxWidth, float step, Measure measure)
{
    std::vector<std::string> lines;
    std::size_t begin = 0;
    while (begin <= value.size())
    {
        std::size_t end = value.find('\n', begin);
        if (end == std::string_view::npos)
            end = value.size();
        const std::string_view paragraph = value.substr(begin, end - begin);
        begin = end + 1;

        std::vector<std::string_view> words;
        for (std::size_t from = 0; from < paragraph.size();)
        {
            const std::size_t space = paragraph.find(' ', from);
            const std::size_t stop = space == std::string_view::npos ? paragraph.size() : space;
            if (stop > from)
                words.push_back(paragraph.substr(from, stop - from));
            from = stop + 1;
        }

        const auto fill = [&](float width) {
            std::vector<std::string> result;
            for (const auto word : words)
            {
                if (!result.empty() && measure(result.back() + ' ' + std::string(word)) <= width)
                {
                    result.back() += ' ';
                    result.back() += word;
                }
                else
                    result.emplace_back(word);
            }
            return result;
        };

        std::vector<std::string> best = fill(maxWidth);
        // Shrink the width while the line count stays the same, so the lines come out about equally long.
        for (float width = maxWidth - step; width > 0.0f && !best.empty(); width -= step)
        {
            auto attempt = fill(width);
            if (attempt.size() != best.size())
                break;
            best = std::move(attempt);
        }
        if (best.empty())
            best.emplace_back();
        lines.insert(lines.end(), best.begin(), best.end());
        if (end == value.size())
            break;
    }
    return lines;
}

// Placard and flag slogans are string literals, so their wrapped lines can be kept by address.
const std::vector<std::string>& microLines(const char* text, float maxWidth)
{
    static std::unordered_map<const char*, std::vector<std::string>> cache;
    auto it = cache.find(text);
    if (it == cache.end())
        it = cache.emplace(text, PixelArt::microWrap(text, maxWidth)).first;
    return it->second;
}

constexpr float placardWidth = 44.0f;  // widest placard text, in micro-font pixels
constexpr float flagTextWidth = 54.0f;
}

void PixelArt::rect(float x, float y, float w, float h, std::uint32_t color, float alpha)
{
    const ax::Color4F rgba(
        static_cast<float>((color >> 16) & 255) / 255.0f,
        static_cast<float>((color >> 8) & 255) / 255.0f,
        static_cast<float>(color & 255) / 255.0f, alpha * opacity_);
    node_->drawSolidRect({std::round(x), std::round(y)}, {std::round(x + w), std::round(y + h)}, rgba);
}

void PixelArt::frame(float x, float y, float w, float h, std::uint32_t color, float thickness)
{
    rect(x, y, w, thickness, color);
    rect(x, y + h - thickness, w, thickness, color);
    rect(x, y, thickness, h, color);
    rect(x + w - thickness, y, thickness, h, color);
}

float PixelArt::textWidth(std::string_view value, int scale)
{
    return value.empty() ? 0.0f : static_cast<float>((value.size() * 6 - 1) * scale);
}

void PixelArt::text(std::string_view value, float x, float y, int scale, std::uint32_t color)
{
    const float size = static_cast<float>(scale);
    for (char letter : value)
    {
        const Glyph rows = glyph(letter);
        for (int row = 0; row < 9; ++row)
        {
            const float top = y + static_cast<float>(6 - row) * size;
            for (int col = 0; col < 5;)
            {
                if (!(rows[row] & (1 << (4 - col))))
                {
                    ++col;
                    continue;
                }
                const int start = col;
                while (col < 5 && (rows[row] & (1 << (4 - col))))
                    ++col;
                rect(x + static_cast<float>(start) * size, top, static_cast<float>(col - start) * size, size, color);
            }
        }
        x += size * 6.0f;
    }
}

std::vector<std::string> PixelArt::wrap(std::string_view value, float maxWidth, int scale)
{
    return wrapText(value, maxWidth, 6.0f * static_cast<float>(scale),
                    [scale](const std::string& line) { return textWidth(line, scale); });
}

std::vector<std::string> PixelArt::microWrap(std::string_view value, float maxWidth)
{
    return wrapText(value, maxWidth, 1.0f, [](const std::string& line) { return microWidth(line, 1); });
}

float PixelArt::microWidth(std::string_view value, int scale)
{
    int width = 0;
    for (char letter : value)
        width += microGlyph(letter).width + 1;
    return static_cast<float>(std::max(0, width - 1) * scale);
}

void PixelArt::micro(std::string_view value, float x, float y, int scale, std::uint32_t color)
{
    const float size = static_cast<float>(scale);
    for (char letter : value)
    {
        const MicroGlyph glyph = microGlyph(letter);
        for (int row = 0; row < 5; ++row)
        {
            const int bits = glyph.rows[row];
            const float top = y + static_cast<float>(4 - row) * size;
            for (int col = 0; col < glyph.width;)
            {
                const int mask = 1 << (glyph.width - 1 - col);
                if (!(bits & mask))
                {
                    ++col;
                    continue;
                }
                const int start = col;
                while (col < glyph.width && (bits & (1 << (glyph.width - 1 - col))))
                    ++col;
                rect(x + static_cast<float>(start) * size, top, static_cast<float>(col - start) * size, size, color);
            }
        }
        x += static_cast<float>(glyph.width + 1) * size;
    }
}

void PixelArt::centered(std::string_view value, float x, float y, int scale, std::uint32_t color)
{
    text(value, x - textWidth(value, scale) * 0.5f, y, scale, color);
}

void PixelArt::flag(const flavour::FlagDef& def, float poleX, float top, float unit, bool trailLeft, float time)
{
    using flavour::FlagArt;
    int width = 28;
    int height = 18;
    const std::vector<std::string>* lines = nullptr;
    const int textScale = std::max(1, static_cast<int>(unit));
    if (def.text)
    {
        lines = &microLines(def.text, flagTextWidth);
        float textW = 0.0f;
        for (const auto& line : *lines)
            textW = std::max(textW, microWidth(line, 1));
        width = static_cast<int>(textW) + 6;
        height = static_cast<int>(lines->size()) * 6 + 5;
    }
    const float left = std::round(trailLeft ? poleX - static_cast<float>(width) * unit : poleX);
    const float bottom = std::round(top - static_cast<float>(height) * unit);
    // Flags with writing on them are starched stiff so you can read them.
    const bool stiff = lines != nullptr;
    const auto wave = [&](int column) {
        if (stiff)
            return 0;
        const float distance = static_cast<float>(trailLeft ? width - column : column);
        return static_cast<int>(std::lround(std::sin(time * 7.0f - distance * 0.45f) * distance /
                                            static_cast<float>(width) * 1.6f));
    };
    // A rectangle on the cloth, in cloth units from the bottom-left; it ripples with the flag.
    const auto cloth = [&](int cx, int cy, int w, int h, std::uint32_t color, float alpha = 1.0f) {
        for (int column = cx; column < cx + w;)
        {
            const int offset = wave(column);
            int stop = column + 1;
            while (stop < cx + w && wave(stop) == offset)
                ++stop;
            rect(left + static_cast<float>(column) * unit, bottom + static_cast<float>(cy + offset) * unit,
                 static_cast<float>(stop - column) * unit, static_cast<float>(h) * unit, color, alpha);
            column = stop;
        }
    };

    if (def.field == flavour::FlagField::Rainbow)
    {
        const int bands = static_cast<int>(flavour::rainbowStripes.size());
        for (int band = 0; band < bands; ++band)
        {
            const int upper = height - band * height / bands;
            const int lower = height - (band + 1) * height / bands;
            cloth(0, lower, width, upper - lower, flavour::rainbowStripes[band]);
        }
    }
    else
        cloth(0, 0, width, height, def.colour);
    cloth(0, 0, width, 1, darken(def.colour, 0.8f));

    switch (def.art)
    {
    case FlagArt::SausageRoll:
        cloth(6, 7, 1, 4, 0xe7a94a);
        cloth(7, 6, 14, 6, 0xe7a94a);
        cloth(21, 7, 1, 4, 0xe7a94a);
        cloth(8, 11, 12, 1, 0xf7d48a);
        for (int flake = 10; flake < 20; flake += 4)
            cloth(flake, 7, 1, 3, 0xb87430);
        cloth(5, 8, 1, 2, 0xb5654a);
        cloth(22, 8, 1, 2, 0xb5654a);
        break;
    case FlagArt::Teapot:
        cloth(9, 4, 10, 8, 0x35588a);
        cloth(10, 12, 8, 1, 0x35588a);
        cloth(12, 13, 4, 1, 0x35588a);
        cloth(13, 14, 2, 1, palette::yellow);
        cloth(19, 7, 2, 2, 0x35588a);
        cloth(21, 9, 2, 2, 0x35588a);
        cloth(7, 6, 1, 5, 0x35588a);
        cloth(8, 10, 1, 1, 0x35588a);
        cloth(8, 5, 1, 1, 0x35588a);
        cloth(11, 9, 2, 2, 0xffffff);
        cloth(10, 4, 8, 1, 0x26406a);
        break;
    case FlagArt::NoPineapple:
        cloth(11, 3, 6, 8, 0xf2c84b);
        cloth(12, 4, 1, 1, 0xb8862a);
        cloth(15, 4, 1, 1, 0xb8862a);
        cloth(13, 6, 2, 1, 0xb8862a);
        cloth(12, 8, 1, 1, 0xb8862a);
        cloth(15, 8, 1, 1, 0xb8862a);
        cloth(12, 11, 1, 3, 0x4c9a4a);
        cloth(14, 11, 1, 4, 0x4c9a4a);
        cloth(16, 11, 1, 3, 0x4c9a4a);
        cloth(8, 1, 12, 1, 0xd23c3c);
        cloth(8, 16, 12, 1, 0xd23c3c);
        cloth(6, 3, 1, 12, 0xd23c3c);
        cloth(21, 3, 1, 12, 0xd23c3c);
        cloth(7, 2, 1, 1, 0xd23c3c);
        cloth(20, 2, 1, 1, 0xd23c3c);
        cloth(7, 15, 1, 1, 0xd23c3c);
        cloth(20, 15, 1, 1, 0xd23c3c);
        for (int step = 0; step < 12; ++step)
            cloth(8 + step, 14 - step, 2, 1, 0xd23c3c);
        break;
    case FlagArt::BinLid:
        cloth(9, 3, 10, 1, 0x6c7780);
        cloth(8, 4, 12, 8, 0x8e9aa3);
        cloth(9, 12, 10, 1, 0x8e9aa3);
        cloth(9, 7, 10, 1, 0x6c7780);
        cloth(10, 10, 8, 1, 0xc5ced4);
        cloth(12, 13, 4, 2, 0x4a535b);
        cloth(13, 13, 2, 1, 0x8e9aa3);
        break;
    case FlagArt::SausageDog:
        cloth(4, 11, 1, 1, 0x8a4b2a);
        cloth(5, 10, 2, 1, 0x8a4b2a);
        cloth(7, 7, 14, 4, 0x8a4b2a);
        cloth(8, 5, 1, 2, 0x5e3019);
        cloth(10, 5, 1, 2, 0x5e3019);
        cloth(17, 5, 1, 2, 0x5e3019);
        cloth(19, 5, 1, 2, 0x5e3019);
        cloth(21, 9, 4, 4, 0x8a4b2a);
        cloth(25, 9, 2, 2, 0x8a4b2a);
        cloth(26, 10, 1, 1, palette::ink);
        cloth(21, 9, 1, 4, 0x5e3019);
        cloth(23, 11, 1, 1, palette::ink);
        cloth(11, 9, 6, 1, 0xd65a5a);
        break;
    case FlagArt::Neutral:
        break;
    }

    if (lines)
    {
        const float size = static_cast<float>(textScale);
        for (std::size_t i = 0; i < lines->size(); ++i)
        {
            const std::string& line = (*lines)[i];
            const float lineX = std::round(left + (static_cast<float>(width) * unit - microWidth(line, textScale)) * 0.5f);
            const float lineY = bottom + (3.0f + static_cast<float>(lines->size() - 1 - i) * 6.0f) * size;
            micro(line, lineX, lineY, textScale, 0x5a4a32);
        }
    }
}

void PixelArt::walker(float x, float y, const WalkerPose& pose, float scale)
{
    using flavour::PropKind;
    constexpr std::array<std::uint32_t, 6> shirts{0xdf7770, 0x8bbdb7, 0xb5a0d7, 0xe1b958, 0x729ec5, 0xc39272};
    constexpr std::array<std::uint32_t, 4> skins{0xf5c497, 0xbb805c, 0xe7ac7c, 0x8f6047};
    constexpr std::array<std::uint32_t, 4> hairs{0x57433d, 0x2b2730, 0xc0803a, 0x9a9a9a};
    constexpr std::array<std::uint32_t, 3> legs{0x394b67, 0x4f4a3f, 0x2f5a4c};
    constexpr std::array<std::uint32_t, 4> beanies{0x53667e, 0xd65a5a, 0x5fa86b, 0xe0a83a};
    const std::uint32_t look = pose.look;
    const auto shirt = shirts[look % shirts.size()];
    const auto skin = skins[(look >> 3) % skins.size()];
    const auto hair = hairs[(look >> 5) % hairs.size()];
    const auto trousers = legs[(look >> 7) % legs.size()];
    const int hairStyle = static_cast<int>((look >> 9) % 3);
    const float phase = static_cast<float>(look & 255) * 0.1f;
    const float dir = pose.leaving ? -1.0f : 1.0f;
    const float time = pose.time;

    float bob = std::abs(std::sin(time * 9 + phase));
    if (pose.blocked)
    {
        x += std::sin(time * 18 + phase) * 2.0f;
        bob *= 1.5f;
    }
    rect(x - 7 * scale, y - 3, 15 * scale, 5, 0x1e303d, 0.45f);

    if (pose.stumble > 0.0f)
    {
        // Flat on their face, pointing the way they were going.
        const auto lie = [&](float dx, float dy, float w, float h, std::uint32_t c) {
            rect(x + (dir > 0 ? dx : -(dx + w)), y + dy, w, h, c);
        };
        lie(-20, 5, 8, 5, palette::ink);
        lie(-15, 3, 10, 10, trousers);
        lie(-6, 3, 19, 12, shirt);
        if (pose.balaclava)
        {
            lie(10, 7, 12, 12, flavour::balaclava);
            lie(16, 11, 6, 5, skin);
        }
        else
            lie(10, 7, 12, 12, skin);
        lie(19, 13, 3, 3, palette::ink);
        text("!", dir > 0 ? x + 22 : x - 32, y + 30, 2, palette::yellow);
        return;
    }

    const float sx = scale * (1.0f + 0.3f * pose.squash);
    const float sy = scale * (1.0f - 0.3f * pose.squash);
    // A rectangle in body units, drawn mirrored when the rioter faces left.
    const auto px = [&](float dx, float dy, float w, float h, std::uint32_t c) {
        const float left = dir > 0 ? dx : -(dx + w);
        rect(x + left * sx, y + (dy + bob) * sy, w * sx, h * sy, c);
    };

    const float stride = std::sin(time * (pose.bored ? 6.0f : 10.0f) + phase) > 0 ? 1.0f : -1.0f;
    px(-5 + stride, 0, 4, 2, palette::ink);
    px(1 - stride, 0, 4, 2, palette::ink);
    px(-4, 2, 3, 4, trousers);
    px(1, 2, 3, 4, trousers);
    px(-5, 6, 10, 8, shirt);
    px(-5, 6, 10, 1, darken(shirt, 0.75f));
    px(-6, 8, 2, 5, skin);
    if (pose.throwing)
    {
        // Throwing arm up and forward after the throw, sleeved so it stands out from the face.
        px(5, 10, 2, 4, shirt);
        px(6, 14, 2, 3, shirt);
        px(7, 17, 2, 3, skin);
    }
    else
        px(5, 8, 2, 5, skin);
    if (pose.balaclava)
    {
        // Black knit over the whole head and nose; only a slit for the eyes.
        px(-4, 14, 8, 8, flavour::balaclava);
        px(-3, 22, 6, 1, flavour::balaclava);
        px(4, 15, 1, 3, flavour::balaclava);
        px(-2, 21, 2, 1, flavour::balaclavaKnit);
        px(-4, 16, 1, 3, flavour::balaclavaKnit);
        px(-1, 15, 1, 1, flavour::balaclavaKnit);
        px(0, 17, 5, 2, skin);
    }
    else
    {
        px(-4, 14, 8, 7, skin);
        if (hairStyle == 0)
        {
            px(-5, 19, 2, 4, hair);
            px(-4, 21, 9, 2, hair);
        }
        else if (hairStyle == 1)
        {
            const auto beanie = beanies[(look >> 11) % beanies.size()];
            px(-5, 20, 10, 4, beanie);
            px(-5, 20, 10, 1, darken(beanie, 0.7f));
            px(-1, 24, 2, 1, palette::paper);
        }
        else
        {
            px(-5, 18, 2, 4, hair);
            px(-4, 21, 6, 1, hair);
        }
        px(4, 16, 2, 2, skin);
    }

    if (pose.surprised)
    {
        px(1, 17, 2, 3, 0xffffff);
        px(2, 18, 1, 1, palette::ink);
        px(2, 14, 2, 2, palette::ink);
    }
    else if (pose.bored)
    {
        px(1, 17, 2, 1, palette::ink);
        px(1, 15, 3, 1, palette::ink);
    }
    else
    {
        px(2, 17, 1, 2, palette::ink);
        px(2, 15, 2, 1, palette::ink);
        if (pose.blocked)
            px(1, 19, 3, 1, palette::ink);
    }

    if (pose.cone)
    {
        const ConeColours colours = coneColours(pose.coneStyle);
        px(-5, 21, 10, 2, colours.base);
        px(-4, 23, 8, 2, colours.body);
        px(-3, 25, 6, 2, colours.stripe);
        px(-2, 27, 4, 2, colours.body);
        px(-1, 29, 2, 2, colours.body);
        if (pose.coneStyle == ConeArt::Sunglasses)
            px(-3, 25, 6, 1, 0x14141c);
        else if (pose.coneStyle == ConeArt::Face)
        {
            px(-2, 25, 1, 1, palette::ink);
            px(1, 25, 1, 1, palette::ink);
        }
    }

    const float lift = static_cast<float>(pose.prop.lift) * 3.0f;
    // The pole sits in the back hand; these are its screen edges.
    const float poleLeft = x + (dir > 0 ? -9.0f : 8.0f) * sx;
    switch (pose.prop.kind)
    {
    case PropKind::Placard:
        px(-9, 9, 1, 19 + lift, 0x9a7354);
        if (!pose.prop.text)
        {
            px(-16, 26 + lift, 18, 12, 0x936d50);
            px(-15, 27 + lift, 16, 10, 0xe8c18c);
            px(-13, 34 + lift, 10, 1, 0x675343);
            px(-12, 31 + lift, look % 2 ? 9.0f : 6.0f, 1, 0x675343);
            px(-13, 29 + lift, 11, 1, 0x675343);
        }
        else
        {
            const int size = std::max(1, static_cast<int>(std::lround(scale / 2)));
            const float unit = static_cast<float>(size);
            const auto& lines = microLines(pose.prop.text, placardWidth);
            float textW = 0.0f;
            for (const auto& line : lines)
                textW = std::max(textW, microWidth(line, size));
            const float pad = 2.0f * unit;
            const float boardW = textW + 2.0f * pad;
            const float boardH = static_cast<float>(lines.size()) * 6.0f * unit - unit + 2.0f * pad;
            const float centre = poleLeft + sx * 0.5f;
            const float left = std::round(centre - boardW * 0.5f);
            const float bottom = std::round(y + (26.0f + lift + bob) * sy);
            const auto board = flavour::boards[static_cast<std::size_t>(pose.prop.board) % flavour::boards.size()];
            const auto ink = flavour::inks[static_cast<std::size_t>(pose.prop.ink) % flavour::inks.size()];
            rect(left - unit, bottom - unit, boardW + 2.0f * unit, boardH + 2.0f * unit, darken(board, 0.6f));
            rect(left, bottom, boardW, boardH, board);
            for (std::size_t i = 0; i < lines.size(); ++i)
            {
                const float lineX = std::round(centre - microWidth(lines[i], size) * 0.5f);
                const float lineY = bottom + pad + static_cast<float>(lines.size() - 1 - i) * 6.0f * unit;
                micro(lines[i], lineX, lineY, size, ink);
            }
        }
        break;
    case PropKind::Flag:
    {
        const float top = 40.0f + lift;
        px(-9, 9, 1, top - 9, 0x9a7354);
        px(-10, top, 3, 2, 0xd9b45a);
        const bool trailLeft = !pose.leaving;
        const float unit = std::max(1.0f, std::round(scale / 2));
        const auto& def = flavour::flags[static_cast<std::size_t>(pose.prop.flag) % flavour::flags.size()];
        flag(def, trailLeft ? poleLeft : poleLeft + sx, y + (top + bob) * sy, unit, trailLeft, time + phase);
        break;
    }
    case PropKind::Inflatable:
    {
        // A sad deflated dinosaur, dragged along by its tail.
        const float sway = std::round(std::sin(time * 3 + phase));
        constexpr std::uint32_t green = 0x7ccf6a, shade = 0x4f9a45, belly = 0xc9e8a0;
        px(-8, 8, 3, 2, green);
        px(-11 + sway, 5, 4, 4, green);
        px(-17 + sway, 1, 8, 7, green);
        px(-16 + sway, 1, 6, 2, belly);
        px(-15 + sway, 8, 2, 1, shade);
        px(-12 + sway, 8, 2, 1, shade);
        px(-15 + sway, 0, 1, 1, shade);
        px(-11 + sway, 0, 1, 1, shade);
        px(-23 + sway, 0, 7, 4, green);
        px(-23 + sway, 0, 7, 1, shade);
        px(-20 + sway, 2, 1, 1, palette::ink);
        break;
    }
    case PropKind::Nothing:
        break;
    }

    if (pose.sweat)
    {
        px(6, 19, 2, 2, 0x9fdcff);
        px(7, 21, 1, 1, 0x9fdcff);
    }
    if (pose.exclaim)
    {
        const float centre = x + dir * 8.0f * sx;
        const float base = y + (25.0f + bob) * sy;
        text("!", centre - 3, base - 2, 2, palette::ink);
        text("!", centre - 5, base, 2, palette::yellow);
    }
}

void PixelArt::gadget(GadgetType type, float x, float y, float health, float time, float scale, ConeArt cone)
{
    auto px = [&](float dx, float dy, float w, float h, std::uint32_t c) {
        rect(x + dx * scale, y + dy * scale, w * scale, h * scale, c);
    };
    if (type == GadgetType::Cone)
    {
        const ConeColours colours = coneColours(cone);
        px(-16, -3, 32, 5, 0x243742);
        px(-15, 0, 30, 5, colours.base);
        px(-12, 5, 24, 5, colours.body);
        px(-10, 10, 20, 7, colours.stripe);
        px(-7, 17, 14, 7, colours.body);
        px(-5, 24, 10, 5, colours.stripe);
        px(-3, 29, 6, 7, colours.body);
        switch (cone)
        {
        case ConeArt::Stripy:
            px(-11, 7, 22, 1, colours.stripe);
            px(-6, 20, 12, 1, colours.stripe);
            px(-3, 32, 6, 1, colours.stripe);
            break;
        case ConeArt::Gold:
            if (std::sin(time * 4.0f + x * 0.01f) > 0.3f)
            {
                px(6, 28, 1, 5, 0xffffff);
                px(4, 30, 5, 1, 0xffffff);
            }
            px(-2, 30, 1, 4, 0xfff3b0);
            break;
        case ConeArt::Sunglasses:
            px(-7, 19, 6, 3, 0x14141c);
            px(1, 19, 6, 3, 0x14141c);
            px(-1, 20, 2, 1, 0x14141c);
            px(-6, 21, 1, 1, 0xffffff);
            px(2, 21, 1, 1, 0xffffff);
            break;
        case ConeArt::Face:
            px(-4, 20, 2, 2, palette::ink);
            px(2, 20, 2, 2, palette::ink);
            px(-3, 18, 6, 1, palette::ink);
            px(-4, 19, 1, 1, palette::ink);
            px(3, 19, 1, 1, palette::ink);
            break;
        case ConeArt::Classic:
            break;
        }
        return;
    }
    if (type == GadgetType::Barrier)
    {
        const float bend = health < 0.3f ? 7.0f : (health < 0.65f ? 3.0f : 0.0f);
        px(-27, -3, 54, 5, 0x243742);
        px(-23, 0, 5, 38, 0x9cbcc5);
        px(18, 0, 5, 38, 0x9cbcc5);
        px(-27, 0, 13, 4, 0xcfe2de);
        px(14, 0, 13, 4, 0xcfe2de);
        px(-25, 26, 50, 11, 0xcfe2de);
        px(-25, 12, 50, 6, 0xa3bbc3);
        px(-13, 26 - bend, 26, 10, palette::yellow);
        for (int i = 0; i < 6; ++i)
            px(-23 + static_cast<float>(i) * 8, 28 - (i > 1 && i < 4 ? bend : 0), 4, 7, 0x344551);
        px(-22, 38, 44, 3, 0xf1f1d7);
        if (health < 0.3f)
            px(-2, 12, 5, 12, 0x344551);
        return;
    }
    px(-38, -4, 76, 6, 0x243742);
    px(-33, 0, 14, 14, palette::dark);
    px(19, 0, 14, 14, palette::dark);
    px(-29, 4, 6, 6, 0x9bacb2);
    px(23, 4, 6, 6, 0x9bacb2);
    px(-37, 12, 74, 29, 0xc3d9d7);
    px(-33, 40, 53, 14, 0xe7eddb);
    px(-36, 18, 72, 10, 0x476eb0);
    for (int i = 0; i < 5; ++i)
        px(-29 + static_cast<float>(i) * 14, 18, 7, 5, palette::yellow);
    px(-26, 35, 25, 13, 0x386278);
    px(7, 34, 19, 14, 0x386278);
    px(-23, 44, 18, 2, 0x8cbdc7);
    px(-37, 29, 5, 7, palette::coral);
    px(31, 28, 6, 8, palette::yellow);
    px(-7, 54, 19, 4, palette::ink);
    px(-6, 58, 8, 5, static_cast<int>(time * 3) % 2 ? palette::blue : palette::paper);
    px(3, 58, 8, 5, static_cast<int>(time * 3) % 2 ? palette::paper : palette::coral);
    if (health < 0.4f)
    {
        px(17, 29, 2, 13, palette::ink);
        px(13, 34, 7, 3, palette::ink);
    }
}

void PixelArt::wrench(float x, float y, std::uint32_t color)
{
    rect(x - 4, y - 12, 8, 26, color);
    rect(x - 12, y + 10, 24, 7, color);
    rect(x - 12, y + 17, 7, 12, color);
    rect(x + 5, y + 17, 7, 12, color);
    rect(x - 2, y - 9, 4, 4, palette::ink);
}

namespace
{
// Which of four poses a tumbling object is in: 0 flat, 1 leaning "/", 2 upright, 3 leaning "\".
int tumble(float spin)
{
    const int step = static_cast<int>(std::floor(spin * 4.0f));
    return ((step % 4) + 4) % 4;
}
}

void PixelArt::brick(float x, float y, float spin, float alpha)
{
    constexpr std::uint32_t body = 0xb9553d, light = 0xdd8466, shade = 0x7a3226;
    switch (const int pose = tumble(spin))
    {
    case 0:
        rect(x - 6, y - 3, 12, 6, body, alpha);
        rect(x - 6, y + 1, 12, 2, light, alpha);
        rect(x - 6, y - 3, 12, 1, shade, alpha);
        rect(x + 5, y - 2, 1, 3, shade, alpha);
        rect(x - 3, y - 1, 2, 1, shade, alpha);
        break;
    case 2:
        rect(x - 3, y - 6, 6, 12, body, alpha);
        rect(x - 3, y - 6, 2, 12, light, alpha);
        rect(x - 3, y - 6, 6, 1, shade, alpha);
        rect(x + 2, y - 5, 1, 11, shade, alpha);
        break;
    default:
    {
        // A staircase of squares, leaning one way or the other.
        const float lean = pose == 1 ? 1.0f : -1.0f;
        for (int i = 0; i < 4; ++i)
        {
            const float step = static_cast<float>(i) * 3.0f - 6.0f;
            rect(x + step * lean - 2, y + step - 2, 5, 5, body, alpha);
        }
        for (int i = 0; i < 4; ++i)
        {
            const float step = static_cast<float>(i) * 3.0f - 6.0f;
            rect(x + step * lean - 2, y + step + 2, 3, 1, light, alpha);
            rect(x + step * lean, y + step - 2, 3, 1, shade, alpha);
        }
        break;
    }
    }
}

void PixelArt::smokeBomb(float x, float y, float spin, std::uint32_t colour, float alpha)
{
    constexpr std::uint32_t tin = 0x3a4352, cap = 0xb8bfca, burnt = 0x5e2c26;
    switch (const int pose = tumble(spin))
    {
    case 0:
        rect(x - 7, y - 3, 14, 6, tin, alpha);
        rect(x - 7, y + 1, 14, 1, 0x5d6878, alpha);
        rect(x - 2, y - 3, 4, 6, colour, alpha);
        rect(x - 7, y - 3, 2, 6, cap, alpha);
        rect(x + 5, y - 2, 2, 4, burnt, alpha);
        break;
    case 2:
        rect(x - 3, y - 7, 6, 14, tin, alpha);
        rect(x - 3, y - 2, 6, 4, colour, alpha);
        rect(x - 3, y - 7, 6, 2, cap, alpha);
        rect(x - 2, y + 5, 4, 2, burnt, alpha);
        break;
    default:
    {
        const float lean = pose == 1 ? 1.0f : -1.0f;
        for (int i = 0; i < 4; ++i)
        {
            const float step = static_cast<float>(i) * 3.0f - 6.0f;
            rect(x + step * lean - 2, y + step - 2, 5, 5, i == 0 ? cap : i == 2 ? colour : tin, alpha);
        }
        break;
    }
    }
}

void PixelArt::flare(float x, float y, std::uint32_t colour, float time, float strength)
{
    if (strength <= 0.0f)
        return;
    const float flicker = 0.7f + 0.3f * std::sin(time * 47.0f) * std::sin(time * 23.0f + 1.3f);
    const float glow = std::round((9.0f + 7.0f * flicker) * strength);
    rect(x - glow, y - glow * 0.7f, glow * 2.0f, glow * 1.4f, colour, 0.2f * strength);
    rect(x - glow * 0.6f, y - glow * 0.45f, glow * 1.2f, glow * 0.9f, colour, 0.35f * strength);
    rect(x - 3, y - 3, 6, 6, 0xfff3b8, 0.9f * strength);
    rect(x - 2, y - 1 - flicker, 4, 4, 0xffffff, strength);
}

void PixelArt::puff(float x, float y, float radius, std::uint32_t colour, float alpha)
{
    // A pixel circle in five bands that don't overlap, so its transparency stays even. Everything snaps to a
    // two-pixel grid to stay chunky.
    const float r = std::max(2.0f, std::round(radius * 0.5f) * 2.0f);
    const float cx = std::round(x * 0.5f) * 2.0f;
    const float cy = std::round(y * 0.5f) * 2.0f;
    const float inner = std::round(r * 0.45f);
    const float outer = std::round(r * 0.8f);
    const float wide = std::round(r * 0.8f);
    const float narrow = std::round(r * 0.45f);
    rect(cx - r, cy - inner, 2.0f * r, 2.0f * inner, colour, alpha);
    if (outer > inner)
    {
        rect(cx - wide, cy + inner, 2.0f * wide, outer - inner, colour, alpha);
        rect(cx - wide, cy - outer, 2.0f * wide, outer - inner, colour, alpha);
    }
    if (r > outer)
    {
        rect(cx - narrow, cy + outer, 2.0f * narrow, r - outer, colour, alpha);
        rect(cx - narrow, cy - r, 2.0f * narrow, r - outer, colour, alpha);
    }
}

void PixelArt::officer(float x, float y, float scale, HatArt hat, bool sweat)
{
    auto px = [&](float dx, float dy, float w, float h, std::uint32_t c) {
        rect(x + dx * scale, y + dy * scale, w * scale, h * scale, c);
    };
    px(-6, 0, 5, 3, palette::dark);
    px(1, 0, 5, 3, palette::dark);
    px(-5, 3, 10, 13, 0x476da3);
    px(-7, 7, 3, 8, 0x476da3);
    px(4, 7, 3, 8, 0x476da3);
    px(-4, 16, 8, 7, 0xf1bd92);
    px(-3, 19, 1, 1, palette::dark);
    px(2, 19, 1, 1, palette::dark);
    px(-2, 16, 4, 1, palette::dark);
    px(1, 11, 2, 3, palette::yellow);
    px(-5, 6, 10, 2, palette::dark);
    if (hat != HatArt::Cap && hat != HatArt::Bobby)
        px(-4, 22, 8, 1, 0x57433d);
    switch (hat)
    {
    case HatArt::Cap:
        px(-6, 23, 12, 3, palette::dark);
        px(-4, 26, 8, 4, 0x476da3);
        px(-1, 26, 2, 3, palette::yellow);
        break;
    case HatArt::Bobby:
        px(-6, 22, 12, 1, 0x101827);
        px(-5, 23, 10, 4, 0x1d2a44);
        px(-4, 27, 8, 3, 0x1d2a44);
        px(-3, 30, 6, 2, 0x1d2a44);
        px(-1, 32, 2, 2, 0xd8dde3);
        px(-1, 24, 2, 3, 0xd8dde3);
        px(-3, 28, 1, 2, 0x3a4c6e);
        break;
    case HatArt::ConeHat:
        px(-6, 23, 12, 2, 0xa94836);
        px(-5, 25, 10, 3, 0xf58c4c);
        px(-4, 28, 8, 2, palette::paper);
        px(-3, 30, 6, 3, 0xf58c4c);
        px(-2, 33, 4, 2, palette::paper);
        px(-1, 35, 2, 2, 0xf58c4c);
        break;
    case HatArt::PartyHat:
        px(-4, 23, 8, 3, 0xf06fa8);
        px(-3, 26, 6, 2, palette::yellow);
        px(-2, 28, 4, 3, 0xf06fa8);
        px(-1, 31, 2, 2, palette::mint);
        px(-2, 33, 4, 3, palette::yellow);
        px(-5, 18, 1, 5, 0xffffff);
        break;
    case HatArt::Crown:
        px(-3, 23, 6, 3, 0xf2c84b);
        px(-3, 26, 1, 2, 0xf2c84b);
        px(-1, 26, 2, 3, 0xf2c84b);
        px(2, 26, 1, 2, 0xf2c84b);
        px(-1, 24, 2, 1, palette::coral);
        px(-3, 23, 6, 1, 0xb8902c);
        break;
    }
    if (sweat)
    {
        px(5, 19, 2, 2, 0x9fdcff);
        px(6, 21, 1, 1, 0x9fdcff);
    }
}

void PixelArt::star(float x, float y, float unit, std::uint32_t color)
{
    // Rows from the top; each pair is the start column and length of a run, in a 9x9 grid.
    constexpr std::array<std::array<int, 4>, 9> rows{{
        {4, 1, 0, 0}, {4, 1, 0, 0}, {3, 3, 0, 0}, {0, 9, 0, 0}, {1, 7, 0, 0},
        {2, 5, 0, 0}, {2, 2, 5, 2}, {1, 2, 6, 2}, {1, 1, 7, 1},
    }};
    const float left = std::round(x - 4.5f * unit);
    const float top = std::round(y + 4.5f * unit);
    for (std::size_t row = 0; row < rows.size(); ++row)
    {
        const float rowY = top - static_cast<float>(row + 1) * unit;
        for (std::size_t run = 0; run < 4; run += 2)
            if (rows[row][run + 1] > 0)
                rect(left + static_cast<float>(rows[row][run]) * unit, rowY,
                     static_cast<float>(rows[row][run + 1]) * unit, unit, color);
    }
}

void PixelArt::padlock(float x, float y, float unit, std::uint32_t color)
{
    auto px = [&](float dx, float dy, float w, float h, std::uint32_t c) {
        rect(x + (dx - 4) * unit, y + dy * unit, w * unit, h * unit, c);
    };
    px(1, 6, 1, 3, color);
    px(6, 6, 1, 3, color);
    px(2, 9, 4, 1, color);
    px(0, 0, 8, 6, color);
    px(3, 2, 2, 2, darken(color, 0.4f));
}
}
