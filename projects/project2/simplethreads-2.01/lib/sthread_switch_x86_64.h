#ifndef STHREAD_SWITCH_X86_64
#define STHREAD_SWITCH_X86_64

/* This is the x86_64 assembly for the actual register saver/stack switcher.
 *
 * void Xsthread_switch(char **old_sp, char *new_sp)
 *   Save the currently running thread's context on its stack, switch to
 *   the new thread by swapping in its stack pointer, then pop that thread's
 *   context off of the stack and return. The state that we store on the
 *   stack includes all of the general-purpose registers; note that we
 *   DON'T store other state that makes up "context" such as the floating-
 *   point registers, MMX registers, and so on. So, this means that this
 *   code may not work (may cause mysterious bugs) for threads in a program
 *   that uses these floating-point / SIMD registers.
 *
 * We put this code in a .S file, instead of using the gcc 'asm (...)' syntax,
 * to make it more robust (this way, the compiler won't change _anything_, and
 * we know exactly what the stack will look like. 
 * 
 * Also note that, by calling this sthread_switch.S rather than .s, this file
 * will be run through the C pre-processor before being fed to the assembler.
 */

#include <config.h>

/* This file is included both from .c files and from .S files, so we
 * define certain prototypes and constants for the C files and define
 * the actual assembly code for the assembly files (where __ASM__ is
 * defined).
 */
#ifndef __ASM__  // in C mode

/* With x86_64, we can no longer use the "regparm" attribute that the
 * i386 code uses; using gdb, I (PJH) verified that &old_sp and new_sp are
 * no longer stored in rax and rdx when running on an x86_64 platform
 * with the regparm attibute set on this function. Instead, we simply
 * use the x86_64 calling convention (the "System V ABI", which is what
 * gcc follows), which states that integer and pointer values are passed
 * to called functions in "the next available register of the sequence
 * %rdi, $rsi, %rdx, %rcx, %r8, and %r9". So, we can get the &old_sp
 * argument from $rdi and the new_sp argument from $rsi. Again, this can
 * be checked by using gdb. Sources of information for the x86_64 calling
 * convention:
 *   http://www.x86-64.org/documentation/abi.pdf (p. 17, p. 20)
 *   http://www.agner.org/optimize/calling_conventions.pdf
 *   http://en.wikipedia.org/wiki/X86_calling_conventions
 * Note that on Windows (Microsoft and Intel compilers) the convention is
 * different, so this code will break. Use gcc only.
 */
void Xsthread_switch(char **old_sp, char *new_sp);
void Xsthread_switch_end();

/* This value tells the stack-setup code (sthread_new_ctx(), sthread_init_stack())
 * how much space (in bytes) we need on the stack to store the general-purpose
 * registers that make up a thread's context. On x86_64 architectures, there
 * are 16 64-bit (8-byte) general-purpose registers; however, we don't store
 * the stack pointer register on the stack, because we store it separately in
 * the thread context structures and pass it as an argument to this function.
 * Therefore, we only need room for 15 registers in our stack context (and so
 * there are 15 pushes and 15 pops in the code below).
 */
#define STHREAD_CONTEXT_SIZE (15*8)

#else  /* in assembly mode */

.globl _old_sp
    .globl _new_sp
    .globl Xsthread_switch
    .globl Xsthread_switch_end

    /* in C terms: void Xsthread_switch(char **old_sp, char *new_sp) */
    Xsthread_switch:
    /* Push register state onto our current (old) stack. The i386 pusha
     * and popa instructions no longer work in 64-bit mode, so instead
     * we explicitly push all of the general-purpose registers here.
     * We don't push any of the floating-point or special purpose
     * registers (e.g. MMX, SSE, and other SIMD registers). Also, we
     * ignore the stack pointer register RSP, because we store it directly
     * in our thread context structures and pass it as an argument to this
     * code (see the documentation for the i386 POPA instruction: it also
     * ignores the ESP register when restoring the register state).
     * Specifying the 64-bit registers as operands causes the stack pointer
     * to be decremented by 8 bytes.
     *
     * The amount of data pushed onto the stack here (and popped off later
     * on) must match STHREAD_CONTEXT_SIZE!
     *
     * The Linux kernel files arch/x86/kernel/entry_[32,64].S shed some
     * light on how context switching is done for _processes_ in Linux;
     * this code is similar, but we don't bother with some of the complexity
     * (e.g. saving segment registers) and we don't have to follow any
     * specific calling conventions; the only convention we have to follow
     * is that the registers are popped in the opposite order that they
     * were pushed.
     */
    push %rax
    push %rbx
    push %rcx
    push %rdx
    push %rdi
    push %rsi
    push %rbp
    push %r8
    push %r9
    push %r10
    push %r11
    push %r12
    push %r13
    push %r14
    push %r15

    /* Save old stack into memory at *old_sp; old_sp is passed to us in
     * register rdi. The stack pointer on x86_64 is rsp, rather than esp;
     * the Intel documentation for the PUSH instruction says that "in
     * 64-bit mode, the size of the stack pointer is always 64 bits." We
     * use movq (q = "quad-word"), rather than movl, for 64-bit registers.
     */
    movq %rsp, (%rdi)

    /* Load new stack from new_sp, which is passed to us in register rsi. */
    movq %rsi, %rsp

    /* Pop saved registers off new stack: */
    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %r11
    pop %r10
    pop %r9
    pop %r8
    pop %rbp
    pop %rsi
    pop %rdi
    pop %rdx
    pop %rcx
    pop %rbx
    pop %rax

    /* Return to whatever PC the current (new) stack tells us to: */
    ret
    Xsthread_switch_end:

#endif  // __ASM__

#endif  // STHREAD_SWITCH_X86_64

