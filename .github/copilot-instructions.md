# Repository guide

## Build and checks

Run these commands from PowerShell at the repository root:

```powershell
.\build.ps1
.\build.ps1 -Run
.\build.ps1 -Installer
.\build.ps1 -ModelOnly
.\tools\smoke_test.ps1
```

`build.ps1` builds the Windows game and runs the gameplay checks; `-Run` launches the built game, `-Installer` packages it, and `-ModelOnly` builds and tests the engine-independent model without Axmol. The model-only build still needs the Visual Studio C++ and CMake tools described in `README.md`. The smoke test is an interactive desktop check and briefly takes over the screen.

The CMake test named `gameplay` is the single registered CTest test. Run it alone after a model-only build with:

```powershell
ctest --test-dir build\model -R "^gameplay$" --output-on-failure
```

Its assertions are grouped in `Tests\GameModelTests.cpp`'s single test executable; there is no per-assertion test selector.

For gameplay logic changes, prefer the headless model checks above. When rendering or input changes require native smoke checks, default to the quick run with failure-only screenshots:

```powershell
.\tools\smoke_test.ps1 -Quick -Screenshots Failures
.\tools\smoke_test.ps1 -Quick -Scenario Defend -Screenshots Failures
```

Select only the affected scenario groups (`Riot`, `Defend`, `Campaign`, `Window`, `Intro`; combine with commas). `Campaign` includes save/restart checks. `Intro` checks skipping in quick mode and automatic completion in the full run; other groups skip the cinematic. Rebuild the game first: the runner requires the debug-only state reporting in the current executable. Quick mode still exercises the real game window and inputs, but skips real-time barrage and animation waits.

Reserve the full run (without `-Quick`) for changes needing real-time gameplay verification. Use `-Tour` for visual captures or `-Natural` for an actual undefended riot loss; neither can be combined with `-Quick`. Screenshots default to key screens; use `-Screenshots All` only when intermediate captures are needed.

Changes to the smoke runner's waits, scenario selection, or screenshot policy have a separate headless check:

```powershell
.\Tests\SmokeHelpersTests.ps1
```

## Architecture

This is a native Windows C++20 game using Axmol. `GameModel` (`Source\GameModel.h/.cpp`) owns the engine-independent campaign and gameplay state. `GameScene` (`Source\GameScene.h/.cpp`) connects that model to Axmol: it handles screens, input, rendering, audio, and visual effects, advancing the model at `config::fixedStep` and translating model events into presentation.

Most gameplay and campaign variation is data-driven. `Source\GameConfig.h` defines shared mechanics; `Source\Levels.h` defines each level's story, waves, goals, and target appearance; `Source\Progression.h` defines goals, upgrades, and cosmetic unlocks; and `Source\Flavour.h` defines crowd dialogue and visual flavor. `PixelArt` and `TargetArt` draw the game's original pixel graphics. `Progress` (`Source\Progress.h/.cpp`) owns persistent campaign data and its key-value save format.

## Repository-specific conventions

- Keep simulation rules in `GameModel`, not in Axmol scene or rendering code. The scene consumes model events and owns effects that are purely visual.
- Tune shared rules in `GameConfig.h` and level-specific content in `Levels.h`; preserve the compile-time validation and the model tests' campaign/winnability expectations when changing level data.
- Keep progression definitions declarative in `Progression.h`. Upgrade and cosmetic IDs are serialized in save files, so changing an existing ID can make that saved entry unrecognized.
- Model checks use a small `check(condition, message)` helper in one executable rather than a unit-test framework. Add coverage alongside the related scenario in `GameModelTests.cpp`; CTest reports the executable as `gameplay`.
- Keep generated audio and branding sources reproducible through `tools\generate_audio.py` and `tools\generate_branding.py`; the CMake build generates their outputs rather than treating generated assets as hand-maintained source.
