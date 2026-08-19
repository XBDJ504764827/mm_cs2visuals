# CS2 Visuals

CS2 Visuals is a Metamod:Source plugin for CS2 servers. It lets each player cycle through four client-side visibility levels on dark maps:

`default -> 1x -> 2x -> 3x -> default`

The plugin sends `r_fullscreen_gamma` only to the player who changed the setting. Other players are not affected. Every connection and map change starts at the default level.

## Installation

1. Install Metamod:Source for CS2.
2. Download the latest release and extract it into the server's `game/csgo/` directory.
3. The package contains the plugin VDF, platform binary, and `cfg/cs2visuals/cs2visuals.cfg`.

The release archives preserve the CS2 installation layout, so no files need to be moved after extraction.

## Controls

When a player joins, the plugin binds G to `cs2visuals_cycle` for that player. This avoids relying on the original `drop` command, which KZ plugins commonly repurpose. The plugin also accepts `drop` as a compatibility fallback. To bind it manually:

```cfg
bind g cs2visuals_cycle
```

Players can also bind another key or use the command directly:

```cfg
bind h cs2visuals_cycle
```

Each change prints the current level to that player only.

## Configuration

Edit `cfg/cs2visuals/cs2visuals.cfg`:

```cfg
cs2visuals_enabled 1
cs2visuals_gamma_default 2.2
cs2visuals_gamma_1 2.0
cs2visuals_gamma_2 1.8
cs2visuals_gamma_3 1.6
```

Lower gamma values make dark areas brighter. The values are clamped to the safe range `1.0` to `3.0`; the server can reload them with `exec cs2visuals/cs2visuals.cfg`.

## Build

The project follows the AMBuild layout used by `mm_misc_plugins` and `mm-cs2rockthevote`.

Prerequisites:

- Python 3
- AMBuild 2.2 or newer
- Metamod:Source source tree
- CS2 HL2SDK (`cs2` branch)
- A C++17 compiler (Clang/GCC or MSVC)

```bash
git clone --recurse-submodules https://github.com/XBDJ504764827/mm_cs2visuals.git
cd mm_cs2visuals
mkdir build && cd build
python3 ../configure.py --enable-optimize
ambuild
```

The installable package is written to `build/package/`.

## License

MIT. Copyright 2026 XBDJ504764827.
