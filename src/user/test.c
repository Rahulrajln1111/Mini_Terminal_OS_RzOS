
void user_program_test(void) {
    kputs("Hello from user-space!\n");
    // This infinite loop is a simple way to prevent a crash
    for (;;) {} 
}