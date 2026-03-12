#!/bin/bash

flags="--target=x86_64-pc-windows-msvc\
       -std=c++20\
       -Wall\
       -Wpedantic\
       -fno-exceptions\
       -fno-rtti\
       -O3"

libs="-lkernel32 -luser32 -ladvapi32 -l../lib/WebView2LoaderStatic"

clang++ $flags $libs master.o embed.asm && ./a
