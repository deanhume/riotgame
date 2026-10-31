#include "GameScene.h"

#include "TargetArt.h"
#include "audio/AudioEngine.h"
#include "base/Director.h"
#include "base/EventDispatcher.h"
#include "base/EventListenerKeyboard.h"
#include "base/EventListenerMouse.h"
#include "base/EventListenerTouch.h"
#include "base/Touch.h"
#include "platform/FileUtils.h"
#include "platform/RenderViewImpl.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <span>

namespace riot
{
namespace
{
bool inside(ax::Vec2 point, float x, float y, float w, float h)
{
    return ax::Rect(x, y, w, h).containsPoint(point);
}

std::string number(int value)
{
    return std::to_string(value);
}

std::string upper(std::string_view value)
{
    std::string result(value);
    for (char& letter : result)
        letter = static_cast<char>(std::toupper(static_cast<unsigned char>(letter)));
    return result;
}

// Where the next glyph would start after `value`: textWidth() leaves out the blank column after the last glyph.
// (Not called "advance": ADL would pick std::advance for a std::string argument.)
float textAdvance(std::string_view value, int scale)
{
    return static_cast<float>(value.size() * 6 * static_cast<std::size_t>(scale));
}

ax::RenderViewImpl* renderView()
{
    return dynamic_cast<ax::RenderViewImpl*>(ax::Director::getInstance()->getRenderView());
}

constexpr std::uint32_t coralInk = 0xac554e;  // coral that reads on paper
constexpr std::uint32_t greenInk = 0x387d6d;
constexpr std::uint32_t redInk = 0xb65850;
constexpr std::uint32_t brownInk = 0x725f54;
constexpr std::uint32_t panel = 0xe6d8b8;     // a box on a paper card
constexpr std::uint32_t starOn = 0xf2b632;

constexpr float bumpSquash = 0.35f;   // seconds of wobble after walking into a wall
constexpr float throwGravity = 900.0f;
constexpr float canisterLinger = 1.5f;  // a burnt-out smoke bomb lies about this long before vanishing
constexpr std::size_t maxThrown = 60;
constexpr std::size_t maxPuffs = 320;

std::uint32_t blend(std::uint32_t from, std::uint32_t to, float amount)
{
    const auto channel = [&](int shift) {
        const float a = static_cast<float>((from >> shift) & 0xff);
        const float b = static_cast<float>((to >> shift) & 0xff);
        return static_cast<std::uint32_t>(std::lround(a + (b - a) * amount)) << shift;
    };
    return channel(16) | channel(8) | channel(0);
}
constexpr float queueSquash = 0.2f;   // ... and after bumping into the back of a queue
constexpr float moodPanic = 65.0f;    // crowd mood (percent) at which the HUD and the officer start to fret

// When each star pops onto the level report.
float starDelay(int star)
{
    return 0.45f + 0.35f * static_cast<float>(star);
}

// The short form of a bonus goal for the in-game tracker.
std::string trackerText(const GoalDef& goal)
{
    switch (goal.type)
    {
    case GoalType::NoBarriersLost: return "NO BARRIERS LOST";
    case GoalType::BudgetLeft: return "END WITH $" + number(goal.amount);
    case GoalType::MaxSpend: return "SPEND $" + number(goal.amount) + " MAX";
    case GoalType::MoodBelow: return "MOOD UNDER " + number(goal.amount) + "%";
    case GoalType::NoVans: return "NO VANS";
    }
    return {};
}

// A paper card with a hard shadow and an ink border, as used by every card screen.
void drawCard(PixelArt& art, float x, float y, float w, float h)
{
    art.rect(x + 10, y - 8, w, h, palette::dark);
    art.rect(x, y, w, h, palette::paper);
    art.frame(x + 8, y + 8, w - 16, h - 16, palette::ink, 3);
}

void drawStar(PixelArt& art, float x, float y, float unit, bool filled, bool onDark)
{
    art.star(x + unit, y - unit, unit, onDark ? 0x0b141e : 0xb9a888);
    art.star(x, y, unit, filled ? starOn : onDark ? 0x3c5661 : 0xd5c7a6);
}

// A 16x16 tick or cross box.
void drawTick(PixelArt& art, float x, float y, bool ok)
{
    art.rect(x, y, 16, 16, ok ? greenInk : redInk);
    if (ok)
    {
        for (int i = 0; i < 4; ++i)
            art.rect(x + 3 + static_cast<float>(i), y + 7 - static_cast<float>(i), 2, 2, palette::paper);
        for (int i = 0; i < 6; ++i)
            art.rect(x + 7 + static_cast<float>(i), y + 5 + static_cast<float>(i), 2, 2, palette::paper);
    }
    else
        for (int i = 0; i < 9; ++i)
        {
            art.rect(x + 3 + static_cast<float>(i), y + 3 + static_cast<float>(i), 2, 2, palette::paper);
            art.rect(x + 3 + static_cast<float>(i), y + 11 - static_cast<float>(i), 2, 2, palette::paper);
        }
}

void drawTitleText(PixelArt& art, std::string_view text, float y, int scale, std::uint32_t color)
{
    art.centered(text, 643, y - 3, scale, 0xd9c9a3);
    art.centered(text, 640, y, scale, color);
}
}

GameScene* GameScene::create(const Options& options)
{
    auto* scene = new GameScene;
    scene->options_ = options;
    if (scene->init())
    {
        scene->autorelease();
        return scene;
    }
    delete scene;
    return nullptr;
}

bool GameScene::init()
{
    if (!Scene::init())
        return false;
    world_ = ax::DrawNode::create();
    ui_ = ax::DrawNode::create();
    addChild(world_);
    addChild(ui_, 10);

    auto* mouse = ax::EventListenerMouse::create();
    mouse->onMouseMove = [this](ax::EventMouse* event) {
        pointer_ = event->getLocation();
        return false;
    };
    mouse->onMouseDown = [this](ax::EventMouse* event) {
        if (event->getMouseButton() == ax::EventMouse::MouseButton::BUTTON_RIGHT)
            click(event->getLocation(), true);
        return false;
    };
    _eventDispatcher->addEventListenerWithSceneGraphPriority(mouse, this);

    // Axmol dispatches left mouse clicks as touches too; handle them only once.
    auto* touch = ax::EventListenerTouchOneByOne::create();
    touch->setSwallowTouches(true);
    touch->onTouchBegan = [this](ax::Touch* event, ax::Event*) {
        pointer_ = event->getLocation();
        click(pointer_, false);
        return true;
    };
    _eventDispatcher->addEventListenerWithSceneGraphPriority(touch, this);

    auto* keyboard = ax::EventListenerKeyboard::create();
    keyboard->onKeyPressed = [this](ax::EventKeyboard::KeyCode code, ax::Event*) { key(code); };
    keyboard->onKeyReleased = [this](ax::EventKeyboard::KeyCode code, ax::Event*) {
        using Key = ax::EventKeyboard::KeyCode;
        if (code == Key::KEY_SPACE)
        {
            model_.raiseShield(false);
            skipSpaceHeld_ = false;
        }
        else if (code == Key::KEY_LEFT_ARROW)
            leftHeld_ = false;
        else if (code == Key::KEY_RIGHT_ARROW)
            rightHeld_ = false;
    };
    _eventDispatcher->addEventListenerWithSceneGraphPriority(keyboard, this);
    ax::AudioEngine::setMaxAudioInstance(12);

    if (!options_.savePath.empty())
    {
        Progress saved;
        if (loadProgress(options_.savePath, saved))
            model_.setProgress(saved);
    }
    if (options_.startLevel > 0)
    {
        cinematic_.skip();
        model_.newGame((options_.startLevel - 1) / 2, options_.startLevel % 2 == 0);
    }
    scheduleUpdate();
    return true;
}

void GameScene::resetScene()
{
    accumulator_ = 0.0f;
    selected_ = 1;
    paused_ = false;
    shake_ = 0.0f;
    banner_ = 0.0f;
    bubbleGap_ = 0.0f;
    chatterTimer_ = flavour::chatterEvery;
    particles_.clear();
    popups_.clear();
    bubbles_.clear();
    thrown_.clear();
    smoke_.clear();
    fx_.clear();
    defendFx_.clear();
    model_.raiseShield(false);
    leftHeld_ = rightHeld_ = false;
    hotspots_.clear();
}

void GameScene::changeScreen()
{
    resetScene();
    sound("place");
    saveIfChanged();
}

void GameScene::saveIfChanged()
{
    if (!model_.takeProgressChanged() || options_.savePath.empty())
        return;
    const bool saved = saveProgress(options_.savePath, model_.progress());
    if (!saved && !saveFailed_)
        AXLOGE("Riot Control: could not save progress to {}", options_.savePath.string());
    saveFailed_ = !saved;
}

void GameScene::writeSmokeState()
{
    if (options_.smokeStatePath.empty())
        return;
    const char* phase = "";
    switch (model_.phase())
    {
    case Phase::Title: phase = "Title"; break;
    case Phase::Map: phase = "Map"; break;
    case Phase::Shop: phase = "Shop"; break;
    case Phase::Wardrobe: phase = "Wardrobe"; break;
    case Phase::Intro: phase = "Intro"; break;
    case Phase::Prep: phase = "Prep"; break;
    case Phase::Wave: phase = "Wave"; break;
    case Phase::Intermission: phase = "Intermission"; break;
    case Phase::Won: phase = "Won"; break;
    case Phase::Lost: phase = "Lost"; break;
    case Phase::DefendIntro: phase = "DefendIntro"; break;
    case Phase::Defend: phase = "Defend"; break;
    case Phase::DefendWon: phase = "DefendWon"; break;
    case Phase::DefendLost: phase = "DefendLost"; break;
    case Phase::Finished: phase = "Finished"; break;
    }
    const std::string state = std::string("phase=") + (cinematic_.active() ? "Cinematic" : phase) +
                              "\nintroScene=" + std::to_string(cinematic_.sceneIndex()) +
                              "\npaused=" + (paused_ ? "1" : "0") +
                              "\nshield=" + std::to_string(model_.defence().shieldHealth()) +
                              "\nsliced=" + std::to_string(model_.defence().sliced()) + "\n";
    if (state == smokeState_)
        return;
    std::ofstream output(options_.smokeStatePath, std::ios::trunc);
    output << state;
    output.flush();
    if (!output)
    {
        AXLOGE("Riot Control: could not write smoke state to {}", options_.smokeStatePath.string());
        options_.smokeStatePath.clear();
        return;
    }
    smokeState_ = state;
}

bool GameScene::shortStart() const
{
    const auto* view = ax::Director::getInstance()->getRenderView();
    return view && std::min(view->getScaleX(), view->getScaleY()) < config::shortStartBelowScale;
}

// Every screen except the street while a level is being played.
bool GameScene::onCard() const
{
    return !model_.playing();
}

float GameScene::roll()
{
    random_ = random_ * 1664525u + 1013904223u;
    return static_cast<float>((random_ >> 8) & 65535) / 65536.0f;
}

void GameScene::perform(Command command, int arg)
{
    hotspots_.clear();
    switch (command)
    {
    case Command::Primary:
        primaryAction();
        return;
    case Command::Back:
        goBack();
        return;
    case Command::Resume:
        paused_ = false;
        model_.raiseShield(false);
        return;
    case Command::Buy:
        sound(model_.buyUpgrade(arg) ? "repair" : "bonk");
        saveIfChanged();
        return;
    case Command::Refund:
        if (model_.progress().pointsSpent() > 0)
            sound("break");
        model_.refundUpgrades();
        saveIfChanged();
        return;
    case Command::Hat:
        if (model_.equipHat(arg))
            sound("place");
        saveIfChanged();
        return;
    case Command::Cone:
        if (model_.equipCone(arg))
            sound("place");
        saveIfChanged();
        return;
    case Command::Level:
        if (!model_.selectLevel(arg))
        {
            sound("bonk");
            return;
        }
        break;
    case Command::Map:
    case Command::Quit:
        model_.openMap();
        break;
    case Command::Title:
        model_.backToTitle();
        break;
    case Command::Shop:
        model_.openShop();
        break;
    case Command::Wardrobe:
        model_.openWardrobe();
        break;
    }
    changeScreen();
}

void GameScene::primaryAction()
{
    hotspots_.clear();
    if (paused_ && model_.playing())
    {
        paused_ = false;
        return;
    }
    switch (model_.phase())
    {
    case Phase::Title:
    case Phase::Finished:
        model_.openMap();
        break;
    case Phase::Map:
        model_.selectLevel(model_.frontierLevel());
        break;
    case Phase::Shop:
        model_.closeShop();
        break;
    case Phase::Wardrobe:
        model_.closeWardrobe();
        break;
    case Phase::Intro:
    case Phase::DefendIntro:
        model_.beginLevel();
        break;
    case Phase::Won:
    case Phase::DefendWon:
        model_.nextLevel();
        break;
    case Phase::Lost:
    case Phase::DefendLost:
        model_.retryLevel();
        break;
    case Phase::Wave:
    case Phase::Defend:
        paused_ = true;
        model_.raiseShield(false);
        return;
    case Phase::Prep:
    case Phase::Intermission:
        model_.ready();
        return;
    }
    changeScreen();
}

// Esc: one step back towards the map (or the title), pausing instead while a level is being played.
void GameScene::goBack()
{
    hotspots_.clear();
    switch (model_.phase())
    {
    case Phase::Title:
        ax::Director::getInstance()->end();
        return;
    case Phase::Map:
        model_.backToTitle();
        break;
    case Phase::Shop:
        model_.closeShop();
        break;
    case Phase::Wardrobe:
        model_.closeWardrobe();
        break;
    case Phase::Prep:
    case Phase::Wave:
    case Phase::Intermission:
    case Phase::Defend:
        paused_ = !paused_;
        model_.raiseShield(false);
        return;
    case Phase::Intro:
    case Phase::Won:
    case Phase::Lost:
    case Phase::Finished:
    case Phase::DefendIntro:
    case Phase::DefendWon:
    case Phase::DefendLost:
        model_.openMap();
        break;
    }
    changeScreen();
}

void GameScene::toggleFullscreen()
{
    auto* view = renderView();
    if (!view)
        return;
    if (view->isFullscreen())
    {
        view->setWindowed(config::width, config::height);
        glfwSetWindowAttrib(view->getWindow(), GLFW_RESIZABLE, GLFW_TRUE);
    }
    else
        view->setFullscreen();
}

void GameScene::key(ax::EventKeyboard::KeyCode code)
{
    using Key = ax::EventKeyboard::KeyCode;
    if (code == Key::KEY_F11)
        toggleFullscreen();
    else if (code == Key::KEY_M)
    {
        muted_ = !muted_;
        if (muted_)
            ax::AudioEngine::stopAll();
    }
    else if (cinematic_.active())
    {
        if (code == Key::KEY_SPACE)
        {
            skipSpaceHeld_ = true;
            finishCinematic();
        }
    }
    else if (code == Key::KEY_SPACE && skipSpaceHeld_)
        return;
    else if (code == Key::KEY_ESCAPE)
        goBack();
    else if (model_.phase() == Phase::Defend &&
             (code == Key::KEY_SPACE || code == Key::KEY_LEFT_ARROW || code == Key::KEY_RIGHT_ARROW))
    {
        if (code == Key::KEY_SPACE)
            model_.raiseShield(!paused_);
        else
        {
            bool& held = code == Key::KEY_LEFT_ARROW ? leftHeld_ : rightHeld_;
            if (!paused_ && !held && model_.swipe(code == Key::KEY_LEFT_ARROW ? -1 : 1))
                sound("thud", 0.15f);
            held = true;
        }
    }
    else if (code == Key::KEY_SPACE || code == Key::KEY_ENTER || code == Key::KEY_KP_ENTER)
        primaryAction();
    else if (options_.debug && (code == Key::KEY_F8 || code == Key::KEY_F9))
    {
        if (code == Key::KEY_F8)
            model_.debugWin();
        else
            model_.debugLose();
        paused_ = false;
        processEvents();
        processDefendEvents();
        saveIfChanged();
    }
    else
    {
        const int digit = code >= Key::KEY_1 && code <= Key::KEY_9 ? static_cast<int>(code) - static_cast<int>(Key::KEY_1)
                                                                   : -1;
        switch (model_.phase())
        {
        case Phase::Map:
            if (digit >= 0 && digit < levelCount)
                perform(Command::Level, digit);
            else if (code == Key::KEY_S)
                perform(Command::Shop);
            else if (code == Key::KEY_W)
                perform(Command::Wardrobe);
            break;
        case Phase::Shop:
            if (digit >= 0 && digit < upgradeCount)
                perform(Command::Buy, digit);
            else if (code == Key::KEY_R)
                perform(Command::Refund);
            break;
        case Phase::Prep:
        case Phase::Wave:
        case Phase::Intermission:
        case Phase::Defend:
            if (digit >= 0 && digit < 4)
                selected_ = digit;
            else if (code == Key::KEY_P)
            {
                paused_ = !paused_;
                model_.raiseShield(false);
            }
            else if (code == Key::KEY_Q && paused_)
                perform(Command::Quit);
            break;
        default:
            break;
        }
    }
}

std::pair<int, int> GameScene::cellAt(ax::Vec2 point) const
{
    const int column = static_cast<int>(std::floor((point.x - config::firstColumnX + config::columnSpacing / 2)
                                                 / config::columnSpacing));
    if (column < 0 || column >= config::columns)
        return {-1, -1};
    for (int lane = 0; lane < config::lanes; ++lane)
        if (std::abs(point.y - config::laneY[lane]) <= 42.0f)
            return {lane, column};
    return {-1, -1};
}

void GameScene::click(ax::Vec2 point, bool repair)
{
    if (cinematic_.active())
        return;
    if (inside(point, 1010, 17, 250, 30))
    {
        key(ax::EventKeyboard::KeyCode::KEY_M);
        return;
    }
    if (inside(point, 890, 17, 115, 30))
    {
        toggleFullscreen();
        return;
    }
    if (!repair)
    {
        const auto hit = std::find_if(hotspots_.begin(), hotspots_.end(), [&](const Hotspot& spot) {
            return inside(point, spot.box.x, spot.box.y - 6, spot.box.w, spot.box.h + 6);
        });
        if (hit != hotspots_.end())
        {
            const Hotspot spot = *hit;
            perform(spot.command, spot.arg);
            return;
        }
    }
    if (!model_.canBuild())
        return;
    if (inside(point, 986, 82, 270, 67))
    {
        primaryAction();
        return;
    }
    for (int i = 0; i < 4; ++i)
        if (inside(point, 24 + i * 238.0f, 70, 226, 84))
        {
            selected_ = i;
            sound("place");
            return;
        }
    if (paused_)
        return;
    auto [lane, column] = cellAt(point);
    if (lane < 0)
        return;
    if (repair || selected_ == 3)
        model_.repair(lane, column);
    else
        model_.place(static_cast<GadgetType>(selected_), lane, column);
    processEvents();
}

void GameScene::sound(const char* name, float volume)
{
    if (muted_ || audioFailed_)
        return;
    const std::string file = std::string("audio/") + name + ".wav";
    if (!ax::FileUtils::getInstance()->isFileExist(file) ||
        ax::AudioEngine::play2d(file, false, volume) == ax::AudioEngine::INVALID_AUDIO_ID)
    {
        AXLOGE("Riot Control: could not play {}", file);
        audioFailed_ = true;
    }
}

void GameScene::burst(float x, float y, std::uint32_t color, int count)
{
    for (int i = 0; i < count && particles_.size() < 400; ++i)
    {
        const float spread = roll();
        const float lift = roll();
        const float life = roll();
        const float size = roll();
        particles_.push_back({{x, y}, {(spread - 0.5f) * 170.0f, 30.0f + lift * 120.0f}, 0.35f + life * 0.5f,
                              3.0f + size * 5.0f, color});
    }
}

// Lobs a brick or smoke bomb from the thrower's raised hand on an arc that comes down where the model said:
// on the front of a wall (aimed bricks) or on the road.
void GameScene::launch(const Event& event)
{
    if (thrown_.size() >= maxThrown)
        return;
    const float laneY = config::laneY[static_cast<std::size_t>(event.lane)];
    Thrown item{};
    item.kind = event.thrown;
    item.lane = event.lane;
    item.column = event.column;
    item.ground = laneY + 3.0f;
    item.position = {event.x + 16.0f, laneY + 38.0f};
    if (event.column >= 0)
        item.wallX = event.landX;
    const ax::Vec2 target{event.landX, event.column >= 0 ? laneY + 24.0f : item.ground};
    const float flight = 0.55f + std::max(20.0f, target.x - item.position.x) / 600.0f;
    item.velocity = {(target.x - item.position.x) / flight,
                     (target.y - item.position.y + 0.5f * throwGravity * flight * flight) / flight};
    item.spin = roll();
    item.spinRate = 1.6f + roll() * 1.4f;
    const auto& colours = flavour::smokeColours;
    item.colour = colours[std::min(colours.size() - 1,
                                   static_cast<std::size_t>(roll() * static_cast<float>(colours.size())))];
    thrown_.push_back(item);
}

float GameScene::flareStrength(const Thrown& item)
{
    if (item.kind != Throwable::SmokeBomb)
        return 0.0f;
    if (!item.landed)
        return 0.6f;
    return std::clamp(flavour::smokeBurnSeconds - item.life, 0.0f, 1.0f);
}

void GameScene::updateThrown(float dt)
{
    using namespace flavour;
    for (Thrown& item : thrown_)
    {
        const bool smoky = item.kind == Throwable::SmokeBomb;
        if (item.landed)
            item.life += dt;
        else
        {
            item.velocity.y -= throwGravity * dt;
            item.position += item.velocity * dt;
            item.spin += item.spinRate * dt;
            if (!item.hitWall && item.wallX >= 0.0f && item.velocity.x > 0.0f && item.position.x >= item.wallX)
            {
                if (model_.gadgetAt(item.lane, item.column))
                {
                    // Bonk! Straight back off the wall and down into the crowd's feet.
                    item.hitWall = true;
                    item.position.x = item.wallX;
                    item.velocity = {-50.0f - roll() * 60.0f, 110.0f + roll() * 90.0f};
                    item.spinRate = -item.spinRate * 1.5f;
                    burst(item.wallX, item.position.y, palette::paper, 5);
                    if (popups_.size() < 14)
                        popups_.push_back({"BONK!", {item.wallX, item.position.y + 34.0f}, 0.8f, palette::paper});
                    if (clonkCooldown_ <= 0.0f)
                    {
                        sound("clonk", 0.3f);
                        clonkCooldown_ = 0.1f;
                    }
                }
                else
                    item.wallX = -1.0f;  // the wall has gone, so it sails on over
            }
            if (item.position.y <= item.ground && item.velocity.y < 0.0f)
            {
                item.position.y = item.ground;
                if (item.bounces++ == 0)
                {
                    burst(item.position.x, item.ground, palette::paper, 3);
                    if (smoky)
                    {
                        if (popups_.size() < 14)
                            popups_.push_back({"FIZZ!", {item.position.x, item.ground + 40.0f}, 1.0f, item.colour});
                        if (fizzCooldown_ <= 0.0f)
                        {
                            sound("fizz", 0.25f);
                            fizzCooldown_ = 0.8f;
                        }
                    }
                    else if (clonkCooldown_ <= 0.0f)
                    {
                        sound("thud", 0.28f);
                        clonkCooldown_ = 0.1f;
                    }
                }
                if (item.velocity.y < -90.0f)
                {
                    item.velocity = {item.velocity.x * 0.45f, -item.velocity.y * 0.32f};
                    item.spinRate *= 0.5f;
                }
                else
                {
                    item.landed = true;
                    item.velocity = {};
                    item.spin = 0.0f;
                }
            }
        }

        const float strength = flareStrength(item);
        if (strength <= 0.0f)
            continue;
        const ax::Vec2 end = item.landed ? ax::Vec2{item.position.x + 8.0f, item.position.y} : item.position;
        item.puffDue += smokePuffsPerSecond * (item.landed ? strength : 0.35f) * dt;
        for (; item.puffDue >= 1.0f; item.puffDue -= 1.0f)
        {
            if (smoke_.size() >= maxPuffs)
                continue;
            const float size = roll();
            smoke_.push_back({end + ax::Vec2{(roll() - 0.5f) * 6.0f, roll() * 4.0f},
                              {-14.0f + (roll() - 0.5f) * 20.0f, 20.0f + roll() * 22.0f}, 0.0f,
                              smokePuffSeconds * (0.7f + roll() * 0.6f), 5.0f + size * 4.0f, 8.0f + roll() * 6.0f,
                              item.colour});
        }
        item.sparkDue += smokeSparksPerSecond * strength * dt;
        for (; item.sparkDue >= 1.0f; item.sparkDue -= 1.0f)
        {
            if (particles_.size() >= 400)
                continue;
            constexpr std::array<std::uint32_t, 3> sparks{0xffffff, 0xfff1a0, 0xffb347};
            const float pick = roll();
            particles_.push_back({end, {(roll() - 0.5f) * 140.0f, 50.0f + roll() * 120.0f}, 0.2f + roll() * 0.3f,
                                  2.0f + std::floor(roll() * 2.0f),
                                  sparks[std::min<std::size_t>(2, static_cast<std::size_t>(pick * 3.0f))]});
        }
    }
    std::erase_if(thrown_, [](const Thrown& item) {
        if (item.position.x < -40.0f || item.position.x > 1320.0f || item.position.y < -40.0f)
            return true;
        const float lasts = item.kind == Throwable::Brick ? flavour::brickRestSeconds
                                                          : flavour::smokeBurnSeconds + canisterLinger;
        return item.landed && item.life >= lasts;
    });

    for (Puff& puff : smoke_)
    {
        puff.age += dt;
        puff.position += puff.velocity * dt;
        puff.velocity.y *= 1.0f - 0.3f * dt;
    }
    std::erase_if(smoke_, [](const Puff& puff) { return puff.age >= puff.lifetime; });
}

// Starts a speech bubble if there is room for one, the rioter hasn't just spoken, and the moment's dice say so.
void GameScene::say(int walkerId, flavour::Moment moment)
{
    using namespace flavour;
    if (static_cast<int>(bubbles_.size()) >= maxBubbles || bubbleGap_ > 0.0f)
        return;
    const Walker* walker = model_.findWalker(walkerId);
    if (!walker || walker->x < 30.0f || walker->x > config::goalX)
        return;
    WalkerFx& fx = fx_[walkerId];
    if (fx.quiet > 0.0f || roll() >= chance[static_cast<std::size_t>(moment)])
        return;
    std::vector<const char*> lines;
    for (const Phrase& phrase : phrases)
        if (phrase.moment == moment)
            lines.push_back(phrase.text);
    // The level's own lines ("My curry sauce!") come up twice as often as the generic ones.
    if (moment == Moment::NearTarget)
        for (const char* line : model_.level().nearTarget)
            if (line)
                lines.insert(lines.end(), 2, line);
    if (lines.empty())
        return;
    const auto pick = std::min(lines.size() - 1, static_cast<std::size_t>(roll() * static_cast<float>(lines.size())));
    bubbles_.push_back({walkerId, lines[pick], bubbleSeconds, {walker->x, config::laneY[walker->lane]}});
    bubbleGap_ = bubbleGap;
    fx.quiet = quietSeconds;
}

void GameScene::processEvents()
{
    using flavour::Moment;
    for (const auto& event : model_.takeEvents())
    {
        const float y = config::laneY[static_cast<std::size_t>(event.lane)];
        switch (event.type)
        {
        case EventType::Placed:
            burst(event.x, y, palette::paper, 8);
            sound("place");
            break;
        case EventType::Repaired:
            burst(event.x, y + 24, palette::mint, 12);
            popups_.push_back({"+ REPAIR", {event.x, y + 70}, 1.0f, palette::mint});
            sound("repair");
            break;
        case EventType::Trip:
            burst(event.x, y + 7, palette::yellow, 10);
            popups_.push_back({"OOPS!", {event.x, y + 63}, 1.0f, palette::yellow});
            if (soundCooldown_ <= 0)
            {
                sound("trip");
                soundCooldown_ = 0.12f;
            }
            fx_[event.walker].surprise = 0.8f;
            say(event.walker, Moment::Tripped);
            break;
        case EventType::Bump:
        {
            WalkerFx& fx = fx_[event.walker];
            fx.squash = fx.squashLength = bumpSquash;
            fx.squashPower = 1.0f;
            fx.surprise = 0.5f;
            const Walker* walker = model_.findWalker(event.walker);
            if (walker && walker->look % 3 == 0)
                fx.sweat = 1.0f;
            else
                fx.exclaim = 0.8f;
            burst(event.x + 8, y + 30, palette::paper, 3);
            if (bumpCooldown_ <= 0)
            {
                sound("bonk");
                bumpCooldown_ = 0.3f;
            }
            say(event.walker, Moment::Blocked);
            break;
        }
        case EventType::NearTarget:
            say(event.walker, Moment::NearTarget);
            break;
        case EventType::Bored:
            fx_[event.walker].sweat = 1.4f;
            say(event.walker, Moment::Bored);
            break;
        case EventType::GiveUp:
            if (popups_.size() < 12)
                popups_.push_back({"WENT HOME!", {event.x, y + 66}, 1.0f, palette::mint});
            say(event.walker, Moment::GivingUp);
            break;
        case EventType::Throw:
            launch(event);
            fx_[event.walker].throwing = flavour::throwArmSeconds;
            say(event.walker, Moment::Throwing);
            break;
        case EventType::Buckle:
            burst(event.x, y + 25, palette::yellow, 4);
            if (soundCooldown_ <= 0)
            {
                sound("bonk");
                soundCooldown_ = 0.16f;
            }
            break;
        case EventType::Break:
            burst(event.x, y + 16, palette::paper, 24);
            burst(event.x, y + 25, palette::blue, 12);
            popups_.push_back({"KRRUNCH!", {event.x, y + 65}, 1.2f, palette::coral});
            shake_ = event.gadget == GadgetType::Van ? 0.38f : 0.2f;
            if (soundCooldown_ <= 0)
            {
                sound("break");
                soundCooldown_ = 0.12f;
            }
            break;
        case EventType::Breach:
            burst(event.x, y + 24, palette::coral, 12);
            popups_.push_back({"+" + number(static_cast<int>(config::arrivalMood)) + " MOOD",
                               {event.x - 30, y + 62}, 1.0f, palette::coral});
            break;
        case EventType::Disperse:
            burst(event.x, y + 18, palette::paper, 7);
            break;
        case EventType::WaveStart:
            banner_ = 2.0f;
            sound("whistle");
            break;
        case EventType::WaveEnd:
            if (model_.phase() == Phase::Won)
            {
                sound("win");
                for (int i = 0; i < 12; ++i)
                    burst(150 + i * 88.0f, 610, i % 2 ? palette::mint : palette::yellow, 16);
            }
            else
                sound("whistle");
            break;
        }
    }
}

// Reactions, speech bubbles and idle chatter.
void GameScene::updateCrowd(float dt)
{
    bubbleGap_ = std::max(0.0f, bubbleGap_ - dt);
    std::erase_if(fx_, [&](const auto& entry) { return !model_.findWalker(entry.first); });
    for (auto& [id, fx] : fx_)
    {
        fx.squash = std::max(0.0f, fx.squash - dt);
        fx.surprise = std::max(0.0f, fx.surprise - dt);
        fx.exclaim = std::max(0.0f, fx.exclaim - dt);
        fx.sweat = std::max(0.0f, fx.sweat - dt);
        fx.throwing = std::max(0.0f, fx.throwing - dt);
        fx.quiet = std::max(0.0f, fx.quiet - dt);
    }
    for (const Walker& walker : model_.walkers())
    {
        WalkerFx& fx = fx_[walker.id];
        const bool queued = walker.blockingGadget >= 0 && !walker.pressing;
        if (queued && !fx.queued && fx.squash <= 0.0f)
        {
            fx.squash = fx.squashLength = queueSquash;
            fx.squashPower = 0.6f;
        }
        fx.queued = queued;
    }

    for (Bubble& bubble : bubbles_)
    {
        bubble.life -= dt;
        if (const Walker* walker = model_.findWalker(bubble.walker))
            bubble.anchor = {walker->x, config::laneY[walker->lane] - (walker->leaving ? 8.0f : 0.0f)};
        else
            bubble.life = 0.0f;
    }
    std::erase_if(bubbles_, [](const Bubble& bubble) { return bubble.life <= 0.0f; });

    if (model_.phase() != Phase::Wave)
        return;
    chatterTimer_ -= dt;
    if (chatterTimer_ > 0.0f)
        return;
    chatterTimer_ = flavour::chatterEvery * (0.5f + roll());
    std::vector<const Walker*> candidates;
    for (const Walker& walker : model_.walkers())
        if (!walker.leaving && walker.stumble <= 0.0f && walker.x > 40.0f && walker.x < config::dangerX)
            candidates.push_back(&walker);
    if (candidates.empty())
        return;
    const auto pick = std::min(candidates.size() - 1,
                               static_cast<std::size_t>(roll() * static_cast<float>(candidates.size())));
    const Walker* chosen = candidates[pick];
    say(chosen->id, chosen->bored ? flavour::Moment::Bored : flavour::Moment::Walking);
}

void GameScene::update(float dt)
{
    dt = std::clamp(dt, 0.0f, 0.1f);
    if (cinematic_.active())
    {
        cinematic_.tick(dt);
        if (!cinematic_.active())
            finishCinematic();
        else
            updateCinematicAudio();
        drawFrame();
        writeSmokeState();
        return;
    }
    if (model_.phase() == Phase::Defend)
    {
        const auto* view = renderView();
        auto* window = view ? view->getWindow() : nullptr;
        const bool focused = window && glfwGetWindowAttrib(window, GLFW_FOCUSED);
        model_.raiseShield(focused && !paused_ && glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS);
        if (!focused)
        {
            paused_ = true;
            leftHeld_ = rightHeld_ = false;
        }
    }
    if (!paused_)
    {
        time_ += dt;
        screenTime_ += dt;
        soundCooldown_ -= dt;
        bumpCooldown_ -= dt;
        clonkCooldown_ -= dt;
        fizzCooldown_ -= dt;
        banner_ = std::max(0.0f, banner_ - dt);
        shake_ = std::max(0.0f, shake_ - dt);
        accumulator_ += dt;
        while (accumulator_ >= config::fixedStep)
        {
            model_.tick(config::fixedStep);
            accumulator_ -= config::fixedStep;
        }
        processEvents();
        processDefendEvents();
        for (auto& effect : defendFx_)
            effect.life -= dt;
        std::erase_if(defendFx_, [](const DefendFx& effect) { return effect.life <= 0.0f; });
        updateCrowd(dt);
        updateThrown(dt);
        if (model_.phase() != shownPhase_)
        {
            if (model_.phase() == Phase::Lost || model_.phase() == Phase::DefendLost)
                sound("lose");
            else if (model_.phase() == Phase::Finished || model_.phase() == Phase::DefendWon)
                sound("win");
            shownPhase_ = model_.phase();
            screenTime_ = 0.0f;
            starsShown_ = 0;
        }
        if (model_.phase() == Phase::Won)
        {
            int due = 0;
            for (int star = 0; star < model_.result().stars; ++star)
                if (screenTime_ >= starDelay(star))
                    due = star + 1;
            if (due > starsShown_)
            {
                starsShown_ = due;
                sound("place");
            }
        }
        if (model_.phase() == Phase::Finished)
        {
            confettiTimer_ -= dt;
            if (confettiTimer_ <= 0.0f)
            {
                confettiTimer_ = 0.3f;
                constexpr std::array<std::uint32_t, 4> colors{palette::yellow, palette::mint, palette::coral,
                                                              palette::blue};
                const float x = 140.0f + roll() * 1000.0f;
                burst(x, 735, colors[static_cast<std::size_t>(roll() * 4.0f) % colors.size()], 14);
            }
        }
        for (auto& particle : particles_)
        {
            particle.life -= dt;
            particle.velocity.y -= 260.0f * dt;
            particle.position += particle.velocity * dt;
        }
        std::erase_if(particles_, [](const Particle& p) { return p.life <= 0; });
        for (auto& popup : popups_)
        {
            popup.life -= dt;
            popup.position.y += 16.0f * dt;
        }
        std::erase_if(popups_, [](const Popup& p) { return p.life <= 0; });
    }
    saveIfChanged();
    world_->setPosition(shake_ > 0 ? std::sin(time_ * 80) * shake_ * 13 : 0,
                        shake_ > 0 ? std::cos(time_ * 94) * shake_ * 8 : 0);
    drawFrame();
    writeSmokeState();
}

void GameScene::drawStreet(PixelArt& art)
{
    using namespace palette;
    art.rect(0, 168, 1280, 520, 0xeccdb0);
    art.rect(0, 600, 1280, 88, 0xe6bcb0);
    art.rect(0, 657, 1280, 31, 0xd9aba7);
    for (int i = 0; i < 4; ++i)
    {
        const float x = 100.0f + i * 261.0f + std::sin(time_ * 0.03f + i) * 20;
        art.rect(x, 641, 64, 10, paper);
        art.rect(x + 13, 651, 36, 10, paper);
    }
    constexpr std::array<std::uint32_t, 5> walls{0xb99292, 0x92a6a0, 0xcbab87, 0x9f9aad, 0xb99d8e};
    for (int i = 0; i < 9; ++i)
    {
        const float x = i * 128.0f - 30;
        const float roof = 581.0f + (i % 3) * 17.0f;
        art.rect(x, 532, 119, roof - 532, walls[i % walls.size()]);
        art.rect(x - 3, roof, 125, 8, 0x746e7c);
        art.rect(x + 85, roof + 8, 14, 20, 0x9e8683);
        for (int j = 0; j < 3; ++j)
        {
            art.rect(x + 13 + j * 31.0f, 551, 19, 28, 0x677680);
            art.rect(x + 16 + j * 31.0f, 566, 13, 10, 0xd8b98f);
        }
    }
    art.rect(0, 510, 1280, 26, 0xd3c9ac);
    art.rect(0, 504, 1280, 6, 0xf9e7c0);
    for (int i = 0; i < 22; ++i)
        art.rect(i * 64.0f, 511, 2, 25, 0xb0b19d);
    art.rect(0, 179, 1082, 325, 0x495d68);
    for (int lane = 0; lane < config::lanes; ++lane)
    {
        const float y = config::laneY[lane];
        art.rect(0, y - 44, 1082, 88, lane % 2 ? 0x4a606a : 0x4e656e);
        for (int i = 0; i < 22; ++i)
            art.rect(i * 51.0f + 12, y - 47, 26, 3, 0xa3b0a5);
        for (int column = 0; column < config::columns; ++column)
        {
            const float x = GameModel::columnX(column);
            const auto color = 0x66808a;
            for (float side : {-1.0f, 1.0f})
            {
                art.rect(x + side * 34 - (side > 0 ? 6.0f : 0.0f), y - 29, 8, 2, color);
                art.rect(x + side * 34 - (side > 0 ? 6.0f : 0.0f), y + 29, 8, 2, color);
            }
        }
        art.text(">", 127, y + 2, 3, 0x8da59f);
        art.text("0" + number(lane + 1), 28, y + 4, 2, 0x8da59f);
        for (int mark = 0; mark < 4; ++mark)
            art.rect(984 + mark * 22.0f, y - 34, 10, 68, 0xc0b480, 0.3f);
    }
    art.rect(0, 168, 1280, 12, 0xc7bfa5);
    art.rect(0, 177, 1280, 3, paper);
    art.rect(956, 182, 3, 318, yellow, 0.5f);
    art.text("DANGER", 971, 488, 1, yellow);
    art.rect(22, 517, 115, 20, ink);
    art.text("CROWD >", 35, 523, 1, paper);

    drawTarget(art, model_.level().look, time_);
    // The officer in charge, in the hat from the wardrobe, getting nervous when the crowd does.
    art.rect(1029, 506, 24, 4, ink, 0.25f);
    art.officer(1040, 508, 2, hats[static_cast<std::size_t>(model_.progress().hat)].art, model_.mood() > moodPanic);

    const ConeArt coneStyle = cones[static_cast<std::size_t>(model_.progress().cone)].art;
    auto [hoverLane, hoverColumn] = cellAt(pointer_);
    if (model_.canBuild() && !paused_ && hoverLane >= 0)
    {
        const float x = GameModel::columnX(hoverColumn);
        const float y = config::laneY[hoverLane];
        bool valid;
        if (selected_ == 3)
        {
            const auto* gadget = model_.gadgetAt(hoverLane, hoverColumn);
            valid = gadget && gadget->type != GadgetType::Cone && gadget->health < model_.maxHealth(gadget->type) &&
                    model_.budget() >= static_cast<float>(model_.repairCost());
        }
        else
            valid = model_.placementError(static_cast<GadgetType>(selected_), hoverLane, hoverColumn).empty();
        art.rect(x - 36, y - 34, 72, 77, valid ? mint : coral, 0.13f);
        art.frame(x - 36, y - 34, 72, 77, valid ? mint : coral);
        if (!model_.gadgetAt(hoverLane, hoverColumn) && selected_ < 3 && valid)
        {
            PixelArt ghost(world_, 0.4f);
            ghost.gadget(static_cast<GadgetType>(selected_), x, y, 1, time_, 1.0f, coneStyle);
        }
    }

    for (int lane = 0; lane < config::lanes; ++lane)
    {
        for (const auto& gadget : model_.gadgets())
        {
            if (gadget.lane != lane)
                continue;
            const float health = gadget.health / model_.maxHealth(gadget.type);
            const float x = GameModel::columnX(gadget.column), y = config::laneY[lane];
            art.gadget(gadget.type, x, y, health, time_, 1.0f, coneStyle);
            if (gadget.type != GadgetType::Cone)
            {
                art.rect(x - 26, y - 14, 52, 7, ink);
                art.rect(x - 24, y - 12, 48 * health, 3, health < 0.3f ? coral : mint);
                if (health < 0.3f && static_cast<int>(time_ * 4) % 2)
                    art.text("!", x + 29, y + 40, 2, coral);
            }
        }
        drawLyingThrown(art, lane);
        drawCrowd(art, lane);
    }
    drawEffects(art);
    drawBubbles(art);
}

// Bricks and smoke bombs lying in one lane's road, under the crowd's feet.
void GameScene::drawLyingThrown(PixelArt& art, int lane)
{
    for (const Thrown& item : thrown_)
    {
        if (!item.landed || item.lane != lane)
            continue;
        if (item.kind == Throwable::Brick)
            art.brick(item.position.x, item.position.y,
                      0.0f, std::clamp((flavour::brickRestSeconds - item.life) / 0.8f, 0.0f, 1.0f));
        else
            art.smokeBomb(item.position.x, item.position.y, 0.0f, item.colour,
                          std::clamp((flavour::smokeBurnSeconds + canisterLinger - item.life) / 0.8f, 0.0f, 1.0f));
    }
}

// Coloured smoke, then anything still in the air, then the fizzing ends of lit smoke bombs, which shine through
// their own smoke.
void GameScene::drawSmoke(PixelArt& art)
{
    for (const Puff& puff : smoke_)
    {
        const float age = puff.age / puff.lifetime;
        const float alpha = 0.7f * std::min(1.0f, puff.age * 6.0f) * (1.0f - age * age);
        art.puff(puff.position.x, puff.position.y, puff.radius + puff.grow * puff.age,
                 blend(puff.colour, 0xfff8f0, 0.15f + age * 0.35f), alpha);
    }
    for (const Thrown& item : thrown_)
    {
        if (item.landed)
            continue;
        if (item.kind == Throwable::Brick)
            art.brick(item.position.x, item.position.y, item.spin);
        else
            art.smokeBomb(item.position.x, item.position.y, item.spin, item.colour);
    }
    for (const Thrown& item : thrown_)
    {
        const float strength = flareStrength(item);
        if (strength > 0.0f)
            art.flare(item.landed ? item.position.x + 8.0f : item.position.x, item.position.y, item.colour,
                      time_ + static_cast<float>(item.lane) * 0.37f + item.spinRate, strength);
    }
}

// One lane of rioters, back to front so the ones nearest the target are on top; leavers walk in front.
void GameScene::drawCrowd(PixelArt& art, int lane)
{
    std::vector<const Walker*> crowd;
    for (const auto& walker : model_.walkers())
        if (walker.lane == lane && walker.x > -40)
            crowd.push_back(&walker);
    std::sort(crowd.begin(), crowd.end(), [](const Walker* a, const Walker* b) {
        return a->leaving != b->leaving ? b->leaving : a->x < b->x;
    });
    const ConeArt coneStyle = cones[static_cast<std::size_t>(model_.progress().cone)].art;
    for (const Walker* walker : crowd)
    {
        WalkerPose pose;
        pose.look = walker->look;
        pose.time = time_;
        pose.blocked = walker->blockingGadget >= 0;
        pose.stumble = walker->stumble;
        pose.leaving = walker->leaving;
        pose.bored = walker->bored && !walker->leaving;
        pose.cone = walker->carryingCone;
        pose.balaclava = flavour::wearsBalaclava(walker->look);
        pose.coneStyle = coneStyle;
        pose.prop = flavour::chooseProp(walker->look, model_.level());
        if (const auto it = fx_.find(walker->id); it != fx_.end())
        {
            const WalkerFx& fx = it->second;
            if (fx.squash > 0.0f)
            {
                // Squash, then overshoot into a stretch, then settle.
                const float left = fx.squash / fx.squashLength;
                pose.squash = fx.squashPower * left * std::cos((1.0f - left) * 9.0f);
            }
            pose.surprised = fx.surprise > 0.0f;
            pose.exclaim = fx.exclaim > 0.0f;
            pose.sweat = fx.sweat > 0.0f;
            pose.throwing = fx.throwing > 0.0f && !walker->leaving && walker->stumble <= 0.0f;
        }
        art.walker(walker->x, config::laneY[lane] - (walker->leaving ? 8.0f : 0.0f), pose);
    }
}

void GameScene::drawEffects(PixelArt& art)
{
    drawSmoke(art);
    for (const auto& particle : particles_)
        art.rect(particle.position.x, particle.position.y, particle.size, particle.size,
                 particle.color, std::min(1.0f, particle.life * 3));
    for (const auto& popup : popups_)
        art.centered(popup.text, popup.position.x, popup.position.y, 1, popup.color);
}

void GameScene::drawBubbles(PixelArt& art)
{
    using namespace palette;
    for (const Bubble& bubble : bubbles_)
    {
        const auto lines = PixelArt::wrap(bubble.text, 150.0f, 1);
        float textW = 0.0f;
        for (const auto& line : lines)
            textW = std::max(textW, PixelArt::textWidth(line, 1));
        const float w = std::round(textW + 12.0f);
        const float h = static_cast<float>(lines.size()) * 10.0f + 7.0f;
        // Pops up a few pixels as it appears.
        const float age = flavour::bubbleSeconds - bubble.life;
        const float bottom = std::round(bubble.anchor.y + 92.0f - std::max(0.0f, 1.0f - age * 8.0f) * 6.0f);
        const float left = std::round(std::clamp(bubble.anchor.x - w * 0.5f, 6.0f, 1076.0f - w));
        const float tail = std::round(std::clamp(bubble.anchor.x, left + 8.0f, left + w - 8.0f));
        art.rect(left - 2, bottom - 2, w + 4, h + 4, ink);
        art.rect(tail - 5, bottom - 6, 10, 5, ink);
        art.rect(tail - 3, bottom - 10, 6, 5, ink);
        art.rect(left, bottom, w, h, 0xffffff);
        art.rect(tail - 3, bottom - 4, 6, 5, 0xffffff);
        art.rect(tail - 1, bottom - 8, 2, 4, 0xffffff);
        float baseline = bottom + h - 6.0f - 7.0f;
        for (const auto& line : lines)
        {
            art.centered(line, left + w * 0.5f, baseline, 1, ink);
            baseline -= 10.0f;
        }
    }
}

void GameScene::drawGoalTracker(PixelArt& art)
{
    using namespace palette;
    const LevelDef& level = model_.level();
    float x = 1256.0f;
    for (int i = goalsPerLevel - 1; i >= 0; --i)
    {
        const bool onTrack = model_.goalOnTrack(i);
        const std::string label = trackerText(level.goals[static_cast<std::size_t>(i)]);
        x -= PixelArt::textWidth(label, 1);
        art.text(label, x, 46, 1, onTrack ? paper : 0x5b7280);
        x -= 9.0f;
        art.star(x, 49.5f, 1, onTrack ? yellow : 0x3c5661);
        x -= 18.0f;
    }
    const std::string heading = "BONUS STARS:";
    art.text(heading, x - PixelArt::textWidth(heading, 1) + 4, 46, 1, muted);
}

void GameScene::drawHud(PixelArt& art)
{
    using namespace palette;
    const LevelDef& level = model_.level();
    art.rect(0, 688, 1280, 112, dark);
    art.rect(0, 685, 1280, 3, yellow);
    art.rect(24, 733, 44, 44, blue);
    art.text("RC", 29, 744, 3, dark);
    art.text("RIOT CONTROL", 84, 756, 3, paper);
    art.text("LEVEL " + number(model_.campaignNumber()) + "/" + number(levelCount * 2), 330, 756, 2, yellow);
    art.text(upper(level.cause), 86, 735, 1, muted);
    const int waves = model_.waveCount();
    const int wave = std::max(1, model_.waveIndex() + 1);
    art.text("WAVE " + number(wave) + "/" + number(waves), 26, 708, 2, mint);
    const float pipPitch = waves <= 6 ? 24.0f : 18.0f;
    for (int i = 0; i < waves; ++i)
        art.rect(174 + i * pipPitch, 708, pipPitch - 8, 12, i < wave ? mint : 0x344753);
    art.text("HELD OFF " + number(model_.heldOff()), 342, 716, 1, muted);
    art.text("WENT HOME " + number(model_.gaveUp()), 342, 704, 1, model_.gaveUp() > 0 ? mint : muted);
    art.rect(489, 709, 2, 70, 0x334955);
    art.text("BUDGET", 516, 762, 1, muted);
    art.text("$" + number(static_cast<int>(model_.budget())), 516, 726, 4, yellow);
    art.text("+$" + number(static_cast<int>(config::budgetPerSecond)) + "/SEC IN WAVES", 516, 708, 1, muted);
    art.text("CROWD MOOD", 729, 762, 1, muted);
    art.text(number(static_cast<int>(model_.mood())) + "%", 951, 759, 2, paper);
    art.rect(729, 731, 274, 17, 0x334955);
    art.rect(732, 734, 268 * model_.mood() / config::maximumMood, 11, model_.mood() > moodPanic ? coral : yellow);
    art.text(model_.mood() > moodPanic ? "THEY'RE NEARLY THERE!" : "KEEP THEM FROM " + upper(level.target), 729, 708, 1,
             model_.mood() > moodPanic ? coral : muted);
    art.rect(1030, 709, 2, 70, 0x334955);
    std::string phase = model_.phase() == Phase::Intermission ? "TEA BREAK" :
                        model_.phase() == Phase::Wave ? "UNTIL WHISTLE" : "PREP TIME";
    art.text(phase, 1058, 762, 1, muted);
    art.text(number(static_cast<int>(std::ceil(model_.remaining()))) + "s", 1060, 714, 5, paper);

    art.rect(0, 0, 1280, 168, dark);
    art.rect(0, 165, 1280, 3, 0x3c5661);
    constexpr std::array<const char*, 4> names{"CONE", "BARRIER", "VAN", "REPAIR"};
    const std::array<std::string, 4> hints{"TRIP / SINGLE USE", "YOUR MAIN WALL",
                                         number(model_.maxVans() - model_.vansPurchased()) + " LEFT THIS LEVEL",
                                         "+" + number(static_cast<int>(model_.repairHealth())) + " HEALTH"};
    const ConeArt coneStyle = cones[static_cast<std::size_t>(model_.progress().cone)].art;
    for (int i = 0; i < 4; ++i)
    {
        const float x = 24 + i * 238.0f;
        const int cost = i == 3 ? model_.repairCost() : model_.cost(static_cast<GadgetType>(i));
        const bool selected = i == selected_;
        const bool available = model_.budget() >= static_cast<float>(cost) &&
                               (i != 2 || model_.vansPurchased() < model_.maxVans());
        art.rect(x, 70, 226, 84, selected ? 0x334c58 : 0x213543);
        art.frame(x, 70, 226, 84, selected ? yellow : 0x425a65, selected ? 3.0f : 2.0f);
        if (i < 3)
            art.gadget(static_cast<GadgetType>(i), x + 42, 86, 1, time_, i == 2 ? 0.75f : 1.0f, coneStyle);
        else
            art.wrench(x + 41, 108, mint);
        art.text(number(i + 1) + " " + names[i], x + 82, 129, 2, selected ? yellow : paper);
        art.text("$" + number(cost), x + 82, 104, 2, available ? mint : coral);
        art.text(hints[i], x + 82, 86, 1, muted);
    }
    art.rect(986, 82, 270, 67, mint);
    art.rect(986, 78, 270, 4, 0x3f9b8d);
    const std::string action = paused_ ? "RESUME >" : model_.phase() == Phase::Wave ? "PAUSE" :
                               model_.phase() == Phase::Intermission ? "NEXT WAVE >" : "READY >";
    art.centered(action, 1121, 119, 2, ink);
    art.centered("[SPACE]", 1121, 97, 1, ink);
    art.text(model_.feedback(), 26, 46, 1, paper);
    drawGoalTracker(art);
    art.text("1-4 SELECT / CLICK TO PLACE / RIGHT CLICK REPAIRS / P OR ESC PAUSES", 26, 22, 1, muted);
    drawSystemHints(art);
}

// Sound and window hints along the bottom edge; the click areas for them live in click().
void GameScene::drawSystemHints(PixelArt& art)
{
    using namespace palette;
    art.text(audioFailed_ ? "AUDIO UNAVAILABLE" : muted_ ? "[M] SOUND OFF" : "[M] SOUND ON", 1045, 24, 1,
             audioFailed_ ? coral : muted);
    const auto* view = renderView();
    std::string windowHint = view && view->isFullscreen() ? "[F11] WINDOWED" : "[F11] FULLSCREEN";
    if (model_.phase() == Phase::Title)
        windowHint = "[ESC] QUIT   " + windowHint;
    else if (onCard())
        windowHint = "[ESC] BACK   " + windowHint;
    if (saveFailed_)
        windowHint = "PROGRESS NOT SAVED!   " + windowHint;
    art.text(windowHint, 1000 - PixelArt::textWidth(windowHint, 1), 24, 1, saveFailed_ ? coral : muted);
}

void GameScene::drawOverlay(PixelArt& art)
{
    using namespace palette;
    const auto phase = model_.phase();
    if (phase == Phase::Intro)
        drawIntro(art);
    else if (phase == Phase::Won || phase == Phase::Lost)
        drawReport(art);
    else if (model_.canBuild())
    {
        if (paused_)
        {
            art.rect(350, 300, 580, 150, dark, 0.94f);
            art.frame(350, 300, 580, 150, mint);
            art.centered("PAUSED FOR A CUPPA", 640, 405, 3, paper);
            art.centered("SPACE OR P TO CARRY ON. Q QUITS TO THE MAP.", 640, 382, 1, muted);
            button(art, {380, 324, 250, 44}, "RESUME", 2, Command::Resume);
            button(art, {650, 324, 250, 44}, "QUIT TO MAP", 2, Command::Quit);
        }
        else if (banner_ > 0)
        {
            art.rect(366, 553, 548, 61, dark, 0.94f);
            art.centered("WAVE " + number(model_.waveIndex() + 1) + " / HOLD THE LINE!", 640, 585, 2, yellow);
            art.centered("THE CROWD DISPERSES AT THE WHISTLE", 640, 566, 1, paper);
        }
        else if (phase == Phase::Prep || phase == Phase::Intermission)
        {
            art.rect(328, 561, 640, 37, dark, 0.93f);
            art.centered(phase == Phase::Prep ? "PLACE YOUR GADGETS. THEN HIT READY." :
                         "CROWD DISPERSED! PATCH UP YOUR LINE.", 648, 574, 2, paper);
        }
    }
}

void GameScene::button(PixelArt& art, const Button& box, std::string_view label, int scale, Command command, int arg,
                       bool enabled)
{
    using namespace palette;
    const bool hot = enabled && inside(pointer_, box.x, box.y - 6, box.w, box.h + 6);
    art.rect(box.x, box.y - 6, box.w, box.h, enabled ? 0xb99146 : 0x8d8676);
    art.rect(box.x, box.y, box.w, box.h, !enabled ? 0xcbc3b0 : hot ? 0xffe08c : yellow);
    art.frame(box.x, box.y, box.w, box.h, enabled ? ink : 0x8d8676, 3);
    art.centered(label, box.x + box.w * 0.5f, box.y + (box.h - 7.0f * static_cast<float>(scale)) * 0.5f, scale,
                 enabled ? ink : 0x7a7466);
    if (enabled)
        hotspots_.push_back({box, command, arg});
}

namespace
{
struct LineStyle
{
    int scale;
    float pitch;  // distance from the top of this line to the top of the next one
    std::uint32_t color;
    bool centered;
};

// Indexed by CopyStyle.
constexpr std::array<LineStyle, 7> longCopy{{
    {6, 66, palette::ink, true},    // Title
    {3, 38, coralInk, true},        // Lead
    {2, 26, palette::ink, false},   // Body
    {2, 30, coralInk, false},       // Heading
    {2, 26, palette::ink, false},   // Bullet
    {3, 21, palette::ink, true},    // Closer
    {1, 22, palette::ink, false},   // Gap
}};

constexpr std::array<LineStyle, 7> shortCopy{{
    {7, 79, palette::ink, true},    // Title
    {3, 40, coralInk, true},        // Lead
    {3, 40, palette::ink, true},    // Body
    {3, 40, coralInk, true},        // Heading
    {3, 40, palette::ink, true},    // Bullet
    {3, 21, palette::ink, true},    // Closer
    {1, 30, palette::ink, false},   // Gap
}};

constexpr float bulletIndent = 30.0f;
}

void GameScene::drawTitle(PixelArt& art)
{
    using namespace palette;
    art.rect(0, 0, 1280, 800, dark);
    drawCard(art, 100, 60, 1080, 716);

    const bool brief = shortStart();
    const std::span<const CopyLine> copy = brief ? std::span<const CopyLine>(startCopyShort)
                                                 : std::span<const CopyLine>(startCopy);
    const auto& styles = brief ? shortCopy : longCopy;

    // Left-aligned lines form one block, centred on the card.
    float blockWidth = 0.0f;
    float height = 0.0f;
    for (std::size_t i = 0; i < copy.size(); ++i)
    {
        const LineStyle& style = styles[static_cast<std::size_t>(copy[i].style)];
        height += i + 1 == copy.size() ? 7.0f * static_cast<float>(style.scale) : style.pitch;
        if (!style.centered)
        {
            const float indent = copy[i].style == CopyStyle::Bullet ? bulletIndent : 0.0f;
            blockWidth = std::max(blockWidth, indent + PixelArt::textWidth(copy[i].text, style.scale));
        }
    }
    const float left = 640.0f - blockWidth * 0.5f;
    // The long copy hangs from the top of the card; the short copy sits in the middle of the space above the button.
    float top = brief ? 500.0f + height * 0.5f : 746.0f;
    for (std::size_t i = 0; i < copy.size(); ++i)
    {
        const CopyLine& line = copy[i];
        const LineStyle& style = styles[static_cast<std::size_t>(line.style)];
        const float baseline = top - 7.0f * static_cast<float>(style.scale);
        const std::string_view text = line.text;
        if (line.style == CopyStyle::Title)
        {
            art.centered(text, 643, baseline - 3, style.scale, 0xd9c9a3);
            art.centered(text, 640, baseline, style.scale, style.color);
        }
        else if (style.centered)
            art.centered(text, 640, baseline, style.scale, style.color);
        else if (line.style == CopyStyle::Bullet)
        {
            const float size = 2.0f * static_cast<float>(style.scale);
            art.rect(left + 4, baseline + 3, size, size, coralInk);
            art.text(text, left + bulletIndent, baseline, style.scale, style.color);
        }
        else
            art.text(text, left, baseline, style.scale, style.color);
        top -= style.pitch;
    }

    // Crowd on the left, police on the right.
    WalkerPose placard;
    placard.look = 0;
    placard.time = time_;
    placard.prop.kind = flavour::PropKind::Placard;
    placard.prop.text = flavour::genericPlacards[0];
    placard.prop.board = 2;
    placard.prop.ink = 1;
    art.walker(180, 118, placard, 3.0f);
    WalkerPose flag;
    flag.look = 27;
    flag.balaclava = true;
    flag.time = time_;
    flag.prop.kind = flavour::PropKind::Flag;
    flag.prop.flag = static_cast<int>(flavour::flags.size()) - 2;
    flag.prop.lift = 1;
    art.walker(290, 118, flag, 3.0f);
    art.rect(1085, 114, 70, 6, ink, 0.25f);
    art.officer(1120, 118, 5, hats[static_cast<std::size_t>(model_.progress().hat)].art);
    art.gadget(GadgetType::Barrier, 1020, 104, 1, time_, 1.6f);

    button(art, {450, 108, 380, 62}, startButton, 3, Command::Primary);
    drawSystemHints(art);
}

void GameScene::drawMap(PixelArt& art)
{
    using namespace palette;
    const Progress& progress = model_.progress();
    art.rect(0, 0, 1280, 800, dark);
    drawCard(art, 100, 60, 1080, 716);
    drawTitleText(art, "PICK YOUR BATTLE", 690, 5, ink);
    art.centered("STARS " + number(progress.totalStars()) + "/" + number(levelCount * maxStarsPerLevel) +
                 "     UPGRADE POINTS " + number(progress.points()), 640, 648, 2, coralInk);

    const int frontier = model_.frontierLevel();
    for (int i = 0; i < levelCount; ++i)
    {
        const LevelDef& level = levels[static_cast<std::size_t>(i)];
        const Button tile{140.0f + static_cast<float>(i % 3) * 340.0f, i < 3 ? 410.0f : 195.0f, 320, 190};
        const bool open = progress.levelUnlocked(i);
        const bool hot = open && inside(pointer_, tile.x, tile.y, tile.w, tile.h);
        art.rect(tile.x + 8, tile.y - 6, tile.w, tile.h, 0xb9a888);
        art.rect(tile.x, tile.y, tile.w, tile.h, !open ? 0xd8cbb0 : hot ? 0xfff6dc : panel);
        art.frame(tile.x, tile.y, tile.w, tile.h, open && i == frontier ? coralInk : ink, open && i == frontier ? 4.0f : 3.0f);
        art.text("LEVELS " + number(i * 2 + 1) + "-" + number(i * 2 + 2), tile.x + 18, tile.y + 152, 2,
                 open ? ink : brownInk);
        art.text("[" + number(i + 1) + "]", tile.x + tile.w - 38, tile.y + 160, 1, brownInk);
        if (!open)
        {
            art.padlock(tile.x + tile.w * 0.5f, tile.y + 62, 5, brownInk);
            art.centered("CLEAR DEFEND " + number(i * 2) + " TO UNLOCK", tile.x + tile.w * 0.5f, tile.y + 34, 1, brownInk);
            continue;
        }
        float baseline = tile.y + 124;
        for (const std::string& line : PixelArt::wrap(level.cause, tile.w - 36, 2))
        {
            art.text(line, tile.x + 18, baseline, 2, coralInk);
            baseline -= 20.0f;
        }
        const bool pending = progress.stars[i] > 0 && !progress.defended[i];
        art.text(pending ? "NEXT: FIRST-PERSON DEFEND" : progress.defended[i] ? "RIOT + DEFEND CLEARED" : "RIOT THEN FIRST-PERSON DEFEND",
                 tile.x + 18, tile.y + 52, 1, pending ? coralInk : brownInk);
        for (int star = 0; star < maxStarsPerLevel; ++star)
            drawStar(art, tile.x + 34 + static_cast<float>(star) * 36, tile.y + 26, 3,
                     star < progress.stars[static_cast<std::size_t>(i)], false);
        if (i == frontier)
        {
            const float nudge = std::abs(std::sin(time_ * 5.0f)) * 5.0f;
            art.text("PLAY >", tile.x + tile.w - 92 + nudge, tile.y + 20, 2, coralInk);
        }
        hotspots_.push_back({tile, Command::Level, i});
    }
    art.centered("PICK A PAIR [1-6]. SPACE CONTINUES. S SHOP, W WARDROBE.", 640, 172, 1, brownInk);
    const int points = progress.points();
    button(art, {140, 95, 320, 62}, "SHOP: " + number(points) + (points == 1 ? " POINT" : " POINTS"), 2,
           Command::Shop);
    if (progress.anyAffordable())
    {
        art.rect(400, 140, 70, 26, coral);
        art.frame(400, 140, 70, 26, ink, 2);
        art.centered("NEW!", 435, 150, 1, paper);
    }
    button(art, {480, 95, 320, 62}, "WARDROBE", 2, Command::Wardrobe);
    button(art, {820, 95, 320, 62}, "< TITLE", 2, Command::Title);
    drawSystemHints(art);
}

void GameScene::drawShop(PixelArt& art)
{
    using namespace palette;
    const Progress& progress = model_.progress();
    art.rect(0, 0, 1280, 800, dark);
    drawCard(art, 100, 60, 1080, 716);
    drawTitleText(art, "UPGRADE SHOP", 690, 5, ink);
    art.centered("POINTS TO SPEND: " + number(progress.points()), 640, 645, 3, coralInk);
    art.centered("1 POINT PER NEW STAR. REPLAY LEVELS FOR MORE. UPGRADES WORK ON EVERY LEVEL.", 640, 622, 1,
                 brownInk);

    const ConeArt coneStyle = cones[static_cast<std::size_t>(progress.cone)].art;
    for (int i = 0; i < upgradeCount; ++i)
    {
        const UpgradeDef& upgrade = upgrades[static_cast<std::size_t>(i)];
        const int tier = progress.tiers[static_cast<std::size_t>(i)];
        const float y = 510.0f - static_cast<float>(i) * 110.0f;
        art.rect(150, y, 980, 96, panel);
        switch (upgrade.effect)
        {
        case UpgradeEffect::BarrierHealth:
            art.gadget(GadgetType::Barrier, 205, y + 26, 1, time_, 1.0f);
            break;
        case UpgradeEffect::StartingBudget:
            art.centered("$", 205, y + 31, 5, greenInk);
            break;
        case UpgradeEffect::RepairHealth:
            art.wrench(205, y + 44, greenInk);
            break;
        case UpgradeEffect::ConeDiscount:
            art.gadget(GadgetType::Cone, 205, y + 24, 1, time_, 1.3f, coneStyle);
            break;
        }
        art.text(number(i + 1) + " " + upgrade.name, 260, y + 62, 3, ink);
        art.text(upgrade.blurb, 260, y + 36, 2, brownInk);
        art.text("NOW: " + upgradeEffectText(upgrade, tier), 260, y + 13, 1, tier > 0 ? greenInk : brownInk);
        if (tier < upgrade.tierCount)
            art.text("NEXT: " + upgradeEffectText(upgrade, tier + 1), 560, y + 13, 1, coralInk);
        for (int pip = 0; pip < upgrade.tierCount; ++pip)
        {
            const float x = 800.0f + static_cast<float>(pip) * 28.0f;
            art.rect(x, y + 58, 20, 20, pip < tier ? greenInk : 0xcdbf9f);
            art.frame(x, y + 58, 20, 20, ink, 2);
        }
        const int price = progress.nextTierCost(i);
        const std::string label = price == 0 ? "MAXED" : "BUY: " + number(price) + (price == 1 ? " PT" : " PTS");
        button(art, {900, y + 18, 210, 60}, label, 2, Command::Buy, i, price > 0 && progress.canBuy(i));
    }
    art.centered(model_.feedback(), 640, 160, 1, ink);
    button(art, {150, 85, 300, 62}, "REFUND ALL", 2, Command::Refund, 0, progress.pointsSpent() > 0);
    const bool onward = model_.shopFrom() == Phase::DefendWon;
    button(art, {680, 85, 450, 62}, !onward ? "< BACK TO MAP" : model_.lastLevel() ? "FINISH >" : "NEXT LEVEL >", 2,
           Command::Primary);
    art.centered("[1-4] BUY  [R] REFUND", 565, 112, 1, brownInk);
    drawSystemHints(art);
}

void GameScene::drawWardrobe(PixelArt& art)
{
    using namespace palette;
    const Progress& progress = model_.progress();
    art.rect(0, 0, 1280, 800, dark);
    drawCard(art, 100, 60, 1080, 716);
    drawTitleText(art, "WARDROBE", 690, 5, ink);
    art.centered("PURELY FOR SHOW. NONE OF THIS CHANGES THE GAME.", 640, 648, 2, coralInk);

    // A preview of the current look on a little bit of street.
    const auto& hat = hats[static_cast<std::size_t>(progress.hat)];
    const auto& cone = cones[static_cast<std::size_t>(progress.cone)];
    art.rect(150, 200, 300, 410, 0x92a6a0);
    for (int j = 0; j < 3; ++j)
    {
        art.rect(172 + j * 96.0f, 470, 60, 90, 0x677680);
        art.rect(180 + j * 96.0f, 510, 44, 30, 0xd8b98f);
    }
    art.rect(150, 200, 300, 80, 0xd3c9ac);
    art.rect(150, 276, 300, 6, 0xf9e7c0);
    art.frame(150, 200, 300, 410, ink, 3);
    art.rect(222, 266, 60, 8, ink, 0.25f);
    art.officer(250, 270, 6, hat.art);
    art.gadget(GadgetType::Cone, 385, 270, 1, time_, 2.0f, cone.art);
    art.centered(std::string(hat.name) + " + " + cone.name, 300, 232, 1, ink);

    const auto tile = [](int index, float y) { return Button{500.0f + static_cast<float>(index) * 128.0f, y, 120, 130}; };
    const auto tileBack = [&](const Button& box, bool unlocked, bool worn) {
        const bool hot = unlocked && inside(pointer_, box.x, box.y, box.w, box.h);
        art.rect(box.x, box.y, box.w, box.h, hot ? 0xfff6dc : panel);
        art.frame(box.x, box.y, box.w, box.h, worn ? greenInk : ink, worn ? 4.0f : 2.0f);
    };
    const auto tileFront = [&](const Button& box, bool unlocked, bool worn, const Unlock& unlock) {
        if (worn)
        {
            art.rect(box.x + box.w - 46, box.y + box.h - 18, 46, 18, greenInk);
            art.text("WORN", box.x + box.w - 40, box.y + box.h - 13, 1, paper);
        }
        if (unlocked)
            return;
        art.rect(box.x, box.y, box.w, box.h, dark, 0.6f);
        art.padlock(box.x + box.w * 0.5f, box.y + 66, 4, paper);
        float baseline = box.y + 44;
        for (const std::string& line : PixelArt::wrap(unlockText(unlock), box.w - 12, 1))
        {
            art.centered(line, box.x + box.w * 0.5f, baseline, 1, paper);
            baseline -= 11.0f;
        }
    };

    art.text("HATS", 500, 590, 3, ink);
    for (int i = 0; i < hatCount; ++i)
    {
        const HatDef& def = hats[static_cast<std::size_t>(i)];
        const Button box = tile(i, 440);
        const bool unlocked = progress.unlocked(def.unlock);
        tileBack(box, unlocked, progress.hat == i);
        art.officer(box.x + 60, box.y + 34, 2, def.art);
        art.centered(def.name, box.x + 60, box.y + 12, 1, ink);
        tileFront(box, unlocked, progress.hat == i, def.unlock);
        if (unlocked)
            hotspots_.push_back({box, Command::Hat, i});
    }
    art.text("CONES", 500, 410, 3, ink);
    for (int i = 0; i < coneCount; ++i)
    {
        const ConeDef& def = cones[static_cast<std::size_t>(i)];
        const Button box = tile(i, 260);
        const bool unlocked = progress.unlocked(def.unlock);
        tileBack(box, unlocked, progress.cone == i);
        art.gadget(GadgetType::Cone, box.x + 60, box.y + 40, 1, time_, 1.5f, def.art);
        art.centered(def.name, box.x + 60, box.y + 12, 1, ink);
        tileFront(box, unlocked, progress.cone == i, def.unlock);
        if (unlocked)
            hotspots_.push_back({box, Command::Cone, i});
    }
    art.text("CLICK TO WEAR. EARN STARS AND BONUS GOALS TO UNLOCK MORE.", 500, 232, 1, brownInk);
    button(art, {440, 95, 400, 62}, "< BACK TO MAP", 2, Command::Back);
    drawSystemHints(art);
}

void GameScene::drawIntro(PixelArt& art)
{
    using namespace palette;
    const LevelDef& level = model_.level();
    const int index = model_.levelIndex();
    const Progress& progress = model_.progress();
    constexpr float cx = 540.0f;

    // The dimmed street stops short of the target so the building stays bright.
    art.rect(0, 0, 1068, 685, dark, 0.74f);
    drawCard(art, 70, 180, 940, 470);
    art.rect(86, 578, 908, 56, ink);
    art.text("LEVEL " + number(model_.campaignNumber()), 108, 587, 6, yellow);
    const float starsX = 108.0f + PixelArt::textWidth("LEVEL " + number(model_.campaignNumber()), 6) + 34.0f;
    for (int star = 0; star < maxStarsPerLevel; ++star)
        drawStar(art, starsX + static_cast<float>(star) * 34, 606, 3,
                 star < progress.stars[static_cast<std::size_t>(index)], true);
    art.text("RIOT", 660, 599, 2, coral);
    for (int i = 0; i < levelCount; ++i)
        art.rect(834 + i * 26.0f, 597, 18, 18, i < index ? mint : i == index ? yellow : 0x3c5661);

    const std::vector<std::string> lines = PixelArt::wrap(level.cause, 840.0f, 4);
    const float block = 28.0f + 40.0f * static_cast<float>(lines.size() - 1);
    float baseline = 520.0f + block * 0.5f - 28.0f;
    for (const std::string& line : lines)
    {
        art.centered(line, cx, baseline, 4, coralInk);
        baseline -= 40.0f;
    }
    art.rect(150, 462, 780, 3, 0xd9c9a3);

    const std::string label = "Target: ";
    const std::string name = level.target;
    const float x = cx - PixelArt::textWidth(label + name, 4) * 0.5f;
    art.text(label, x, 410, 4, brownInk);
    art.text(name, x + textAdvance(label, 4), 410, 4, ink);

    art.centered("BONUS GOALS: CLEAR THE LEVEL FOR 1 STAR, PLUS 1 STAR FOR EACH GOAL MET", cx, 378, 1, coralInk);
    for (int i = 0; i < goalsPerLevel; ++i)
    {
        const float y = 344.0f - static_cast<float>(i) * 32.0f;
        drawStar(art, 196, y + 7, 2, true, false);
        art.text(goalText(level.goals[static_cast<std::size_t>(i)]), 220, y, 2, ink);
        if (progress.goalsMet[static_cast<std::size_t>(index)] & (1u << i))
            art.text("DONE BEFORE", 832, y + 3, 1, greenInk);
    }
    if (level.crowdNote && *level.crowdNote)
    {
        const std::string heads = "Heads up: ";
        const std::string note = level.crowdNote;
        const float noteX = cx - PixelArt::textWidth(heads + note, 2) * 0.5f;
        art.text(heads, noteX, 278, 2, brownInk);
        art.text(note, noteX + textAdvance(heads, 2), 278, 2, ink);
    }

    // Point at the building the crowd is marching on.
    const float nudge = std::abs(std::sin(time_ * 5.0f)) * 9.0f;
    art.text(">", 1022 + nudge, 589, 6, yellow);

    button(art, {110, 200, 200, 62}, "< MAP", 2, Command::Map);
    button(art, {350, 200, 380, 62}, "START LEVEL >", 2, Command::Primary);
    drawSystemHints(art);
}

void GameScene::drawReport(PixelArt& art)
{
    using namespace palette;
    const LevelDef& level = model_.level();
    const LevelResult& result = model_.result();
    const bool won = model_.phase() == Phase::Won;

    art.rect(0, 0, 1280, 685, dark, 0.74f);
    drawCard(art, 171, 120, 938, 549);
    art.rect(187, 609, 906, 35, ink);
    art.text("LEVEL " + number(model_.campaignNumber()) + " REPORT", 207, 621, 2, yellow);
    const std::string target = upper(level.target);
    art.text(target, 1077 - PixelArt::textWidth(target, 1), 624, 1, paper);

    art.centered(won ? "LEVEL " + number(model_.campaignNumber()) + " CLEARED!" : "OH, CRUMBS.", 640, 541, 6,
                 won ? greenInk : redInk);
    art.centered(won ? level.victory : level.defeat, 640, 507, 2, ink);

    if (won)
    {
        for (int star = 0; star < maxStarsPerLevel; ++star)
        {
            const float age = screenTime_ - starDelay(star);
            const bool on = star < result.stars && age >= 0.0f;
            const float unit = on ? std::round(5.0f + std::max(0.0f, 1.0f - age * 6.0f) * 3.0f) : 5.0f;
            drawStar(art, 560.0f + static_cast<float>(star) * 80.0f, 455, unit, on, false);
        }
        if (result.stars > result.previousBest && screenTime_ >= starDelay(result.stars - 1) + 0.3f)
            art.text(result.previousBest > 0 ? "NEW BEST!" : "FIRST CLEAR!", 790, 450, 2, coralInk);
    }
    else
        art.centered("TIP: SPREAD BARRIERS ACROSS ALL LANES. REPAIR BEFORE THEY BREAK.", 640, 458, 1, brownInk);

    art.rect(235, 282, 810, 138, panel);
    const std::array<std::pair<const char*, int>, 5> stats{
        {{"RIOTERS HELD OFF", model_.heldOff()},
         {"WENT HOME BORED", model_.gaveUp()},
         {"COMEDY TRIPS", model_.trips()},
         {"STUFF LOBBED", model_.bricksThrown() + model_.smokeBombsThrown()},
         {"SLIPPED THROUGH", model_.reached()}}};
    for (std::size_t i = 0; i < stats.size(); ++i)
    {
        const float y = 396.0f - static_cast<float>(i) * 26.0f;
        art.text(stats[i].first, 255, y, 2, ink);
        const std::string value = number(stats[i].second);
        art.text(value, 620 - PixelArt::textWidth(value, 2), y, 2, i + 1 == stats.size() ? coralInk : greenInk);
    }
    art.rect(640, 292, 2, 118, 0xd9c9a3);
    art.text("BONUS GOALS", 662, 400, 1, coralInk);
    for (int i = 0; i < goalsPerLevel; ++i)
    {
        const float y = 366.0f - static_cast<float>(i) * 32.0f;
        const bool met = won ? result.goals[static_cast<std::size_t>(i)] : model_.goalOnTrack(i);
        drawTick(art, 662, y - 2, won && met);
        art.text(goalText(level.goals[static_cast<std::size_t>(i)]), 686, y, 2, won && met ? ink : brownInk);
    }
    if (!won)
        art.text("NO STARS THIS TIME. GO AGAIN!", 662, 302, 1, brownInk);
    else if (result.pointsEarned > 0)
        art.text("+" + number(result.pointsEarned) + (result.pointsEarned == 1 ? " UPGRADE POINT" : " UPGRADE POINTS"),
                 662, 300, 2, greenInk);
    else
        art.text("NO NEW STARS, SO NO NEW POINTS.", 662, 302, 1, brownInk);

    const std::string saved = std::string(level.summaryLabel) + ": " + (won ? std::string(level.summaryValue) : "none") + ".";
    art.centered(saved, 640, 258, 2, won ? ink : coralInk);
    if (won && !result.unlocked.empty())
    {
        std::string unlocked = "UNLOCKED ";
        for (std::size_t i = 0; i < result.unlocked.size(); ++i)
            unlocked += (i ? ", " : "") + result.unlocked[i];
        art.centered(unlocked + (result.unlocked.size() == 1 ? "! TRY IT ON IN THE WARDROBE." : "! TRY THEM ON IN THE WARDROBE."),
                     640, 232, 1, coralInk);
    }

    const std::string next = !won ? "TRY AGAIN >" : "DEFEND NEXT >";
    button(art, {231, 140, 260, 62}, "< MAP", 2, Command::Map);
    button(art, {640, 140, 400, 62}, next, 2, Command::Primary);
    drawSystemHints(art);
}

void GameScene::drawFinished(PixelArt& art)
{
    using namespace palette;
    const Progress& progress = model_.progress();
    art.rect(0, 0, 1280, 800, dark);
    drawCard(art, 100, 60, 1080, 716);

    drawTitleText(art, finalHeadline, 686, 6, greenInk);
    art.centered(finalSubline, 640, 636, 3, ink);

    // Everything the player saved along the way, with the best stars for each level.
    std::vector<std::string> saved;
    float width = 0.0f;
    for (const LevelDef& level : levels)
    {
        saved.push_back(std::string(level.summaryLabel) + ": " + level.summaryValue + ".");
        width = std::max(width, PixelArt::textWidth(saved.back(), 2));
    }
    constexpr float starsWidth = 3 * 22.0f + 20.0f;
    const float left = 640.0f - (width + starsWidth) * 0.5f;
    art.rect(left - 30, 358, width + starsWidth + 60, 232, panel);
    for (std::size_t i = 0; i < saved.size(); ++i)
    {
        const float y = 556.0f - static_cast<float>(i) * 34.0f;
        art.rect(left - 14, y + 3, 8, 8, greenInk);
        art.text(saved[i], left, y, 2, ink);
        for (int star = 0; star < maxStarsPerLevel; ++star)
            drawStar(art, left + width + 30 + static_cast<float>(star) * 22, y + 7, 2, star < progress.stars[i], false);
    }

    art.centered("RIOTERS HELD OFF: " + number(model_.totalHeldOff()) + " (" + number(model_.totalGaveUp()) +
                 " WENT HOME BORED)", 640, 318, 2, ink);
    art.centered("COMEDY TRIPS: " + number(model_.totalTrips()) + "     STARS: " + number(progress.totalStars()) + "/" +
                 number(levelCount * maxStarsPerLevel), 640, 284, 2, ink);
    art.centered("GO BACK FOR THE STARS YOU MISSED, OR TRY ON YOUR NEW PARTY HAT.", 640, 250, 1, brownInk);

    const HatArt hat = hats[static_cast<std::size_t>(progress.hat)].art;
    art.officer(176, 300, 5, hat);
    art.officer(1104, 300, 5, hat);
    button(art, {450, 108, 380, 62}, "BACK TO MAP >", 2, Command::Primary);
    drawSystemHints(art);
    drawEffects(art);
}

void GameScene::drawFrame()
{
    world_->clear();
    ui_->clear();
    hotspots_.clear();
    PixelArt world(world_);
    PixelArt ui(ui_);
    if (cinematic_.active())
    {
        drawCinematic(ui);
        return;
    }
    switch (model_.phase())
    {
    case Phase::Title:
        drawTitle(ui);
        break;
    case Phase::Map:
        drawMap(ui);
        break;
    case Phase::Shop:
        drawShop(ui);
        break;
    case Phase::Wardrobe:
        drawWardrobe(ui);
        break;
    case Phase::Finished:
        drawFinished(ui);
        break;
    case Phase::DefendIntro:
    case Phase::Defend:
    case Phase::DefendWon:
    case Phase::DefendLost:
        drawDefend(world);
        drawDefendCard(ui);
        drawSystemHints(ui);
        break;
    case Phase::Intro:
    case Phase::Prep:
    case Phase::Wave:
    case Phase::Intermission:
    case Phase::Won:
    case Phase::Lost:
        drawStreet(world);
        drawHud(ui);
        drawOverlay(ui);
        break;
    }
}
}
