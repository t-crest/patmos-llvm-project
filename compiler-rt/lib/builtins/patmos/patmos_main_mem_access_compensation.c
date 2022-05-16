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
 * it can only clobber r3,r4,r5, and r6. It must remember to save s0 before using
 * predicates.
 *
 * ===----------------------------------------------------------------------===
 */

#include "../int_lib.h"

void __patmos_main_mem_access_compensation(void) __attribute__((alias("__patmos_main_mem_access_compensation2")));

void
__patmos_main_mem_access_compensation1(unsigned, unsigned) __attribute__((naked,noinline));
void
__patmos_main_mem_access_compensation1(unsigned max, unsigned remaining)
{
    __asm__ volatile (
		"mfs $r5 = $s0;"
		"__patmos_main_mem_access_compensation_loop:;"
		"cmpneq $p2 = $r3, 1;"
		"cmpneq $p3 = $r4, 0;"
		"lwm ($p3) $r0 = [$r0];"
		"br ($p2) __patmos_main_mem_access_compensation_loop; "
		"sub ($p3) $r4 = $r4, 1;"
		"sub ($p2) $r3 = $r3, 1;"
		"mts $s0 = $r5;"
		"retnd;"
		:
		: 
		: "r3", "r4", "r5"
	);
}

void
__patmos_main_mem_access_compensation2(unsigned, unsigned) __attribute__((naked,noinline));
void
__patmos_main_mem_access_compensation2(unsigned max, unsigned remaining)
{
    __asm__ volatile (
		"mfs $r5 = $s0;"
		"li  $r6 = 2;"
		"__patmos_main_mem_access_compensation2_loop:;"
		"cmpule $p2 = $r6, $r3;"
		"cmpule $p3 = $r6, $r4;"
		"lwm ($p3) $r0 = [$r0];"
		"lwm ($p3) $r0 = [$r0];"
		"br ($p2) __patmos_main_mem_access_compensation2_loop; "
		"sub ($p3) $r4 = $r4, $r6;"
		"sub ($p2) $r3 = $r3, $r6;"
		"cmpult $p3 = $r0, $r4;"
		"lwm ($p3) $r0 = [$r0];"
		"mts $s0 = $r5;"
		"retnd;"
		:
		: 
		: "r3", "r4", "r5", "r6"
	);
}

void
__patmos_main_mem_access_compensation4(unsigned, unsigned) __attribute__((naked,noinline));
void
__patmos_main_mem_access_compensation4(unsigned max, unsigned remaining)
{
    __asm__ volatile (
		"mfs $r5 = $s0;"
		"li  $r6 = 4;"
		"__patmos_main_mem_access_compensation4_loop:;"
		"cmpule $p2 = $r6, $r3;"
		"cmpule $p3 = $r6, $r4;"
		"lwm ($p3) $r0 = [$r0];"
		"lwm ($p3) $r0 = [$r0];"
		"lwm ($p3) $r0 = [$r0];"
		"lwm ($p3) $r0 = [$r0];"
		"br ($p2) __patmos_main_mem_access_compensation4_loop; "
		"sub ($p3) $r4 = $r4, $r6;"
		"sub $r3 = $r3, $r6;"
		"cmpult $p3 = $r0, $r4;"
		"sub ($p3) $r4 =  $r4, 1;"
		"lwm ($p3) $r0 = [$r0];"
		"cmpult $p3 = $r0, $r4;"
		"sub ($p3) $r4 =  $r4, 1;"
		"lwm ($p3) $r0 = [$r0];"
		"cmpult $p3 = $r0, $r4;"
		"sub ($p3) $r4 =  $r4, 1;"
		"lwm ($p3) $r0 = [$r0];"
		"mts $s0 = $r5;"
		"retnd;"
		:
		: 
		: "r3", "r4", "r5", "r6"
	);
}

void
__patmos_main_mem_access_compensation8(unsigned, unsigned) __attribute__((naked,noinline));
void
__patmos_main_mem_access_compensation8(unsigned max, unsigned remaining)
{
    __asm__ volatile (
		"br __patmos_main_mem_access_compensation8_cond; "
		"mfs $r5 = $s0;"
		"li  $r6 = 8;"
		"__patmos_main_mem_access_compensation8_loop:;"
		"lwm ($p3) $r0 = [$r0];"
		"lwm ($p3) $r0 = [$r0];"
		"lwm ($p3) $r0 = [$r0];"
		"lwm ($p3) $r0 = [$r0];"
		"lwm ($p3) $r0 = [$r0];"
		"lwm ($p3) $r0 = [$r0];"
		"lwm ($p3) $r0 = [$r0];"
		"lwm ($p3) $r0 = [$r0];"
		"__patmos_main_mem_access_compensation8_cond:;"
		"cmpule $p2 = $r6, $r3;"
		"cmpule $p3 = $r6, $r4;"
		"br ($p2) __patmos_main_mem_access_compensation8_loop; "
		"sub ($p2) $r3 = $r3, $r6;"
		"sub ($p3) $r4 = $r4, $r6;"
		"li  $r6 = 2;"
		"li  $r3 = 7;"
		"__patmos_main_mem_access_compensation82_loop:;"
		"cmpule $p2 = $r6, $r3;"
		"cmpule $p3 = $r6, $r4;"
		"lwm ($p3) $r0 = [$r0];"
		"lwm ($p3) $r0 = [$r0];"
		"br ($p2) __patmos_main_mem_access_compensation82_loop; "
		"sub ($p3) $r4 = $r4, $r6;"
		"sub ($p2) $r3 = $r3, $r6;"
		"cmpult $p3 = $r0, $r4;"
		"lwm ($p3) $r0 = [$r0];"
		"mts $s0 = $r5;"
		"retnd;"
		:
		: 
		: "r3", "r4", "r5", "r6"
	);
}
