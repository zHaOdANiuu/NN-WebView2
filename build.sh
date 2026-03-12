#!/bin/bash

flags="--target=x86_64-pc-windows-msvc\
       -std=c++20\
       -Wall\
       -Wpedantic\
       -I./lib"
libs="-lkernel32 -luser32 -ladvapi32 -l./lib/WebView2LoaderStatic"

if [ "$DEBUG" = "y" ]; then
  flags+=" -DDEBUG -O0 -g"
else
  flags+=" -fno-exceptions -fno-rtti -O3"
fi

if [ ! -d "build" ]; then mkdir build; fi
if [ "lib/stdafx.hpp" -nt "build/stdafx.pchpp" ] || [ ! -f "build/stdafx.pchpp" ]; then
  clang++ $flags lib/stdafx.hpp -o build/stdafx.pchpp
fi

if [ "master.cpp" -nt "build/master.o" ] || [ ! -f " build/master.o" ]; then
  clang++ $flags -include-pch build/stdafx.pchpp -c master.cpp -o build/master.o
fi

./build-asm.sh www "index.html" && clang++ $flags $libs build/master.o build/embed.asm -o build/a.exe
