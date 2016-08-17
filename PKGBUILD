# Maintainer: Marcin Sielski <marcin.sielski@gmail.com>

_realname=simplehttpd
pkgbase=mingw-w64-${_realname}-git
pkgname="${MINGW_PACKAGE_PREFIX}-${_realname}-git"
pkgver=0.0.1
pkgrel=1
pkgdesc="Simple HTTP Server (git) (mingw-w64)"
arch=('any')
url="https://github.com/marcin-sielski/simplehttpd"
license=('LGPL')
makedepends=("${MINGW_PACKAGE_PREFIX}-gcc"
             'git')
depends=("${MINGW_PACKAGE_PREFIX}-libmicrohttpd"
         "${MINGW_PACKAGE_PREFIX}-glib2")
options=('strip' 'staticlibs')
source=("${_realname}"::"git+https://github.com/marcin-sielski/simplehttpd.git#branch=master")
sha256sums=('SKIP')

pkgver() {
  cd "${srcdir}"/${_realname}
  printf "%s.%s" "$(git rev-list --count HEAD)" "$(git rev-parse --short HEAD)"
}

build() {
  cd "$srcdir"/${_realname}
  autoreconf -i
  [[ -d "${srcdir}"/build-${CARCH} ]] && rm -rf "${srcdir}"/build-${CARCH}
  mkdir -p "${srcdir}"/build-${CARCH} && cd "${srcdir}"/build-${CARCH}
  ../${_realname}/configure \
    --prefix=${MINGW_PREFIX} \
    --build=${MINGW_CHOST} \
    --host=${MINGW_CHOST} \
    --target=${MINGW_CHOST}

  make
}

package() {
  cd "${srcdir}"/build-${CARCH}
  make DESTDIR=${pkgdir} install-strip
}
