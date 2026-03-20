#!/bin/bash
# scripts/install-presets.sh - Install recommended projectM preset packs locally
set -euo pipefail

PROJECTM_HOME="${XDG_DATA_HOME:-$HOME/.local/share}/projectM"
SOURCE_ROOT="$PROJECTM_HOME/sources"
PRESET_ROOT="$PROJECTM_HOME/presets"
PACK_ROOT="$PROJECTM_HOME/preset-packs"
TEXTURE_LINK="$PROJECTM_HOME/textures"
FEATURED_ROOT="$PRESET_ROOT/featured"
CLASSIC_DIR="$PACK_ROOT/classic"
CREAM_DIR="$PACK_ROOT/cream-of-the-crop"
TEXTURE_REPO_DIR="$SOURCE_ROOT/milkdrop-texture-pack"

CLASSIC_REPO_URL="https://github.com/projectM-visualizer/presets-projectm-classic.git"
CREAM_REPO_URL="https://github.com/projectM-visualizer/presets-cream-of-the-crop.git"
TEXTURE_REPO_URL="https://github.com/projectM-visualizer/presets-milkdrop-texture-pack.git"

GREEN='\033[0;32m'; YELLOW='\033[1;33m'; RED='\033[0;31m'; NC='\033[0m'
info()  { echo -e "${GREEN}[INFO]${NC} $*"; }
warn()  { echo -e "${YELLOW}[WARN]${NC} $*"; }
error() { echo -e "${RED}[ERROR]${NC} $*" >&2; exit 1; }

usage() {
    cat <<'EOF'
Usage: ./scripts/install-presets.sh [options]

Installs recommended preset packs into the standard user-local projectM layout:
  ~/.local/share/projectM/

Default behavior installs:
  - classic projectM preset pack
  - Cream of the Crop pack
  - Milkdrop texture pack
  - featured symlink collection for selected presets

Options:
  --classic              Install/update the classic preset pack only
  --cream                Install/update the Cream of the Crop pack only
  --textures             Install/update the Milkdrop texture pack only
  --featured             Rebuild the featured preset collection only
  --mega-pack PATH       Integrate a locally downloaded Mega Pack directory or archive
  --all                  Install classic + cream + textures + featured (default)
  --help                 Show this help text

Notes:
  - Presets are installed per-user; no root privileges are required.
  - The plugin auto-detects ~/.local/share/projectM/presets.
  - The Mega Pack is not auto-downloaded because there is no stable redistributable source.
EOF
}

need_cmd() {
    command -v "$1" >/dev/null 2>&1 || error "Required command not found: $1"
}

ensure_dirs() {
    mkdir -p "$SOURCE_ROOT" "$PACK_ROOT" "$PRESET_ROOT" "$FEATURED_ROOT"
}

clone_or_update_repo() {
    local repo_url="$1"
    local dest_dir="$2"

    need_cmd git

    if [[ -d "$dest_dir/.git" ]]; then
        info "Updating $(basename "$dest_dir")..."
        git -C "$dest_dir" pull --ff-only
    elif [[ -e "$dest_dir" ]]; then
        error "Path exists and is not a git checkout: $dest_dir"
    else
        info "Cloning $(basename "$dest_dir")..."
        git clone --depth 1 "$repo_url" "$dest_dir"
    fi
}

safe_symlink() {
    local source_path="$1"
    local link_path="$2"

    if [[ -L "$link_path" ]]; then
        ln -sfn "$source_path" "$link_path"
    elif [[ -e "$link_path" ]]; then
        warn "Skipping existing non-symlink path: $link_path"
    else
        ln -s "$source_path" "$link_path"
    fi
}

install_classic_pack() {
    clone_or_update_repo "$CLASSIC_REPO_URL" "$CLASSIC_DIR"
    safe_symlink "$CLASSIC_DIR" "$PRESET_ROOT/classic"
}

install_cream_pack() {
    clone_or_update_repo "$CREAM_REPO_URL" "$CREAM_DIR"
    safe_symlink "$CREAM_DIR" "$PRESET_ROOT/cream-of-the-crop"
}

install_texture_pack() {
    clone_or_update_repo "$TEXTURE_REPO_URL" "$TEXTURE_REPO_DIR"
    safe_symlink "$TEXTURE_REPO_DIR/textures" "$TEXTURE_LINK"
    safe_symlink "$TEXTURE_REPO_DIR/textures" "$PRESET_ROOT/textures"
}

integrate_mega_pack() {
    local source_path="$1"
    local dest_dir="$PACK_ROOT/milkdrop-mega-pack"

    [[ -n "$source_path" ]] || error "--mega-pack requires a path"
    [[ -e "$source_path" ]] || error "Mega Pack source not found: $source_path"

    rm -rf "$dest_dir"
    mkdir -p "$dest_dir"

    if [[ -d "$source_path" ]]; then
        info "Copying Mega Pack directory..."
        cp -a "$source_path"/. "$dest_dir/"
    else
        if command -v bsdtar >/dev/null 2>&1; then
            info "Extracting Mega Pack archive with bsdtar..."
            bsdtar -xf "$source_path" -C "$dest_dir"
        elif command -v unzip >/dev/null 2>&1; then
            info "Extracting Mega Pack archive with unzip..."
            unzip -q "$source_path" -d "$dest_dir"
        else
            error "Need bsdtar or unzip to extract Mega Pack archive"
        fi
    fi

    safe_symlink "$dest_dir" "$PRESET_ROOT/milkdrop-mega-pack"
}

find_first_preset() {
    local pattern="$1"
    shift
    find "$@" -type f \( -iname '*.milk' -o -iname '*.prjm' \) -iname "$pattern" -print -quit 2>/dev/null
}

link_featured_presets() {
    mkdir -p "$FEATURED_ROOT"

    local search_roots=()
    [[ -d "$CLASSIC_DIR" ]] && search_roots+=("$CLASSIC_DIR")
    [[ -d "$CREAM_DIR" ]] && search_roots+=("$CREAM_DIR")
    [[ -d "$PACK_ROOT/milkdrop-mega-pack" ]] && search_roots+=("$PACK_ROOT/milkdrop-mega-pack")

    if (( ${#search_roots[@]} == 0 )); then
        warn "No preset packs installed yet; featured collection not rebuilt."
        return
    fi

    rm -f "$FEATURED_ROOT"/*.milk "$FEATURED_ROOT"/*.prjm 2>/dev/null || true

    local specs=(
        'Geiss - Feedback 2|*geiss*feedback*2*.milk|*geiss*feedback*.milk'
        'Geiss - Spiral|*geiss*spiral*.milk'
        'Geiss - Zoom Pulse|*geiss*zoom*pulse*.milk|*geiss*zoom*.milk'
        'Rovastar - Fractal Spinning Plasma|*rovastar*fractal*spinning*plasma*.milk|*rovastar*plasma*.milk'
        'Rovastar - Hyperspace|*rovastar*hyperspace*.milk'
        'Krash - Beat Reaction|*krash*beat*reaction*.milk|*krash*beat*.milk'
        'Aderrasi - Electric Rose|*aderrasi*electric*rose*.milk|*electric*rose*.milk'
    )

    local linked=0
    local spec title match pattern
    for spec in "${specs[@]}"; do
        IFS='|' read -r -a fields <<< "$spec"
        title="${fields[0]}"
        match=""

        for pattern in "${fields[@]:1}"; do
            match="$(find_first_preset "$pattern" "${search_roots[@]}")"
            [[ -n "$match" ]] && break
        done

        if [[ -n "$match" ]]; then
            safe_symlink "$match" "$FEATURED_ROOT/$(basename "$match")"
            info "Featured preset linked: $title"
            linked=$((linked + 1))
        else
            warn "Featured preset not found in installed packs: $title"
        fi
    done

    info "Featured collection rebuilt: $linked preset links in $FEATURED_ROOT"
}

install_classic=false
install_cream=false
install_textures=false
install_featured=false
mega_pack_path=""

if (( $# == 0 )); then
    install_classic=true
    install_cream=true
    install_textures=true
    install_featured=true
fi

while (( $# > 0 )); do
    case "$1" in
        --classic)
            install_classic=true
            ;;
        --cream)
            install_cream=true
            ;;
        --textures)
            install_textures=true
            ;;
        --featured)
            install_featured=true
            ;;
        --mega-pack)
            shift
            mega_pack_path="${1:-}"
            [[ -n "$mega_pack_path" ]] || error "--mega-pack requires a path"
            ;;
        --all)
            install_classic=true
            install_cream=true
            install_textures=true
            install_featured=true
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        *)
            error "Unknown option: $1"
            ;;
    esac
    shift
done

ensure_dirs

$install_classic && install_classic_pack
$install_cream && install_cream_pack
$install_textures && install_texture_pack
[[ -n "$mega_pack_path" ]] && integrate_mega_pack "$mega_pack_path"
$install_featured && link_featured_presets

echo ""
info "Preset root ready: $PRESET_ROOT"
info "Texture root ready: $TEXTURE_LINK"
info "Set the plugin preset directory to: $PRESET_ROOT"

if [[ -n "$mega_pack_path" ]]; then
    info "Mega Pack integrated under: $PRESET_ROOT/milkdrop-mega-pack"
else
    warn "MilkDrop 2 Mega Pack was not auto-downloaded."
    warn "Download it manually, then run: ./scripts/install-presets.sh --mega-pack /path/to/extracted-or-archive"
fi