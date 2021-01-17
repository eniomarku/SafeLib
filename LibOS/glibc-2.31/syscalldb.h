#ifndef _SYSCALLDB_H_
#define _SYSCALLDB_H_

#ifdef __ASSEMBLER__
.weak syscalldb
.type syscalldb, @function

# if defined(PSEUDO) && defined(SYSCALL_NAME) && defined(SYSCALL_SYMBOL)
#  define SYSCALLDB                     \
    subq $128, %rsp;                    \
    .cfi_adjust_cfa_offset 128;         \
    callq *syscalldb@GOTPCREL(%rip);    \
    addq $128, %rsp;                    \
    .cfi_adjust_cfa_offset -128
# else
#  define SYSCALLDB                             \
    callq *syscalldb@GOTPCREL(%rip)
# endif

#else /* !__ASSEMBLER__ */
asm (
".weak syscalldb\r\n"
".type syscalldb, @function\r\n");

#define SYSCALLDB                           \
    "subq $128, %%rsp\n\t"                  \
    ".cfi_adjust_cfa_offset 128\n\t"        \
    "callq *syscalldb@GOTPCREL(%%rip)\n\t"  \
    "addq $128, %%rsp\n\t"                  \
    ".cfi_adjust_cfa_offset -128\n\t"

#define SYSCALLDB_ASM                       \
    "callq *syscalldb@GOTPCREL(%rip)\n\t"

#endif /* Assembler */

#endif /* _SYSCALLDB_H */
