pkgname=ttun-git
pkgver=1.0.0
pkgrel=1
pkgdesc="TTun - A simple Layer 3 UDP tunnel written in C"
arch=('x86_64' 'aarch64')
license=('GPL')
depends=('glibc' 'iproute2')
source=('ttun.c' 'Makefile' 'ttun.service' 'ttun.conf')
md5sums=('SKIP' 'SKIP' 'SKIP' 'SKIP')

build() {
    cd "$srcdir"
    make
}

package() {
    cd "$srcdir"
    
    # نصب فایل اجرایی
    make DESTDIR="$pkgdir" install
    
    # نصب سرویس systemd
    install -Dm644 ttun.service "$pkgdir/usr/lib/systemd/system/ttun.service"
    
    # نصب فایل تنظیمات
    install -Dm644 ttun.conf "$pkgdir/etc/ttun.conf"
}