.global paging_trampoline

.extern kernel_main

paging_trampoline:
    mov $0xC01FFE00, %esp

    ; # Jump to higher-half kernel_main
    ; # VA = KERNEL_HIGHBASE + (kernel_main - KERNEL_BASE)
    mov $0xC0100000, %eax  ; # replace 0xC0100000 with actual calculation
    jmp *%eax
