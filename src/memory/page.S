[BITS 32]

section .asm

global paging_load_dir
global enable_paging
global enable_paging_continue

paging_load_dir:
    push ebp
    mov ebp,esp
    mov eax,[ebp+8]
    mov cr3,eax
    pop ebp
    ret

enable_paging:
    mov eax,cr0
    or eax,0x80000000
    mov cr0,eax

    ; This is the corrected long jump for NASM.
    ; It jumps to the 'enable_paging_continue' label using the code selector 0x08.
    jmp enable_paging_continue

enable_paging_continue:
    ret