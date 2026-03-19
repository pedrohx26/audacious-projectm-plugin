# Milkdrop Preset Variables Reference

Quick reference for the most common variables found in `.milk` preset files.
These can be edited in the plugin's **Preset Variable Editor** (press E).

## General / Per-Frame Variables

### Motion & Warping

| Variable | Default | Range | Description |
|----------|---------|-------|-------------|
| `zoom` | 1.0 | 0.5–2.0 | Zoom per frame (>1 = zoom in) |
| `rot` | 0.0 | -π..π | Rotation per frame (radians) |
| `warp` | 0.0 | 0–2.0 | Warp distortion amount |
| `cx` | 0.5 | 0–1 | Center of rotation X |
| `cy` | 0.5 | 0–1 | Center of rotation Y |
| `dx` | 0.0 | -0.1–0.1 | Horizontal pan per frame |
| `dy` | 0.0 | -0.1–0.1 | Vertical pan per frame |
| `sx` | 1.0 | 0.9–1.1 | Horizontal stretch |
| `sy` | 1.0 | 0.9–1.1 | Vertical stretch |

### Colour & Decay

| Variable | Default | Range | Description |
|----------|---------|-------|-------------|
| `decay` / `fDecay` | 0.98 | 0.9–1.0 | How quickly old frames fade |
| `gamma` / `fGammaAdj` | 1.0 | 0.5–5.0 | Gamma correction |

### Waveform

| Variable | Default | Range | Description |
|----------|---------|-------|-------------|
| `wave_mode` / `nWaveMode` | 0 | 0–7 | Waveform display style |
| `wave_a` | 1.0 | 0–1 | Waveform opacity |
| `wave_r` / `wave_g` / `wave_b` | varies | 0–1 | Waveform colour |
| `wave_x` / `wave_y` | 0.5 | 0–1 | Waveform position |
| `wave_scale` | 1.0 | 0.01–10 | Waveform size |
| `wave_smoothing` | 0.75 | 0–1 | Waveform temporal smoothing |

### Borders

| Variable | Range | Description |
|----------|-------|-------------|
| `ob_size` / `ib_size` | 0–0.5 | Outer / inner border thickness |
| `ob_r/g/b/a` / `ib_r/g/b/a` | 0–1 | Border colour & opacity |

### Effects (boolean flags: 0 or 1)

| Variable | Description |
|----------|-------------|
| `bAdditiveWaves` | Additive blending for waveforms |
| `bWaveDots` | Draw waveform as dots |
| `bWaveThick` | Thick waveform lines |
| `bDarkenCenter` | Darken the center of the screen |
| `bBrighten` | Brighten post-effect |
| `bDarken` | Darken post-effect |
| `bSolarize` | Solarize post-effect |
| `bInvert` | Invert colours |
| `bTexWrap` | Texture coordinate wrapping |
| `bRedBlueStereo` | Red/blue stereo effect |

### Video Echo

| Variable | Range | Description |
|----------|-------|-------------|
| `echo_zoom` / `fVideoEchoZoom` | 0.5–2 | Echo zoom level |
| `echo_alpha` / `fVideoEchoAlpha` | 0–1 | Echo opacity |
| `echo_orient` / `nVideoEchoOrientation` | 0–3 | Echo flip orientation |

### Motion Vectors

| Variable | Description |
|----------|-------------|
| `mv_x` / `mv_y` | Number of motion vectors (grid) |
| `mv_dx` / `mv_dy` | Motion vector offset |
| `mv_l` | Motion vector length |
| `mv_r/g/b/a` | Motion vector colour |

## Q Variables (q1–q32)

Used to pass values between equation blocks:
- **preset init** → sets base values
- **per-frame** → can read/modify; values pass to per-pixel and shaders
- Values reset each frame to init values

## Read-Only Variables (available in all blocks)

| Variable | Description |
|----------|-------------|
| `time` | Seconds since start |
| `fps` | Current frame rate |
| `frame` | Frame counter |
| `progress` | 0→1 progress through current preset duration |
| `bass` / `mid` / `treb` | Current frequency band levels (~1.0 normal) |
| `bass_att` / `mid_att` / `treb_att` | Attenuated (smoothed) versions |
| `meshx` / `meshy` | Mesh grid dimensions |
| `aspectx` / `aspecty` | Aspect ratio corrections |

## Custom Shapes (1–4) and Waves (1–4)

Each has its own init + per-frame equations. Variables include:
`sides`, `x`, `y`, `rad`, `ang`, `r/g/b/a`, `r2/g2/b2/a2`, `tex_zoom`, `tex_ang`, etc.

For the full specification, see:
https://www.geisswerks.com/milkdrop/milkdrop_preset_authoring.html
