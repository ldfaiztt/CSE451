/* This file is just a place holder for the real assembly, which is in a
 * cpu-specific file we conditionally include.
 *
 * We put it in a .S file, instead of using the gcc 'asm (...)' syntax, to
 * make it more robust (this way, the compiler won't change _anything_, and we
 * know exactly what the stack will look like. 
 * Also note that, by calling this sthread_switch.S rather than .s, this file
 * will be run through the C pre-processor before being fed to the assembler.
 */

#include <config.h>

#ifdef STHREAD_CPU_I386	
#include "sthread_switch_i386.h"
#endif

#ifdef STHREAD_CPU_X86_64
#include "sthread_switch_x86_64.h"
#endif
