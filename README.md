# audacious-plugin-projectm

A visualization plugin for [Audacious](https://audacious-media-player.org/) that
renders **Milkdrop-compatible presets** (`.milk` / `.prjm`) via
[libprojectM 4.x](https://github.com/projectM-visualizer/projectm).

## Features

| Feature | Description |
| --- | --- |
| **Milkdrop rendering** | Full OpenGL rendering of Milkdrop 1 & 2 presets via libprojectM |
| **Audacious menu integration** | Menu entries under *View → Visualization* for all controls |
| **Standalone preset browser** | Independent dockable window with search, featured collection, and multi-directory scanning |
| **Preset variable editor** | Parse `.milk` files, edit all general variables (zoom, rot, decay, wave_*, …) and equation blocks (per-frame, per-pixel) live |
| **Real-time controls** | FPS, beat sensitivity, hard/soft cut durations |
| **Keyboard shortcuts** | N P R L E F – all from the vis window |
| **Settings persistence** | Audacious plugin settings and standard projectM preset directories |

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
├── src/
│   └── projectm-vis.cc           # Plugin source (single file)
├── scripts/
│   ├── build.sh                   # Build script
│   ├── install.sh                 # Install script (deps + build + install)
│   ├── install-presets.sh         # Preset pack installer / linker
│   └── prepare-aur.sh            # AUR / [extra] submission helper
└── doc/
    ├── milkdrop-variables.md      # Reference for .milk preset variables
    └── preset-installation.md     # Preset pack layout and installation strategy
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

## Quick Install

```bash
git clone https://github.com/YOURNAME/audacious-plugin-projectm.git
cd audacious-plugin-projectm
chmod +x scripts/*.sh
./scripts/install.sh

# Also install recommended preset packs:
./scripts/install.sh --with-presets
```

The install script uses `sudo -n` when available and otherwise falls back to `pkexec`.
Preset packs are installed per-user under `~/.local/share/projectM/`.

## Build Only

```bash
./scripts/build.sh          # Release build
./scripts/build.sh Debug    # Debug build
```

## PKGBUILD (makepkg)

```bash
makepkg -si
```

For AUR installs, preset packs should stay optional. Relevant companion AUR packages include:

- `projectm-presets-classic-git`
- `projectm-presets-cream-of-the-crop`

The package also installs a helper script and preset guide under `/usr/share/audacious-plugin-projectm/` and `/usr/share/doc/audacious-plugin-projectm/`.

## Getting Presets

```bash
# Install classic + Cream of the Crop + textures + featured links
./scripts/install-presets.sh

# Integrate a manually downloaded MilkDrop Mega Pack as well
./scripts/install-presets.sh --mega-pack /path/to/extracted-or-archive
```

This creates a standard projectM layout under `~/.local/share/projectM/` and links packs into `~/.local/share/projectM/presets`.

Or use the **Browse** button in the plugin to point to any directory.

## Recommended Packs

- **Classic projectM presets**: original pack with many of the older Geiss / Rovastar-era presets
- **Cream of the Crop**: curated top-tier collection and current upstream default
- **Milkdrop texture pack**: strongly recommended for texture-heavy presets
- **MilkDrop 2 Mega Pack**: supported as a manual import because the upstream source is a large external download

The helper script also tries to build a `featured` folder containing links to these requested presets when found:

- Geiss - Feedback 2
- Geiss - Spiral
- Geiss - Zoom Pulse
- Rovastar - Fractal Spinning Plasma
- Rovastar - Hyperspace
- Krash - Beat Reaction
- Aderrasi - Electric Rose

## Usage

### Setup
1. **Enable:** Settings → Plugins → Visualization → *ProjectM (Milkdrop)*
2. **Plugin settings:** in the same Plugins list, click the **Settings** button next to *ProjectM (Milkdrop)*.
    Configure preset directory, FPS, beat sensitivity, and hard/soft cut timing there.

### Controls
**Menu entries** appear under *View → Visualization*:
- **Next / Previous / Random Preset** – quick navigation
- **Lock / Unlock Preset** – hold current preset while visualizing
- **Edit Preset Variables…** – open variable editor for the current preset
- **Browse Preset Directory…** – file dialog to select a preset folder
- **Preset Browser** – ✨ **NEW** – open standalone dockable preset browser window
- **Toggle Fullscreen** – expand visualization to full screen

### Keyboard Shortcuts (visualization window must have focus):

| Key | Action |
| --- | --- |
| `N` / `→` | Next preset |
| `P` / `←` | Previous preset |
| `R` | Random preset |
| `L` | Lock / unlock |
| `E` | Open variable editor |
| `F` | Toggle fullscreen |

### Preset Browser Window

Open via **View → Visualization → Preset Browser**. This is a standalone dockable window that:

- **Searches** across multiple preset directories in real-time
- **Shows featured presets** – a curated collection of recommended presets
- **Refreshes** to detect new preset packs added to the file system
- **Loads presets** by double-clicking or selecting and pressing the Load button
- **Floats independently** or can be docked alongside other Audacious panels (Now Playing, Library, Playlist, etc.)

**Browser paths** (auto-scanned):
- `~/.local/share/projectM/presets/` (user-installed packs)
- `/usr/share/projectM/presets/` (system-wide)
- `/usr/local/share/projectM/presets/` (fallback)

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

**Specific classic presets missing?** Run `./scripts/install-presets.sh --all` to install the classic pack, Cream of the Crop, textures, and rebuild the featured preset links.

**libprojectM not found during build?** On Arch, install `libprojectm`, not `projectm`. Verify with `pkg-config --modversion libprojectM`; it must report 4.x.

**Crashes?** libprojectM 4.x is required. Version 3.x is not compatible.

## License

GPLv2+ · libprojectM is LGPL 2.1
