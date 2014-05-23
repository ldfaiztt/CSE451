#ifndef STHREAD_SWITCH_I386
#define STHREAD_SWITCH_I386

/* This is the i386 assembly for the actual register saver/stack switcher.
 *
 * void Xsthread_switch(char **old_sp, char *new_sp)
 *   Save the currently running thread's context on its stack, switch to
 *   the new thread by swapping in its stack pointer, then pop that thread's
 *   context off of the stack and return. The state that is stored on the
 *   stack includes all of the general-purpose registers.
 *
 * We put this code in a .S file, instead of using the gcc 'asm (...)' syntax,
 * to make it more robust (this way, the compiler won't change _anything_, and
 * we know exactly what the stack will look like. 
 * 
 * Also note that, by calling this sthread_switch.S rather than .s, this file
 * will be run through the C pre-processor before being fed to the assembler. */

#include <config.h>

/* This file is included both from .c files and from .S files, so we
 * define certain prototypes and constants for the C files and define
 * the actual assembly code for the assembly files (where __ASM__ is
 * defined). */
#ifndef __ASM__ /* in C mode */

/* info gcc (section 6.30 Declaring Attributes of Functions) says:
 *   On the Intel 386, the `regparm' attribute causes the compiler to
 *   pass arguments number one to NUMBER if they are of integral type
 *   in registers EAX, EDX, and ECX instead of on the stack.
 * So, we expect to find &old_sp in eax and new_sp in edx. This can be
 * confirmed by using gdb to step through to this function (setting
 * a breakpoint on this function may not work properly) and then running
 * the "info registers" command. */
void __attribute__((regparm(2))) Xsthread_switch(char **old_sp, char *new_sp);
void Xsthread_switch_end();

/* This value tells the stack-setup code how much space (in bytes) we need
 * on the stack to store the general-purpose registers that make up a thread's
 * context. On i386, there are 8 32-bit general-purpose registers; the pusha
 * and popa instructions (used below) push and pop all 8 of these. */
#define STHREAD_CONTEXT_SIZE (8*4)

#else  /* in assembly mode */
.globl _old_sp
    .globl _new_sp
    .globl Xsthread_switch
    .globl Xsthread_switch_end

    /* in C terms: void Xsthread_switch(char **old_sp, char *new_sp) */
    Xsthread_switch:
    /* Push register state onto our current (old) stack (pusha = push
     * all registers).
     * PJH: note that pusha / popa only push/pop the x86 general-purpose
     * registers, but there is other state that makes up "context" such
     * as the floating-point registers, MMX registers, etc. So, I think
     * that this simplethreads code may not work (may cause mysterious
     * bugs) for threads in a program that uses these floating-point /
     * SIMD registers. */
    pusha

    /* Save old stack into *old_sp */
    movl %esp, (%eax)

    /* Load new stack from new_sp */
    movl %edx, %esp

    /* Pop saved registers off new stack (popa = pop all registers) */
    popa

    /* Return to whatever PC the current (new) stack
     * tells us to. */
    ret
    Xsthread_switch_end:

#endif  // __ASM__

#endif  // STHREAD_SWITCH_I386
