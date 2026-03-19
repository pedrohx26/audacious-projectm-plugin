# audacious-plugin-projectm

A visualization plugin for [Audacious](https://audacious-media-player.org/) that
renders **Milkdrop-compatible presets** (`.milk` / `.prjm`) via
[libprojectM 4.x](https://github.com/projectM-visualizer/projectm).

## Features

| Feature | Description |
| --- | --- |
| **Milkdrop rendering** | Full OpenGL rendering of Milkdrop 1 & 2 presets via libprojectM |
| **Audacious menu integration** | Menu entries under *View → Visualization* for all controls |
| **Preset browser** | Searchable list, click to load, navigate with keyboard |
| **Preset variable editor** | Parse `.milk` files, edit all general variables (zoom, rot, decay, wave_*, …) and equation blocks (per-frame, per-pixel) live |
| **Real-time controls** | FPS, beat sensitivity, hard/soft cut durations |
| **Keyboard shortcuts** | N P R L E F – all from the vis window |
| **Settings persistence** | QSettings saves your config between sessions |

## Project Structure

```text
audacious-plugin-projectm/
├── CMakeLists.txt                 # Main build file
├── PKGBUILD                       # Arch Linux package build
├── LICENSE
├── README.md
├── .gitignore
├── .clang-format
├── .editorconfig
├── audacious-projectm.code-workspace   # VS Code workspace
├── .vscode/
│   ├── c_cpp_properties.json      # IntelliSense config
│   └── tasks.json                 # Build / install tasks
├── src/
│   └── projectm-vis.cc           # Plugin source (single file)
├── scripts/
│   ├── build.sh                   # Build script
│   ├── install.sh                 # Install script (deps + build + install)
│   └── prepare-aur.sh            # AUR / [extra] submission helper
└── doc/
    └── milkdrop-variables.md      # Reference for .milk preset variables
```

## Requirements

- **Arch Linux** (other distros need package name adjustments)
- `audacious` ≥ 4.0 with Qt interface
- `libprojectm` (libprojectM ≥ 4.0)
- `qt6-base`
- `mesa` (OpenGL)
- `cmake` ≥ 3.16, `pkgconf`

Install the Arch dependencies with:

```bash
sudo pacman -S --needed audacious libprojectm qt6-base mesa cmake pkgconf
```

If you are running from a VS Code terminal, prefer:

```bash
pkexec pacman -S --needed audacious libprojectm qt6-base mesa cmake pkgconf
```

## Quick Install

```bash
git clone https://github.com/YOURNAME/audacious-plugin-projectm.git
cd audacious-plugin-projectm
chmod +x scripts/*.sh
./scripts/install.sh
```

The install script uses `sudo -n` when available and otherwise falls back to `pkexec`.

## Build Only

```bash
./scripts/build.sh          # Release build
./scripts/build.sh Debug    # Debug build
```

## From VS Code

1. Open `audacious-projectm.code-workspace`
2. `Ctrl+Shift+B` → select **Build**
3. Or use Terminal → Run Task → **Install (sudo)**

## PKGBUILD (makepkg)

```bash
makepkg -si
```

## Getting Presets

```bash
# Cream of the Crop – ~10K curated presets
mkdir -p ~/.local/share/projectM
git clone https://github.com/projectM-visualizer/presets-cream-of-the-crop \
    ~/.local/share/projectM/presets

# Also grab the texture pack
git clone https://github.com/projectM-visualizer/presets-milkdrop-texture-pack \
    ~/.local/share/projectM/textures
```

Or use the **Browse** button in the plugin to point to any directory.

## Usage

1. **Enable:** Settings → Plugins → Visualization → *ProjectM (Milkdrop)*
2. **Menu entries** appear under *View → Visualization*:
   - Next / Previous / Random Preset
   - Lock / Unlock Preset
   - Edit Preset Variables…
   - Browse Preset Directory…
   - Toggle Fullscreen
3. **Keyboard shortcuts** (vis window must have focus):

| Key | Action |
| --- | --- |
| `N` / `→` | Next preset |
| `P` / `←` | Previous preset |
| `R` | Random preset |
| `L` | Lock / unlock |
| `E` | Open variable editor |
| `F` | Toggle fullscreen |

## Preset Variable Editor

Press **E** or use the menu to open the editor for the current preset.

- **Variables tab** – all general Milkdrop variables with their values, known ranges, and descriptions. Edit any value directly.
- **Per-Frame Init / Per-Frame / Per-Pixel tabs** – edit the equation code blocks.
- **Warp / Comp Shader tabs** – view the HLSL/GLSL shader code (read-only).
- **Apply & Reload** – saves to a temp file and hot-reloads in projectM.
- **Save** / **Save As** – write changes back to disk.

## AUR / EXTRA Submission

```bash
# Prepare for AUR:
./scripts/prepare-aur.sh

# Prepare for [extra]:
./scripts/prepare-aur.sh --extra
```

The script creates a tarball, generates the PKGBUILD with correct checksums,
runs `makepkg --printsrcinfo`, validates with `namcap`, and prints step-by-step
instructions for submission.

## Troubleshooting

**Plugin not visible?** Make sure you're using Audacious with the Qt interface (not GTK). Check `pkg-config --variable=plugin_dir audacious` for the install path.

**Black screen?** Select a preset directory with `.milk` files. Some presets need the texture pack.

**libprojectM not found during build?** On Arch, install `libprojectm`, not `projectm`. Verify with `pkg-config --modversion libprojectM`; it must report 4.x.

**Crashes?** libprojectM 4.x is required. Version 3.x is not compatible.

## License

GPLv2+ · libprojectM is LGPL 2.1
