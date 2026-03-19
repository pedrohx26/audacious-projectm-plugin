#!/bin/bash
# scripts/install.sh – Install the plugin (build first if needed)
set -euo pipefail
cd "$(dirname "$0")/.."

GREEN='\033[0;32m'; YELLOW='\033[1;33m'; RED='\033[0;31m'; NC='\033[0m'
info()  { echo -e "${GREEN}[INFO]${NC} $*"; }
warn()  { echo -e "${YELLOW}[WARN]${NC} $*"; }
error() { echo -e "${RED}[ERROR]${NC} $*" >&2; exit 1; }

install_with_privileges() {
    if sudo -n true 2>/dev/null; then
        sudo -n "$@"
    elif command -v pkexec >/dev/null 2>&1; then
        pkexec "$@"
    else
        error "Need root privileges. Install pkexec or configure passwordless sudo for this command."
    fi
}

# ── 1. Dependencies ─────────────────────────────────────────────────
info "Checking dependencies..."
DEPS=(audacious audacious-plugins libprojectm qt6-base mesa cmake pkgconf)
MISSING=()
for d in "${DEPS[@]}"; do
    pacman -Qi "$d" &>/dev/null || MISSING+=("$d")
done
if (( ${#MISSING[@]} )); then
    info "Installing: ${MISSING[*]}"
    install_with_privileges pacman -S --needed --noconfirm "${MISSING[@]}"
fi

# ── 2. Build ─────────────────────────────────────────────────────────
if [[ ! -f build/projectm-vis.so ]]; then
    info "Building..."
    bash scripts/build.sh Release
fi

# ── 3. Install ───────────────────────────────────────────────────────
info "Installing plugin..."
install_with_privileges cmake --install build

PLUGIN_DIR=$(pkg-config --variable=plugin_dir audacious)
if [[ -f "$PLUGIN_DIR/Visualization/projectm-vis.so" ]]; then
    info "Installed: $PLUGIN_DIR/Visualization/projectm-vis.so"
else
    error "Installation failed!"
fi

# ── 4. Check for presets ─────────────────────────────────────────────
for p in /usr/share/projectM/presets /usr/local/share/projectM/presets \
         /usr/share/projectm-presets ~/.projectM/presets \
         ~/.local/share/projectM/presets; do
    if [[ -d "$p" ]] && ls "$p"/*.milk &>/dev/null 2>&1; then
        info "Presets found at: $p"
        FOUND=1; break
    fi
done
if [[ -z "${FOUND:-}" ]]; then
    warn "No presets found. Download some:"
    echo "  mkdir -p ~/.local/share/projectM/presets"
    echo "  git clone https://github.com/projectM-visualizer/presets-cream-of-the-crop ~/.local/share/projectM/presets"
fi

echo ""
info "Done! Enable the plugin in Audacious:"
info "  Settings → Plugins → Visualization → ProjectM (Milkdrop)"
info ""
info "Menu entries appear under: View → Visualization"
