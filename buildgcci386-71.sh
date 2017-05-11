#!/bin/bash
top=`pwd`
instpath="$top/gcc-7.1.0-i386-cc-build"

wget -c ftp://ftp.gnu.org/pub/gnu/binutils/binutils-2.28.tar.bz2 ftp://ftp.gnu.org/pub/gnu/gcc/gcc-7.1.0/gcc-7.1.0.tar.bz2 || exit 1

if false; then #DEBUG

rm -Rf "$instpath" || exit 1
mkdir -p "$instpath" || exit 1

# binutils
cd "$top" || exit 1
rm -Rf "binutils-2.28" "binutils-2.28-build" || exit 1
tar -xjf "binutils-2.28.tar.bz2" || exit 1
mkdir -p "binutils-2.28-build" || exit 1
cd "binutils-2.28-build" || exit 1
../binutils-2.28/configure --prefix="$instpath" --target=i386-elf --enable-static --disable-shared || exit 1
make -j5 >/dev/null || exit 1
make install >/dev/null || exit 1
cd "$top" || exit 1

fi #DEBUG

# gcc
cd "$top" || exit 1
rm -Rf "gcc-7.1.0" "gcc-7.1.0-build" || exit 1
tar -xjf "gcc-7.1.0.tar.bz2" || exit 1
mkdir -p "gcc-7.1.0-build" || exit 1
cd "gcc-7.1.0-build" || exit 1
../gcc-7.1.0/configure --prefix="$instpath" --target=i386-elf --enable-static --disable-shared --disable-nls --enable-languages=c,c++ --without-headers --disable-libstdcxx --disable-libada --disable-libssp --disable-bootstrap --disable-objc-gc --disable-host-shared --disable-libquadmath || exit 1
make -j5 >/dev/null || exit 1
make all-gcc >/dev/null || exit 1
make all-target-libgcc >/dev/null || exit 1
make install-gcc >/dev/null || exit 1
make install-target-libgcc >/dev/null || exit 1
cd "$top" || exit 1


