# Make sure you opened the x64 Native Tools Command Prompt for VS 2022 and then opened MSYS2 shell command
echo Getting libvpx...
git clone --depth 1 https://chromium.googlesource.com/webm/libvpx.git
cd ../build
mkdir libvpx
cd libvpx
../../sources/libvpx/configure --target=x86_64-win64-vs17 --enable-static --enable-static-msvcrt --disable-examples --disable-tools --disable-docs --disable-unit-tests --prefix="../../installed/libvpx"
make
make install
mv ../../installed/libvpx/lib/x64/vpxmt.lib ../../installed/libvpx/lib/x64/vpx.lib
cd ../../sources