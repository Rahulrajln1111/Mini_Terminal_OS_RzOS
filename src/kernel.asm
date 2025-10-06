[BITS 32]

global _start
extern paging_enable_and_jump
extern kernel_main
CODE_SEG equ 0x08
DATA_SEG equ 0x10

_start:
    mov ax,DATA_SEG
    mov ds,ax  
    mov es,ax 
    mov fs,ax 
    mov gs,ax  
    mov ss,ax 
    mov ebp,0x00200000
    mov esp,ebp

    ;Enabling A20 line
    in al, 0x92
    or al, 2
    out 0x92, al
    
    call paging_enable_and_jump
    jmp 0xC0100000 + (kernel_main - 0xC0100000)
    jmp $

times 512-($ - $$) db 0