#!/bin/bash

# THIS SHOULD BE RUN AFTER 
# arch -x86_64 zsh  # optional: enter x86_64 Rosetta shell first
# conda activate pretext

buildtype="debug"  # release debug, 

# -------------Compile environment----------------
# add the path of cmake to the environment path
export PATH="/opt/homebrew/bin:$PATH"
# -----------------------------


[ -d "app" ] && rm -rf "app"  # remove existing app output directory
[ -d "builddir" ] && rm -rf "builddir"  # remove existing build directory
mkdir -p app
mkdir -p builddir
cat > "app/.gitignore" <<END
*
Build type: ${buildtype}
cmake --version
Host_machine_name: $(uname) 
END


if [ `uname` == Darwin ]; then  # if this is build on mac m-chip (Darwin)
  [ -d app/PretextView.app ] && rm -rf app/PretextView.app
  # arch -x86_64 zsh
  # env CC=clang CXX=clang MACOSX_DEPLOYMENT_TARGET=10.10 meson setup --buildtype=${buildtype} --prefix=$(pwd)/app/PretextView.app --bindir=Contents/MacOS --unity on builddir
  env CC=clang CXX=clang MACOSX_DEPLOYMENT_TARGET=14.3 meson setup --buildtype=${buildtype} --prefix=$(pwd)/app/PretextView.app --bindir=Contents/MacOS --unity on builddir
else
  env CC=clang CXX=clang meson setup --buildtype=${buildtype} --prefix=$(pwd)/app --bindir=./ --unity on builddir
fi

meson compile -C builddir 
meson install -C builddir

