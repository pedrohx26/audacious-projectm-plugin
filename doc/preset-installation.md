# Preset Installation

This plugin renders projectM/Milkdrop presets, but the preset packs themselves are separate content.

## Recommended Layout

The repo now uses the standard user-local projectM directory layout:

```text
~/.local/share/projectM/
├── preset-packs/
│   ├── classic/
│   ├── cream-of-the-crop/
│   └── milkdrop-mega-pack/
├── presets/
│   ├── classic -> ../preset-packs/classic
│   ├── cream-of-the-crop -> ../preset-packs/cream-of-the-crop
│   ├── featured/
│   ├── milkdrop-mega-pack -> ../preset-packs/milkdrop-mega-pack
│   └── textures -> ../textures
└── textures -> ./sources/milkdrop-texture-pack/textures
```

The plugin already auto-detects `~/.local/share/projectM/presets`, so once these paths exist, Audacious can use them directly.

## Included Installer

Run:

```bash
./scripts/install-presets.sh
```

That installs or updates:

- `presets-projectm-classic`
- `presets-cream-of-the-crop`
- `presets-milkdrop-texture-pack`
- a `featured/` directory containing symlinks to specifically requested presets when found

## Featured Presets

The installer tries to locate and link these presets into `~/.local/share/projectM/presets/featured/`:

- Geiss - Feedback 2
- Geiss - Spiral
- Geiss - Zoom Pulse
- Rovastar - Fractal Spinning Plasma
- Rovastar - Hyperspace
- Krash - Beat Reaction
- Aderrasi - Electric Rose

These are linked from whichever installed pack contains the best matching preset filename.

### Viewing Featured Presets

Use the **Preset Browser** window to quickly access the featured collection:
1. *View → Visualization → Preset Browser*
2. Click the **📌 Featured** button
3. Browse and load the curated presets

## Using the Preset Browser

The plugin provides a standalone **Preset Browser** window for convenient preset discovery and loading:

**Features:**
- **Search across all directories** – type to filter presets by name
- **Featured collection** – one-click access to the curated 7-preset selection
- **Refresh** – rescan directories for newly added preset files
- **Load** – double-click or select + click Load to play a preset
- **Independent window** – floats or docks with other Audacious panels

**Access:** *View → Visualization → Preset Browser*

The browser automatically scans:
- `~/.local/share/projectM/presets/` (your installed packs)
- `/usr/share/projectM/presets/` (system-wide)
- `/usr/local/share/projectM/presets/` (fallback)

## MilkDrop 2 Mega Pack

The Mega Pack is intentionally not auto-downloaded by the installer or the PKGBUILD:

- it is very large
- the upstream source is not a normal stable release artifact
- AUR packaging should not fetch large ad-hoc content during install

After manually downloading it, integrate it with:

```bash
./scripts/install-presets.sh --mega-pack /path/to/extracted-folder-or-archive
```

That places it under `~/.local/share/projectM/preset-packs/milkdrop-mega-pack` and links it into the standard presets directory.

## AUR Strategy

For AUR packaging, the clean approach is:

- package the Audacious plugin separately
- expose preset packs as optional dependencies
- keep user-side preset installation in helper scripts or companion AUR packages

Relevant AUR packages already exist for:

- `projectm-presets-classic-git`
- `projectm-presets-cream-of-the-crop`

The texture pack and Mega Pack remain best handled outside the plugin PKGBUILD.

*Last updated: March 2026*