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
}
