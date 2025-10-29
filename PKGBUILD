# Maintainer: Aaron Bockelie <aaron@bockelie.com>
pkgname=obsbot-control
pkgver=0.2.0
pkgrel=1
pkgdesc="Qt-based control application for OBSBOT cameras (Meet 2, Tiny 2, Tiny 2 Lite)"
arch=('x86_64')
url="https://github.com/aaronsb/obsbot-controls-qt-linux"
license=('MIT')
depends=(
    'qt6-base'
    'qt6-multimedia'
    'glibc'
    'gcc-libs'
)
makedepends=(
    'cmake'
    'git'
)
optdepends=(
    'v4l2loopback-dkms: Virtual camera support'
    'lsof: Camera usage detection'
)
source=("git+https://github.com/aaronsb/obsbot-controls-qt-linux.git#tag=v${pkgver}")
sha256sums=('SKIP')

build() {
    cd "${srcdir}/${pkgname}-qt-linux"

    # Create build directory
    mkdir -p build
    cd build

    # Configure with CMake
    cmake .. \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr

    # Build
    make -j$(nproc)
}

package() {
    cd "${srcdir}/${pkgname}-qt-linux/build"

    # Install binaries
    install -Dm755 obsbot-gui "${pkgdir}/usr/bin/obsbot-gui"
    install -Dm755 obsbot-cli "${pkgdir}/usr/bin/obsbot-cli"

    # Install desktop file
    install -Dm644 "${srcdir}/${pkgname}-qt-linux/obsbot-control.desktop" \
        "${pkgdir}/usr/share/applications/obsbot-control.desktop"

    # Install icon
    install -Dm644 "${srcdir}/${pkgname}-qt-linux/resources/icons/camera.svg" \
        "${pkgdir}/usr/share/icons/hicolor/scalable/apps/obsbot-control.svg"

    # Install license
    install -Dm644 "${srcdir}/${pkgname}-qt-linux/LICENSE" \
        "${pkgdir}/usr/share/licenses/${pkgname}/LICENSE"

    # Install documentation
    install -Dm644 "${srcdir}/${pkgname}-qt-linux/README.md" \
        "${pkgdir}/usr/share/doc/${pkgname}/README.md"
}
