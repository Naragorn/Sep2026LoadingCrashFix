# Sep 2026 Loading Crash Fix (Windows 11 KB5124010)

Oblivion, Fallout: New Vegas or Fallout 3 crashing right after the main menu,
just before the loading screen? If you're on Windows 11 and recently got
update **KB5124010**, this little plugin is for you.

**[⬇ Download the latest release](https://github.com/Naragorn/Sep2026LoadingCrashFix/releases/latest)**

## Install

Grab the zip for your game and install it with your mod manager, or copy the
folder inside it into your game's `Data` folder.

| Game | Needs | The DLL ends up in |
| --- | --- | --- |
| Oblivion | [xOBSE](https://github.com/llde/xOBSE) | `Data\OBSE\Plugins\` |
| Fallout: New Vegas | [xNVSE](https://github.com/xNVSE/NVSE) | `Data\NVSE\Plugins\` |
| Fallout 3 | [FOSE](http://fose.silverlock.org/) | `Data\FOSE\Plugins\` |

It's the same DLL for all three games. To remove it, just delete it.

Two things that trip people up:

- **The folder matters.** The DLL has to sit in `Data\<extender>\Plugins\`,
  not in the game folder and not loose in `Data`.
- **Start the game through the script extender** (`obse_loader.exe`,
  `nvse_loader.exe`, `fose_loader.exe`), or let your mod manager do it.
  Started the normal way, the extender doesn't load, and neither does the fix.

**Fallout 3 on Steam:** Steam ships version 1.7.0.4, and FOSE only runs on
1.7.0.3. Run the [Fallout Anniversary Patcher](https://www.nexusmods.com/fallout3/mods/24913)
first. After that you start `Fallout3.exe` as usual and it loads FOSE by
itself (no `fose_loader.exe` needed). GOG is already on 1.7.0.3.

## Still crashing?

Look in your game folder for `Sep2026LoadingCrashFix.log`.

- **No log file?** The plugin didn't load. Check the folder and how you
  start the game (see above).
- **Log says "AC3 decoder blocked"** but the game still crashes? Check Event
  Viewer: if the crash is *not* in `msmpeg2ac3dec.dll`, it's a different
  problem than this one.
- **On Linux?** This Windows bug can't hit you there, so your crash is
  something else.

## Rather not use a plugin?

You can also just uninstall the update: Settings > Windows Update > Update
history > Uninstall updates > KB5124010.

Catch is, Windows likes to put it right back. Turning off "Get the latest
updates as soon as they're available" in Windows Update helps, but it may
still come back with a later update. The plugin keeps working either way.

Or... maybe it's time to embrace Linux 🐧 No surprise Windows updates
breaking your old games over there.

## What's going on?

The update ships a broken Windows audio decoder (`msmpeg2ac3dec.dll`).
These games never actually use it, but Windows tries it out every time the
music changes, and on the second try it takes the game down with it.

The plugin simply tells Windows "skip that one" while the game is running.
Your music keeps playing through the normal MP3 decoder, same as before.
No game files or Windows files get touched.

It's a workaround for a Windows bug. Once Microsoft fixes it, the plugin just
sits there doing nothing, so no rush to remove it.

## Am I affected?

Check Windows Event Viewer after a crash. If it says your game crashed in
`msmpeg2ac3dec.dll` with `0xc0000602`, yep, that's this one.

## Heads up

- **Oblivion:** tested and working (Windows 11, build 26200.9550).
- **New Vegas:** tested and working too (crashed on a save load without the
  plugin, loaded and played fine with it).
- **Fallout 3:** tested and working too (crashed on entering a cell without
  the plugin, ran fine with it).

Works for you, or doesn't? Please open an issue with your game, your Windows
build (`winver`) and the `Sep2026LoadingCrashFix.log` from your game folder.
Thanks!

## Building it yourself

Needs Visual Studio with a 32-bit toolchain (run from a `vcvars32` prompt):

```
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release .
cmake --build build
ctest --test-dir build --output-on-failure
```

`RenderTest` needs an MP3 and uses Oblivion's `tes4title.mp3` by default
(`-DMUSIC_TEST_FILE=...` to change it). The details of the fix are in
`src/DecoderBlock.h`.
