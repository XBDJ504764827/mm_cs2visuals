# CS2 Visuals

CS2 Visuals is a Metamod:Source plugin for CS2 servers. It lets each player cycle through four client-side visibility levels on dark maps:

`default -> 1x -> 2x -> 3x -> default`

The plugin creates a Source 2 `post_processing_volume` for the selected player and filters its network transmission so other players never receive it. This changes the final rendered image through the post-processing pipeline instead of changing a server-side gamma value. Every connection and map change starts at the default level.

## Installation

1. Install Metamod:Source for CS2.
2. Download the latest release and extract it into the server's `game/csgo/` directory.
3. The package contains the plugin VDF, platform binary, gamedata, and `cfg/cs2visuals/cs2visuals.cfg`.

The release archives preserve the CS2 installation layout, so no files need to be moved after extraction.

Pull requests compile both Linux and Windows versions and upload installable preview ZIPs as workflow artifacts. Download those artifacts from the PR's **Actions** run to test the plugin. Preview builds never create tags or Releases; a tag and Release are created only after the PR is merged into `main`.

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
cs2visuals_postprocess_1 "lighting/postprocessing/cs2kr/nightvision/nv_soft.vpost"
cs2visuals_postprocess_2 "lighting/postprocessing/cs2kr/nightvision/nv_medium.vpost"
cs2visuals_postprocess_3 "lighting/postprocessing/cs2kr/nightvision/nv_strong.vpost"
```

The default paths point to the `.vpost` resources shipped by Workshop addon [3763782470](https://steamcommunity.com/sharedfiles/filedetails/?id=3763782470), which is the asset source used by [CS2KR-NightVision-SW2](https://github.com/CS2KR/CS2KR-NightVision-SW2). Install that addon on the server and clients, and use a resource precacher such as zResourcePrecacher/MultiAddonManager so clients have the compiled `.vpost_c` files. The server config accepts other `.vpost` paths if you provide your own post-processing assets. The plugin logs an initialization error if the Source 2 entity functions cannot be resolved.

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
