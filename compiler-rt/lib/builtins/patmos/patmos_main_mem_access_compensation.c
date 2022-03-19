/* ===---- patmos_main_mem_access_compensation.c - Access Compensation -----===
 *
 *                     The LLVM Compiler Infrastructure
 *
 * This file is dual licensed under the MIT and the University of Illinois Open
 * Source Licenses. See LICENSE.TXT for details.
 *
 * ===----------------------------------------------------------------------===
 *
 * Function that can compensate for the variability in main memory accesses.
 *
 * "max" is the maximum number of extra enabled accesses a function can need
 * "remaining" is the number of enabled accesses a given call to the function
 * is missing.
 * 
 * __patmos_main_mem_access_compensation performs "remaining" number of enabled
 * loads to main memory.
 *
 * __patmos_main_mem_access_compensation has a custom calling convention:
 * it can only clobber r3,r4, and r5. It must remember to save s0 before using
 * predicates.
 *
 * ===----------------------------------------------------------------------===
 */

#include "../int_lib.h"

void
__patmos_main_mem_access_compensation(unsigned, unsigned) __attribute__((naked,noinline));
void
__patmos_main_mem_access_compensation(unsigned max, unsigned remaining)
{
    __asm__ volatile (
		"mfs $r5 = $s0;"
		"__patmos_main_mem_access_compensation_loop:;"
		"cmpult $p2 = $r0, $r3;"
		"cmpult $p3 = $r0, $r4;"
		"lwm ($p3) $r0 = [$r0];"
		"br ($p2) __patmos_main_mem_access_compensation_loop; "
		"sub ($p3) $r4 = $r4, 1;"
		"sub $r3 = $r3, 1;"
		"mts $s0 = $r5;"
		"retnd;"
		:
		: 
		: "r3", "r4", "r5"
	);
}

