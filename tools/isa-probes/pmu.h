/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Count E2K hardware monitor events in user code through Linux perf
   (spec/isa/monitors.md).  Header only; include it in a probe.

     int fd = pmu_open (0x313);         // EXEC on monitor DIM1
     unsigned long c = pmu_count (fd, fn, n, p);

   An event's config is 0xMMEE: MM the monitor (0 DDM0, 1 DDM1, 2 DIM0,
   3 DIM1, 4 either DDM, 5 either DIM), EE the event number
   (spec/isa/monitor-events-v3.yaml).  Counting is per thread and user
   mode only, which /proc/sys/kernel/perf_event_paranoid 2 allows.

   While an event counts on DIM0 or DIM1, user code can also read the
   counter itself with pmu_dimar0 () or pmu_dimar1 () (rrd %dimarN, no
   system call), which is cheaper but includes nothing that perf adds
   (the value is the raw counter of the current CPU: pin the thread).
   The data monitors (%ddmarN) are not readable in user mode.  */

#ifndef PMU_H
#define PMU_H

#include <linux/perf_event.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <unistd.h>

static inline unsigned long
pmu_dimar0 (void)
{
  unsigned long v;
  __asm__ volatile ("rrd %%dimar0, %0" : "=r" (v));
  return v;
}

static inline unsigned long
pmu_dimar1 (void)
{
  unsigned long v;
  __asm__ volatile ("rrd %%dimar1, %0" : "=r" (v));
  return v;
}

/* Open one user-mode counter for CONFIG, disabled.  Returns the fd or
   -1 (errno set; EINVAL: the event does not exist on this monitor or
   CPU).  */
static int
pmu_open (unsigned long config)
{
  struct perf_event_attr a;
  memset (&a, 0, sizeof a);
  a.size = sizeof a;
  a.type = PERF_TYPE_RAW;
  a.config = config;
  a.disabled = 1;
  a.exclude_kernel = 1;
  a.exclude_hv = 1;
  return syscall (SYS_perf_event_open, &a, 0, -1, -1, 0);
}

typedef long (*pmu_fn) (long, long *);

/* The count of FD's event during F (N, P).  The figure includes a few
   instructions of the ioctl wrappers; subtract a run of an empty F.  */
static unsigned long
pmu_count (int fd, pmu_fn f, long n, long *p)
{
  unsigned long v = 0;
  ioctl (fd, PERF_EVENT_IOC_RESET, 0);
  ioctl (fd, PERF_EVENT_IOC_ENABLE, 0);
  f (n, p);
  ioctl (fd, PERF_EVENT_IOC_DISABLE, 0);
  if (read (fd, &v, sizeof v) != sizeof v)
    return ~0UL;
  return v;
}

#endif
