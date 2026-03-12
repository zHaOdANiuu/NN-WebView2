.section .rodata, "a"
.globl __www_start
.globl __www_end

.balign 8
__www_start:
    .quad names
    .quad entry
    .quad datas
    .quad bytes
    .long 3
    .zero 4

names:
    .quad name1, name2, name3

entry:
    .asciz "index.html"

datas:
    .quad data1, data2, data3

bytes:
    .quad bytes1, bytes2, bytes3

name1: .asciz "index.css"
name2: .asciz "index.html"
name3: .asciz "index.js"

data1: .incbin "assets/index.css"
       .byte 0
bytes1 = . - data1 - 1

data2: .incbin "assets/index.html"
       .byte 0
bytes2 = . - data2 - 1

data3: .incbin "assets/index.js"
       .byte 0
bytes3 = . - data3 - 1

__www_end:
