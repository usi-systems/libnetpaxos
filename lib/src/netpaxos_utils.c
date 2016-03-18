#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include "stdint.h"
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#ifdef __MACH__
#include <mach/clock.h>
#include <mach/mach.h>

#endif

void gettime(struct timespec * ts) {
#ifdef __MACH__ 
        clock_serv_t cclock;
        mach_timespec_t mts;
        host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &cclock);
        clock_get_time(cclock, &mts);
        mach_port_deallocate(mach_task_self(), cclock);
        ts->tv_sec = mts.tv_sec;
        ts->tv_nsec = mts.tv_nsec;

#else
        clock_gettime(CLOCK_REALTIME, ts);
#endif
}

int timediff(struct timespec *result, struct timespec *x, struct timespec *y)
{
  result->tv_sec = x->tv_sec - y->tv_sec;
  result->tv_nsec = x->tv_nsec - y->tv_nsec;

  /* Return 1 if result is negative. */
  return x->tv_sec < y->tv_sec;
}


int compare_ts(struct timespec *time1, struct timespec *time2) {
        if (time1->tv_sec < time2->tv_sec)
            return (-1) ;               /* Less than */
        else if (time1->tv_sec > time2->tv_sec)
            return (1) ;                /* Greater than */
        else if (time1->tv_nsec < time2->tv_nsec)
            return (-1) ;               /* Less than */
        else if (time1->tv_nsec > time2->tv_nsec)
            return (1) ;                /* Greater than */
        else
            return (0) ;                /* Equal */
}