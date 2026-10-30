#include "GameScene.h"

#include "audio/AudioEngine.h"
#include "base/Director.h"
#include "platform/Application.h"
#include "platform/RenderViewImpl.h"
#include <cstdlib>
#include <filesystem>
#include <string>
#include <string_view>
#include <windows.h>
#include <shellapi.h>

#pragma comment(lib, "shell32.lib")

class RiotApplication final : public ax::Application
{
public:
    RiotApplication(bool fullscreen, riot::GameScene::Options options) : fullscreen_(fullscreen), options_(options) {}

    void initGfxContextAttrs() override
    {
        GfxContextAttrs attributes{8, 8, 8, 8, 24, 8, 0};
        ax::RenderView::setGfxContextAttrs(attributes);
    }

    bool applicationDidFinishLaunching() override
    {
        auto* director = ax::Director::getInstance();
        constexpr std::string_view title = "Riot Control";
        ax::RenderViewImpl* view = fullscreen_ ? ax::RenderViewImpl::createWithFullScreen(title) : nullptr;
        if (!view)
            view = ax::RenderViewImpl::createWithRect(
                title, ax::Rect(0, 0, riot::config::width, riot::config::height), 1.0f, true);
        if (!view)
        {
            MessageBoxW(nullptr, L"Could not create the game window.", L"Riot Control", MB_ICONERROR);
            return false;
        }
        director->setRenderView(view);
        view->setDesignResolutionSize(riot::config::width, riot::config::height, ResolutionPolicy::SHOW_ALL);
        director->setAnimationInterval(1.0f / 60.0f);
        director->setStatsDisplay(false);
        // Letterbox bars on non-16:10 screens take the HUD colour instead of black.
        const std::uint32_t bars = riot::palette::dark;
        director->setClearColor(ax::Color4F(static_cast<float>((bars >> 16) & 0xff) / 255.0f,
                                            static_cast<float>((bars >> 8) & 0xff) / 255.0f,
                                            static_cast<float>(bars & 0xff) / 255.0f, 1.0f));
        auto* scene = riot::GameScene::create(options_);
        if (!scene)
        {
            MessageBoxW(nullptr, L"Could not initialize the game.", L"Riot Control", MB_ICONERROR);
            return false;
        }
        director->runWithScene(scene);
        return true;
    }

    void applicationDidEnterBackground() override
    {
        ax::Director::getInstance()->stopAnimation();
        ax::AudioEngine::pauseAll();
    }

    void applicationWillEnterForeground() override
    {
        ax::Director::getInstance()->startAnimation();
        ax::AudioEngine::resumeAll();
    }

    ~RiotApplication() override { ax::AudioEngine::end(); }

private:
    bool fullscreen_;
    riot::GameScene::Options options_;
};

namespace
{
// Command line switches:
//   --level N (or --level=N)  start on that level's intro card
//   --debug                   enable the F8 / F9 win / lose shortcuts
//   --windowed                start in a window instead of fullscreen
//   --save PATH (or --save=PATH)  where progress is saved; defaults to %LOCALAPPDATA%\RiotControl\progress.txt
struct LaunchOptions
{
    riot::GameScene::Options game;
    bool windowed = false;
};

std::filesystem::path defaultSavePath()
{
    std::wstring folder(MAX_PATH, L'\0');
    const DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", folder.data(), static_cast<DWORD>(folder.size()));
    if (length == 0 || length >= folder.size())
        return {};
    folder.resize(length);
    return std::filesystem::path(folder) / L"RiotControl" / L"progress.txt";
}

LaunchOptions parseOptions()
{
    LaunchOptions options;
    options.game.savePath = defaultSavePath();
    int count = 0;
    LPWSTR* arguments = CommandLineToArgvW(GetCommandLineW(), &count);
    if (!arguments)
        return options;
    // Accepts "--name value" and "--name=value"; returns false if `argument` is some other switch.
    const auto valueOf = [&](int& i, std::wstring_view name, std::wstring& value) {
        const std::wstring_view argument = arguments[i];
        if (argument == name && i + 1 < count)
        {
            value = arguments[++i];
            return true;
        }
        if (argument.size() > name.size() && argument.starts_with(name) && argument[name.size()] == L'=')
        {
            value = argument.substr(name.size() + 1);
            return true;
        }
        return false;
    };
    for (int i = 1; i < count; ++i)
    {
        const std::wstring_view argument = arguments[i];
        std::wstring value;
        if (argument == L"--debug")
            options.game.debug = true;
        else if (argument == L"--windowed")
            options.windowed = true;
        else if (valueOf(i, L"--level", value))
        {
            const long level = std::wcstol(value.c_str(), nullptr, 10);
            if (level > 0)
                options.game.startLevel = static_cast<int>(level);
        }
        else if (valueOf(i, L"--save", value) && !value.empty())
            options.game.savePath = value;
    }
    LocalFree(arguments);
    return options;
}
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    const LaunchOptions options = parseOptions();
    RiotApplication application(riot::config::startFullscreen && !options.windowed, options.game);
    return application.run();
}