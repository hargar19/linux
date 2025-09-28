// SPDX-License-Identifier: GPL-2.0
// new-syscall
/*
 * Dummy syscall for baseline measurements
 * Returns 0 - minimal overhead syscall for comparison
 * Option B: Includes kernel-side timing for precise measurements
 */
#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/ktime.h>
#include <linux/atomic.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>

/* Global counters for kernel-side profiling */
static atomic64_t call_count = ATOMIC64_INIT(0);
static atomic64_t total_time_ns = ATOMIC64_INIT(0);
static atomic64_t min_time_ns = ATOMIC64_INIT(LLONG_MAX);
static atomic64_t max_time_ns = ATOMIC64_INIT(0);

SYSCALL_DEFINE0(dummy)
{
    ktime_t start, end;
    s64 delta_ns;
    s64 current_min, current_max;
    
    start = ktime_get();
    
    /* Minimal work - just increment counter */
    atomic64_inc(&call_count);
    
    end = ktime_get();
    delta_ns = ktime_to_ns(ktime_sub(end, start));
    
    /* Update timing statistics */
    atomic64_add(delta_ns, &total_time_ns);
    
    /* Update min time (atomic compare-and-swap loop) */
    do {
        current_min = atomic64_read(&min_time_ns);
        if (delta_ns >= current_min)
            break;
    } while (atomic64_cmpxchg(&min_time_ns, current_min, delta_ns) != current_min);
    
    /* Update max time (atomic compare-and-swap loop) */
    do {
        current_max = atomic64_read(&max_time_ns);
        if (delta_ns <= current_max)
            break;
    } while (atomic64_cmpxchg(&max_time_ns, current_max, delta_ns) != current_max);
    
    return 0;
}

/* Proc interface to read kernel-side statistics */
static int dummy_stats_show(struct seq_file *m, void *v)
{
    s64 calls = atomic64_read(&call_count);
    s64 total_ns = atomic64_read(&total_time_ns);
    s64 min_ns = atomic64_read(&min_time_ns);
    s64 max_ns = atomic64_read(&max_time_ns);
    
    seq_printf(m, "=== Kernel-Side Dummy Syscall Statistics ===\n");
    seq_printf(m, "Total calls:      %lld\n", calls);
    
    if (calls > 0) {
        s64 avg_ns = total_ns / calls;
        s64 min_display = (min_ns == LLONG_MAX) ? 0 : min_ns;
        
        seq_printf(m, "Total time:       %lld ns (%lld.%06lld ms)\n", 
                   total_ns, total_ns / 1000000, (total_ns / 1000) % 1000);
        seq_printf(m, "Average time:\n");
        seq_printf(m, "  %lld ns\n", avg_ns);
        seq_printf(m, "  %lld.%06lld ms\n", avg_ns / 1000000, (avg_ns / 1000) % 1000);
        seq_printf(m, "  %lld.%09lld sec\n", avg_ns / 1000000000, avg_ns % 1000000000);
        seq_printf(m, "Min time:\n");
        seq_printf(m, "  %lld ns\n", min_display);
        seq_printf(m, "  %lld.%06lld ms\n", min_display / 1000000, (min_display / 1000) % 1000);
        seq_printf(m, "  %lld.%09lld sec\n", min_display / 1000000000, min_display % 1000000000);
        seq_printf(m, "Max time:\n");
        seq_printf(m, "  %lld ns\n", max_ns);
        seq_printf(m, "  %lld.%06lld ms\n", max_ns / 1000000, (max_ns / 1000) % 1000);
        seq_printf(m, "  %lld.%09lld sec\n", max_ns / 1000000000, max_ns % 1000000000);
        seq_printf(m, "Calls per second: %lld (estimated)\n", 
                   total_ns > 0 ? (1000000000LL * calls) / total_ns : 0);
    } else {
        seq_printf(m, "No calls recorded yet\n");
    }
    seq_printf(m, "===========================================\n");
    
    return 0;
}

static int dummy_stats_open(struct inode *inode, struct file *file)
{
    return single_open(file, dummy_stats_show, NULL);
}

static ssize_t dummy_stats_write(struct file *file, const char __user *buffer, 
                                 size_t count, loff_t *pos)
{
    /* Reset statistics when anything is written to the file */
    atomic64_set(&call_count, 0);
    atomic64_set(&total_time_ns, 0);
    atomic64_set(&min_time_ns, LLONG_MAX);
    atomic64_set(&max_time_ns, 0);
    
    return count;
}

static const struct proc_ops dummy_stats_ops = {
    .proc_open    = dummy_stats_open,
    .proc_read    = seq_read,
    .proc_write   = dummy_stats_write,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release,
};

static int __init dummy_syscall_init(void)
{
    /* Create /proc/dummy_stats for reading statistics */
    proc_create("dummy_stats", 0666, NULL, &dummy_stats_ops);
    pr_info("dummy syscall: kernel-side profiling enabled, stats at /proc/dummy_stats\n");
    return 0;
}

static void __exit dummy_syscall_exit(void)
{
    remove_proc_entry("dummy_stats", NULL);
    pr_info("dummy syscall: kernel-side profiling disabled\n");
}

module_init(dummy_syscall_init);
module_exit(dummy_syscall_exit);