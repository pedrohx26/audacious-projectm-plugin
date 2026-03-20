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
    'projectm-presets-classic-git: classic/original preset pack with many Geiss and Rovastar-era presets'
    'projectm-presets-cream-of-the-crop: curated preset pack (~10K presets)'
    'git: required for scripts/install-presets.sh helper when installing preset packs manually'
)

# For local builds from this directory:
source=()
sha256sums=()

build() {
    cd "$startdir"
    cmake -B build \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr
    cmake --build build --parallel "$(nproc)"
}

package() {
    cd "$startdir"
    DESTDIR="$pkgdir" cmake --install build

    install -Dm755 scripts/install-presets.sh "$pkgdir/usr/share/${pkgname}/scripts/install-presets.sh"
    install -Dm644 doc/preset-installation.md "$pkgdir/usr/share/doc/${pkgname}/preset-installation.md"
}
