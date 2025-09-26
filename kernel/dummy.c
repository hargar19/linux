// SPDX-License-Identifier: GPL-2.0
// new-syscall
/*
 * Dummy syscall for baseline measurements
 * Returns 0 - minimal overhead syscall for comparison
 */
#include <linux/kernel.h>
#include <linux/syscalls.h>

SYSCALL_DEFINE0(dummy)
{
    return 0;
}