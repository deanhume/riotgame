# Riot Control

A silly **native Windows desktop game**, written in C++20 with Axmol 2.11.5.
No browser, JavaScript, npm, or web server.

**Britain is in uproar. Again.** Six levels of very cross little people marching
on the town over the daftest things imaginable, from curry sauce to a cancelled
bank holiday.

Hold back the Walkers with cones, metal barriers, and a very limited fleet of
police vans. Survive six timed waves per level without crowd mood reaching 100%.
The rioters chatter in speech bubbles, wave daft placards and invented flags, and
eventually get bored and go home. Every level awards up to three stars, which buy
upgrades between levels and unlock purely cosmetic hats and cones.

## Play

For friends, share **`dist\RiotControl-Setup-0.1.0.exe`**. It installs the game
and its dependencies for the current Windows user, adds a Start Menu shortcut,
and provides an uninstaller in Windows Settings. No administrator rights,
developer tools, or internet connection are needed to play. Uninstalling keeps
saved progress. The installer is unsigned, so Windows may show an unknown
publisher or SmartScreen warning.

Double-click `dist\RiotControl\RiotControl.exe` after building. Keep the entire
`RiotControl` folder together: the executable needs its DLLs, shaders, and audio.

The game **starts full screen** on your main monitor at its native resolution.
The 1280x800 playfield scales up to fit, with navy bars at the sides on
widescreen displays. Press `F11` (or click the hint in the HUD) to switch
between full screen and a window. Like most full screen games, it minimises if
it loses focus (for example with Alt+Tab); click its taskbar button to return.
To start in a window instead, run `RiotControl.exe --windowed`.

### How a run goes

Start screen (the story and a START button) → **map** (pick a level) → intro
card → prep → six waves → level report → (upgrade shop) → next intro card → ...
→ level 6 → **"You saved the nation"** end screen, which lists every level's
result and best stars. Each intro card shows the level number, the daft cause,
the target the crowd is marching on, your best stars so far, the level's two
bonus goals, and a heads-up about what the crowd will throw. Each report shows the
stars earned, how many rioters you held off (and how many went home bored), how
much they lobbed at you, the bonus goals met, and a silly summary line for
that level ("Curry sauce saved: 1 bottle").

The map shows every level with its best stars. Level 1 is always open; each
later level opens once the one before it has been cleared, and cleared levels can
be replayed at any time for more stars. Space on the map plays the next level
that still needs clearing (or still lacks three stars).

If the crowd mood maxes out you lose and can **try the same level again**: it
restarts from a clean street, budget, and van allowance. Every level starts that
way; only stars, upgrades, and cosmetics carry over.

The start screen shows the full story, or a shorter version when the playfield
is drawn smaller than 960x600 pixels (for example in a small window).

### Stars, upgrades and the wardrobe

- **Stars:** clearing a level earns 1 star, plus 1 for each of its two bonus
  goals met in the same run (lose no barriers or vans, finish with money in the
  bank, keep the crowd mood under a set level, or win without a police van). The
  goals and how they are going are shown on the intro card and in the HUD.
- **Upgrade points:** 1 point per star, counted from your best result on each
  level, so replaying a level only pays out for stars you did not have before.
- **Upgrade shop:** reinforced barriers (more health), bigger budget (more
  starting money), faster repairs (each repair restores more health), and cheaper
  cones. Each has two or three tiers at rising prices; REFUND ALL gives every
  point back so you can try a different mix. The shop opens from the map, and
  after a win when you can afford something.
- **Wardrobe:** hats for the officer by the target (peaked cap, bobby helmet,
  cone hat, party hat, tiny crown) and designs for your cones (stripy, cool in
  sunglasses, friendly with a face, gold). They are unlocked by total stars, by
  clearing level 6, or by meeting certain types of bonus goal, and change nothing
  but the look.

**Saving:** progress is saved automatically to
`%LOCALAPPDATA%\RiotControl\progress.txt` whenever it changes. It is a short,
readable `key=value` text file (best stars, goals met, upgrade tiers, the hat and
cone being worn); delete it to start from scratch. A damaged file only loses the
damaged entries. If saving fails, the game says so in the corner and carries on.

### The crowd

Each Walker carries a placard (the level's own slogans or generic ones, some
misspelt on purpose), an invented flag (including rainbow stripes with an
oversized cartoon sausage dog), a deflated inflatable dinosaur, or nothing.
A Walker who trips over a cone pinches it and wears it as a hat. They
squash and wobble when they hit a wall, show a "!" or a sweat drop, and
occasionally say something in a speech bubble (at most three bubbles at once).

Walkers held up by a wall, or stuck in the queue behind one, slowly lose
**patience**. When it runs low they grumble; when it runs out they turn round and
go home. Walkers who go home count as held off and do not affect crowd mood, so a
solid wall visibly thins the crowd. Patience varies a little from Walker to
Walker, and later levels have more stubborn crowds.

Plenty of Walkers (about 60%) turn up in black knitted balaclavas. During waves
the crowd lobs **bricks** (aimed bricks bonk off the front of your walls) and,
from level 2 on, **smoke bombs** that land in the road and fizz like flares in
bright coloured smoke. Each level throws a bit more than the last; the level's
intro card says what to expect. Bricks and smoke are purely for show: they never
damage gadgets, raise mood or cost money, and they don't change the crowd.

### Controls

| Control | Action |
| --- | --- |
| Click a gadget card / `1`-`4` | Select cone, barrier, van, or repair |
| Click / tap a street tile | Place the selected gadget (or repair it with `4`) |
| Right-click a gadget | Quick repair |
| Space / Enter | Press the screen's main button (START, START LEVEL, NEXT LEVEL, TRY AGAIN, ...), ready the next wave, or pause/resume |
| `1`-`6` / `S` / `W` on the map | Pick a level / open the shop / open the wardrobe |
| `1`-`4` / `R` in the shop | Buy an upgrade / refund all |
| `P` | Pause/resume during a level |
| `Q` while paused | Quit the level and go back to the map |
| `M` / sound button | Mute/unmute |
| `F11` / full screen button | Toggle full screen and windowed |
| Escape | Go back: level cards and the end screen → map → start screen; close the shop or wardrobe; pause during a level; quit from the start screen |

### Rules

There are three independent lanes. Walkers push into obstacles rather than
pathfinding around them. Queued Walkers add pressure, so a crowd damages a wall
faster than one Walker. Cones trip one Walker and disappear. Repairs restore
85 health for $20 (more with the faster repairs upgrade); no charge for an
undamaged gadget. Only two vans can be purchased per level, even if they break.

**Economy:** every level starts with $260 (more with the bigger budget upgrade);
earn **$10 per second during waves**, capped at $420. No income during prep or
tea breaks, and no per-Walker rewards.

**Timed waves:** when the whistle blows, every remaining Walker disperses and
counts as held off. Walkers reaching the target raise mood and do not count.
Mood also rises in the marked danger zone and slowly recovers when it is empty.
Gadgets and their damage persist between waves. The last whistle wins the level.

Tip: spread your initial defense across all three lanes. Timing matters:
sometimes there is no need to repair a wall if the crowd cannot reach the
target before the whistle.

### The six levels

| # | Cause | Target |
| --- | --- | --- |
| 1 | No more curry sauce at the chippy! | The chippy |
| 2 | The last sausage roll has been sold! | The village bakery |
| 3 | Bin day has been moved to Thursday! | The council offices |
| 4 | Someone put pineapple on the town's official pizza! | Luigi's Pizzeria |
| 5 | The village hall has run out of biscuits! | The village hall tea urn |
| 6 | The bank holiday has been cancelled. Nobody knows why. | The town hall |

All six use the same mechanics (Walkers only; cone, barrier, and van). Each
level has six waves and a gentle ramp: roughly 195 Walkers in level 1 up to
roughly 249 in level 6, about 5% more per level. The final wave is the same size
in every level, because the damage a crowd does grows with the square of its
length, so a bigger finale makes a level unwinnable very quickly.

### Developer switches

Pass these to `RiotControl.exe`:

| Switch | Effect |
| --- | --- |
| `--windowed` | Start in a window instead of full screen |
| `--level N` (or `--level=N`) | Start on level N's intro card (1-6), even if it is locked |
| `--debug` | `F8` clears the current level, `F9` fails it (ignored without `--debug`) |
| `--save PATH` (or `--save=PATH`) | Load and save progress at PATH instead of `%LOCALAPPDATA%\RiotControl\progress.txt` |

## Build

Requires Windows 10/11 x64, Visual Studio 2022 or newer with **Desktop development
with C++**, the **C++ CMake tools** component, and Python 3.9+ on PATH.

From PowerShell in this repository:

```powershell
.\build.ps1
.\build.ps1 -Run
.\build.ps1 -Installer
```

The script locates Visual Studio's compiler, CMake, and Ninja. Missing Axmol and
shader-compiler files are downloaded into `.tools`; Axmol's build downloads its
pinned native dependencies from GitHub. The engine archive is SHA-256 verified.
No npm registry access, system-wide installs, execution-policy changes, or
engine setup script are used.

The build runs gameplay checks and writes the playable folder to
`dist\RiotControl`, including app-local C++ runtime DLLs and bundled notices.
The game works offline after building. First build takes longer because it
compiles the engine; later builds reuse it.

`-Installer` also creates the single shareable setup executable in `dist`.
It downloads a SHA-256-verified portable NSIS compiler into `.tools` on first
use, without installing tools system-wide. To repackage an existing build with
a new version number, run `.\tools\package_installer.ps1 -Version 0.1.1`.
The installer defaults to `%LOCALAPPDATA%\Programs\Riot Control`; updates reuse
the previous install location and do not reset saved progress.

## Tweak

**Levels are data.** `Source\Levels.h` holds everything that changes from level
to level, plus all of the story copy:

- the cause, target, win-card summary line, and victory/defeat captions
- the target building's sign, tagline, and window captions (`TargetLook`)
- the crowd size of each wave, written as `config::withCrowds({12, 18, ...})`
- the level's own placard slogans and near-the-target shouts ("My curry sauce!")
- its two bonus goals (`goals`) and how quickly its crowd gets bored
  (`patienceDrain`: lower is more stubborn)
- how often its crowd throws things (`bricksPerMinute`, `smokeBombsPerMinute`)
  and the "Heads up" line on its intro card (`crowdNote`)
- optional per-level overrides: `startingBudget`, `maxVans`, `prepSeconds`
- the start screen text (long and short versions) and the final screen text

To add a level, add an entry to `levels`, bump `levelCount`, and pick a
`TargetArt` (a new building style is a small addition to `Source\TargetArt.cpp`).
The model tests loop over `levels`, so a new level is checked automatically.

**Rioter personality is data.** `Source\Flavour.h` holds the speech bubble
phrases (each tied to a moment: walking, blocked, tripped, near the target,
bored, giving up, throwing), how often each moment speaks, how many bubbles can show at
once and for how long, the odds of each prop, the generic placards, the placard
colours, and the flags. Every flag and sign is invented; keep it that way. It
also holds the share of Walkers in balaclavas (`balaclavaPercent`) and their
colours, and the look of thrown things: smoke colours, how long a smoke bomb
burns, how much smoke and how many sparks it gives off, and how long a brick
lies in the road. `GameConfig.h` has how far the crowd can throw (`throwShortest`,
`throwLongest`, `throwReach`).

**Progression is data.** `Source\Progression.h` holds the bonus goal types, the
stars per goals met, points per star, the upgrades (each tier's cost and effect),
and the hats and cones with their unlock rules. To add an upgrade such as the tea
trolley later, add an `UpgradeEffect`, an entry in `upgrades`, and apply it in
`GameModel`. The save file stores ids, so renaming an id drops that entry from
existing saves.

`Source\GameConfig.h` contains costs, health, damage, income, mood rates, lane
positions, prep time, patience (`patienceSeconds`, `patienceSpread`, `boredAt`),
the shared wave pacing (`wavePacing`: how long each wave lasts and how long its
crowd takes to arrive), `shortStartBelowScale`, and `startFullscreen` (set it to
`false` to open in a window by default).

`GameModel` is engine-independent campaign and gameplay logic, and `Progress`
holds the saved progress and its file format. `GameScene` handles the screens,
input, feedback, speech bubbles, and rendering; `PixelArt` draws the original
pixel shapes and fonts (story text is mixed case, HUD labels stay uppercase, and
a tiny font letters the placards); `TargetArt` draws the six placeholder
buildings.
`tools\generate_audio.py` creates the placeholder arcade sounds locally using
only Python's standard library.

`tools\generate_branding.py` draws the original cone-in-a-police-cap icon and
matching wordmark without external fonts or image packages. The build embeds
the multi-size icon in the game and installer, and exports reusable ICO, PNG
and SVG artwork to `dist\RiotControl\branding`. Edit the generator's shapes and
palette to change the branding.

## Checks

```powershell
.\build.ps1 -ModelOnly
.\tools\smoke_test.ps1
```

Model checks cover the level data (five to eight waves, each bigger than the
last, no level easier than the one before), the campaign flow (title, intro,
prep, win, next level, end screen, back to the title), blocking, crowd pressure,
cones, economy, repair limits, van limits, timed dispersal, an undefended loss,
retry resetting the level, and the debug shortcuts. They also cover boredom
(stuck Walkers grumble, then go home a few at a time and count as held off),
thrown bricks and smoke bombs (thrown forward by marching Walkers, aimed bricks
land on the front of a wall in reach, no smoke bombs in level 1, every throw
counted),
stars and bonus goals, points only for new best stars, buying and refunding
upgrades and their effects, cosmetic unlocks, and saving and loading (including
damaged files). A simple bot that builds a
steady three-barrier line (tried in four different columns) and repairs it must
win every level, and the whole campaign is played through in sequence. That bot
is a floor, not a ceiling: winnable is not the same as fun, so play-test the
balance. `-ModelOnly` does not require Axmol or Python.

The native smoke check opens the game in its configured start mode (full screen
by default; `.\tools\smoke_test.ps1 -Windowed` starts windowed), exercises
mouse/keyboard controls, and toggles full screen and windowed with `F11` both
ways. It plays level 1 from the map and quits from the pause box; plays levels 5
and 6 to the end screen (buying an upgrade in the shop and putting on a hat in
the wardrobe), checks the save file, and restarts to check it loads; loses and
retries a level and backs out with Esc; checks that a small window switches to
the short start text; and checks Esc on the start screen. Each scenario uses its
own save file, so your real progress is never touched. It captures only the
game window into `.tools\smoke` and closes it. Extra switches:

- `-Tour` also captures every level's intro card, street, and a busy wave, plus
  the shop and wardrobe with everything unlocked.
- `-Natural` also loses level 1 for real (nothing built, no debug keys), which
  takes a minute or two.

It briefly takes over the screen, so leave the machine alone while it runs: the
game window takes focus, and any real key press or click would reach it. Its
captures support high-DPI Windows displays. Physical touchscreen hardware has
not been tested; tap interaction uses the engine's shared touch/left-click path.
