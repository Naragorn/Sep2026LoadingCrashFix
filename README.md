# Sep 2026 Loading Crash Fix (Windows 11 KB5124010)

Oblivion crashing right after the main menu, just before the loading screen?
If you're on Windows 11 and recently got update **KB5124010**, this little
plugin is for you.

**[⬇ Download the latest release](https://github.com/Naragorn/Sep2026LoadingCrashFix/releases/latest)**

## Install

1. You need [xOBSE](https://github.com/llde/xOBSE).
2. Install the zip with your mod manager, or copy its `OBSE` folder into
   your `Oblivion\Data` folder (so the DLL ends up in `Data\OBSE\Plugins\`).
3. Play. That's it.

To remove it, just delete the DLL.

## Rather not use a plugin?

You can also just uninstall the update: Settings > Windows Update > Update
history > Uninstall updates > KB5124010.

Catch is, Windows likes to put it right back. Turning off "Get the latest
updates as soon as they're available" in Windows Update helps, but it may
still come back with a later update. The plugin keeps working either way.

## What's going on?

The update ships a broken Windows audio decoder (`msmpeg2ac3dec.dll`).
Oblivion never actually uses it, but Windows tries it out every time the
music changes, and on the second try it takes the game down with it.

The plugin simply tells Windows "skip that one" while Oblivion is running.
Your music keeps playing through the normal MP3 decoder, same as before.
No game files or Windows files get touched.

It's a workaround for a Windows bug. Once Microsoft fixes it, you can
delete the plugin.

## Am I affected?

Check Windows Event Viewer after a crash. If it says `Oblivion.exe` crashed in
`msmpeg2ac3dec.dll` with `0xc0000602`, yep, that's this one.

## Heads up

So far this is tested on one PC (Windows 11, build 26200.9550). If it works
or doesn't work for you, please open an issue and include your Windows build
(`winver`) and the `Sep2026LoadingCrashFix.log` from your Oblivion folder. Thanks!

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
