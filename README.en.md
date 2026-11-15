# Legacy Game Tools

[日本語](README.md) · English

A Vector compatibility module addressing startup crashes in old games on current Android versions.

## First unlock after boot

Hook initialization that needs settings or cached discovery results waits for the first user unlock after boot. A later ordinary screen lock does not repeat initialization. Existing screen-off power-saving and security behavior is preserved. Unlock delivery is event-driven, with no polling or new resident service.

## Supported games

| Game | Verified version | Package |
| --- | --- | --- |
| D4DJ D4U demo | 1.2.1 / code 8 | `com.bushiroad.d4djddedition` |
| SEKAI demo | 1.0.0 / code 200005 | `com.sega.pjsekai.re` |

The module supports specific arm64 Unity-engine builds. It does not apply to unrelated apps or different engine versions merely sharing a game name.

## Setup

1. Install the signed release APK from [GitHub Releases](https://github.com/OneUI-Mods/legacy-game-tools/releases).
2. Enable it in an Xposed API 102 framework such as Vector on a rooted device.
3. Select the two games above as targets. SystemUI and the system framework are not required.
4. Enable compatibility for the game in this app, close the game, and reopen it.

Existing game APKs and saved data do not need replacement. Settings are separate per game and apply on its next process start. No device reboot is required.

## What is corrected

The module compensates for old Unity memory-tracking code that cannot handle current Android memory layouts and crashes immediately after startup. Actual allocation and deallocation remain with the game.

Both demos' online services have ended. This app does not restore servers or services and does not distribute game data. It does not guarantee progress beyond network waits or that the entire game is playable.

Physical testing on Android 17 / Galaxy S26 Ultra reached D4U's network-wait screen and SEKAI's post-startup notice screen. Installation is supported on Android 12+, but other devices and OS versions are unverified.

## SEKAI BPM definitions

Version 0.2.0-rc1 adds compatibility for reading two-character alphanumeric BPM IDs in the supported SEKAI demo. It follows SEKAI's compatibility switch and applies on the next game start. BPM values, note timing, and audio are unchanged; other regular expressions and D4U are unaffected. Loading was verified in the supported demo, but compatibility with every chart or audio asset is not guaranteed.

## Disabling and removal

Disable the game's switch or the Vector module and restart the game to restore original behavior. Game data does not need to be deleted.

## Distribution and licenses

- Check updates through [OneUI Mods Manager](https://github.com/OneUI-Mods/OneUIModsManager), with prereleases enabled.
- In-app **Open-source licenses** groups notices by component.
- The project uses the MIT License. It contains no game binaries, images, audio, or Unity libraries.
- This unofficial compatibility module is unaffiliated with the games' rights holders.

See [developer documentation (Japanese)](docs/development.md) for analysis and builds, and [release instructions (Japanese)](docs/releasing.md) for signing and distribution.

## SEKAI demo life

The SEKAI settings include **Prevent life loss during live play**, ON by default. Miss judgments, scores, combos, and recovery remain unchanged. It applies when startup compatibility is enabled; close and reopen the game after changing it. Only the supported 1.0.0 demo is targeted. This is experimental, with a complete live-play hardware check still pending. See the [implementation scope (Japanese)](docs/sekai-life.md).

## Title and live-start waits

**Tap to skip title call** and **Shorten the white live-start transition** are independently switchable and ON by default. Loading, authentication, and song/note synchronization are preserved while title-voice waiting and the white effect are shortened. Close and reopen the game after changes. Android 17 hardware testing confirmed tapping during a playing title call to reach the live menu and progressing into a live session after the white transition. Stock fade duration and particles are retained. Targets and validation scope are documented in the [developer notes (Japanese)](docs/sekai-flow.md).

For solo play, the six-second fixed wait also ends early only after audio preparation, the fade, and the stock transition display are confirmed complete. If completion cannot be confirmed, the module retains the original-equivalent wait. This is controlled by **Shorten the white live-start transition** as well.
