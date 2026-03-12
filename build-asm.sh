#!/bin/bash

# ./build-asm.sh www "indx.html"

files=$(find $1 -type f)
names=""
namesAsciz=""
datas=""
datasIncbin=""
bytes=""

i=1
for file in $files; do
  names+="name$i, "
  namesAsciz+="name$i: .asciz \""${file#*/}"\"\n"
  datas+="data$i, "
  datasIncbin+="data$i: .incbin \"../$file\"\n       .byte 0\n"
  datasIncbin+="bytes$i = . - data$i - 1\n\n"
  bytes+="bytes$i, "
  ((++i))
done

names=${names::-2}
datas=${datas::-2}
datasIncbin=${datasIncbin::-2}
bytes=${bytes::-2}
((--i))

{
cat << EOF
.section .rodata, "a"
.globl __www_start
.globl __www_end

.balign 8
__www_start:
    .quad names
    .quad entry
    .quad datas
    .quad bytes
    .long $i
    .zero 4

names:
    .quad $names

entry:
    .asciz "$2"

datas:
    .quad $datas

bytes:
    .quad $bytes

EOF
echo -e $namesAsciz
echo -e "$datasIncbin"
echo "__www_end:"
} > build/embed.asm
