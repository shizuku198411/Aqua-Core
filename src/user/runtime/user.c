extern char __stack_top[];
extern int main(int argc, char **argv);

#include "process.h"
#include "user_syscall.h"

__attribute__((used))
int user_main_entry(void) {
    struct exec_args args;
    char *argv[PROC_EXEC_ARGV_MAX + 1];
    int argc = 0;

    if (getargs(&args) == 0) {
        argc = args.argc;
        if (argc < 0) {
            argc = 0;
        }
        if (argc > PROC_EXEC_ARGV_MAX) {
            argc = PROC_EXEC_ARGV_MAX;
        }
        for (int i = 0; i < argc; i++) {
            argv[i] = args.argv[i];
        }
    }
    argv[argc] = NULL;
    return main(argc, argv);
}

__attribute__((section(".text.start")))
__attribute__((naked))
void start(void) {
    __asm__ __volatile__(
        "mv sp, %[stack_top]\n"
        "call user_main_entry\n"
        "call exit\n"
        :
        : [stack_top] "r" (__stack_top)
    );
}
