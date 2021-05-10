@ECHO OFF

mkdir app
echo * >app/.gitignore

SET CC=clang-cl
SET CXX=clang-cl
SET WINDRES=rc

meson setup --buildtype=release --prefix=%cd%\app --bindir=. --unity on builddir
meson compile -C builddir
meson install -C builddir