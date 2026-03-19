#!/bin/bash
# scripts/prepare-aur.sh
#
# Prepares the package for submission to AUR or the Arch [extra] repository.
#
# Usage:
#   ./scripts/prepare-aur.sh          # prepare for AUR
#   ./scripts/prepare-aur.sh --extra  # prepare for [extra] repo
#
set -euo pipefail
cd "$(dirname "$0")/.."

PKG="audacious-plugin-projectm"
VER="1.0.0"
TARGET="${1:-aur}"

GREEN='\033[0;32m'; YELLOW='\033[1;33m'; NC='\033[0m'
info() { echo -e "${GREEN}[INFO]${NC} $*"; }
warn() { echo -e "${YELLOW}[WARN]${NC} $*"; }

# ── 1. Create source tarball ─────────────────────────────────────────
info "Creating source tarball..."
TARBALL="${PKG}-${VER}.tar.gz"
git archive --format=tar.gz --prefix="${PKG}-${VER}/" HEAD -o "$TARBALL" 2>/dev/null || {
    warn "Not a git repo or git not available, creating tarball manually..."
    mkdir -p /tmp/${PKG}-${VER}
    cp -r src CMakeLists.txt PKGBUILD .SRCINFO README.md LICENSE doc/ /tmp/${PKG}-${VER}/ 2>/dev/null || true
    (cd /tmp && tar czf "$(pwd)/$TARBALL" ${PKG}-${VER}/)
    rm -rf /tmp/${PKG}-${VER}
}
info "Tarball: $TARBALL"

# ── 2. Generate PKGBUILD ────────────────────────────────────────────
info "Writing PKGBUILD..."
SHA256=$(sha256sum "$TARBALL" | awk '{print $1}')

cat > PKGBUILD << 'PKGBUILD_EOF'
# Maintainer: Your Name <your@email.com>
pkgname=audacious-plugin-projectm
pkgver=1.0.0
pkgrel=1
pkgdesc="ProjectM (Milkdrop) visualization plugin for Audacious – load, browse & edit presets"
arch=('x86_64')
url="https://github.com/YOURNAME/audacious-plugin-projectm"
license=('GPL2')
depends=(
    'audacious'
    'libprojectm'
    'qt6-base'
)
makedepends=(
    'cmake'
    'pkgconf'
    'mesa'
)
optdepends=(
    'projectm-presets-cream-of-the-crop: curated preset pack (~10K presets)'
)
source=("${pkgname}-${pkgver}.tar.gz")
sha256sums=('PLACEHOLDER')

build() {
    cd "${pkgname}-${pkgver}"
    cmake -B build \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr
    cmake --build build --parallel "$(nproc)"
}

package() {
    cd "${pkgname}-${pkgver}"
    DESTDIR="$pkgdir" cmake --install build
}
PKGBUILD_EOF

sed -i "s|PLACEHOLDER|${SHA256}|" PKGBUILD
info "PKGBUILD written."

# ── 3. Generate .SRCINFO ─────────────────────────────────────────────
if command -v makepkg &>/dev/null; then
    info "Generating .SRCINFO..."
    makepkg --printsrcinfo > .SRCINFO
    info ".SRCINFO generated."
else
    warn "makepkg not found – .SRCINFO not generated (run on Arch Linux)"
fi

# ── 4. Validate ──────────────────────────────────────────────────────
if command -v namcap &>/dev/null; then
    info "Running namcap on PKGBUILD..."
    namcap PKGBUILD || true
fi

# ── 5. Instructions ──────────────────────────────────────────────────
echo ""
if [[ "$TARGET" == "--extra" ]]; then
    info "=== Submission to [extra] repository ==="
    echo ""
    echo "  1. Fork https://gitlab.archlinux.org/archlinux/packaging/packages"
    echo "  2. Create a new package directory: ${PKG}"
    echo "  3. Copy PKGBUILD and .SRCINFO into it"
    echo "  4. Upload source tarball to a stable URL (GitHub Releases)"
    echo "  5. Update the source=() URL in PKGBUILD"
    echo "  6. Submit a merge request on GitLab"
    echo ""
    echo "  References:"
    echo "    https://wiki.archlinux.org/title/Arch_package_guidelines"
    echo "    https://wiki.archlinux.org/title/Creating_packages"
    echo ""
else
    info "=== Submission to AUR ==="
    echo ""
    echo "  1. Create an AUR account: https://aur.archlinux.org/register"
    echo "  2. Add your SSH key: https://aur.archlinux.org/account"
    echo ""
    echo "  3. Clone (first time):"
    echo "       git clone ssh://aur@aur.archlinux.org/${PKG}.git"
    echo "       cd ${PKG}"
    echo ""
    echo "  4. Copy files:"
    echo "       cp /path/to/PKGBUILD ."
    echo "       cp /path/to/.SRCINFO ."
    echo ""
    echo "  5. Upload source tarball to GitHub Releases and update source=()"
    echo ""
    echo "  6. Commit & push:"
    echo "       git add PKGBUILD .SRCINFO"
    echo "       git commit -m 'Initial upload: ${PKG} ${VER}'"
    echo "       git push"
    echo ""
    echo "  References:"
    echo "    https://wiki.archlinux.org/title/AUR_submission_guidelines"
    echo "    https://wiki.archlinux.org/title/PKGBUILD"
    echo ""
fi

# ── 6. AUR directory structure ───────────────────────────────────────
AUR_DIR="aur-package"
info "Creating AUR-ready directory: $AUR_DIR/"
mkdir -p "$AUR_DIR"
cp PKGBUILD "$AUR_DIR/"
[[ -f .SRCINFO ]] && cp .SRCINFO "$AUR_DIR/"
cp "$TARBALL" "$AUR_DIR/" 2>/dev/null || true

info "Files ready in $AUR_DIR/:"
ls -la "$AUR_DIR/"
