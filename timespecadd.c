#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <string.h>
#include <assert.h>

void timespecadd(struct timespec *result, __const struct timespec *a, __const struct timespec *b);

void timespecadd_v1(struct timespec *result, __const struct timespec *a, __const struct timespec *b) {
	uint64_t ans = timespec2nsec(a);
	uint64_t bns = timespec2nsec(b);
	uint64_t rns = 0;

	if (__builtin_add_overflow(ans, bns, &rns)) {
		rns = ~0UL;
	}

	nsec2timespec(result, rns);
}

#if 1
#define __NS_PER_SEC 1000000000L
void timespecadd_v2(struct timespec *result, __const struct timespec *a, __const struct timespec *b) {
    // struct timespec res;
    time_t extra_secs = 0;
    int overflow = 0;

    overflow = __builtin_add_overflow(a->tv_sec, b->tv_sec, &result->tv_sec);

    if (!overflow) {
        extra_secs = a->tv_nsec / __NS_PER_SEC + b->tv_nsec / __NS_PER_SEC;
        result->tv_nsec = (a->tv_nsec % __NS_PER_SEC) + (b->tv_nsec % __NS_PER_SEC);

        if (result->tv_nsec > __NS_PER_SEC) {
            extra_secs++;
            result->tv_nsec -= __NS_PER_SEC;
        }

        overflow = __builtin_add_overflow(result->tv_sec, extra_secs, &result->tv_sec);

    }

    if (overflow) {
        if (a->tv_sec < 0 || b->tv_sec < 0) {
            result->tv_sec = INT64_MIN;
            result->tv_nsec = INT32_MIN;
        } else {
            result->tv_sec = INT64_MAX;
            result->tv_nsec = INT32_MAX;
        }
    }
}
#else
void timespecadd_v2(struct timespec *result, __const struct timespec *a, __const struct timespec *b) {
    // struct timespec res;
    time_t extra_secs = 0;
    int overflow = 0;
    uint64_t    ns;

    overflow = __builtin_add_overflow(a->tv_sec, b->tv_sec, &result->tv_sec);

    if (!overflow) {
        ns = a->tv_nsec + b->tv_nsec; //canot overflow
        extra_secs = ns / 1000000000;
        overflow = __builtin_add_overflow(result->tv_sec, extra_secs, &result->tv_sec);
        result->tv_nsec = ns % 1000000000;
    }

    if (overflow) {
        if (a->tv_sec < 0 || b->tv_sec < 0) {
            result->tv_sec = INT64_MIN;
            result->tv_nsec = INT32_MIN;
        } else {
            result->tv_sec = INT64_MAX;
            result->tv_nsec = INT32_MAX;
        }
    }
}

#endif

void ts_init(struct timespec *ts, time_t s, long ns) {
    ts->tv_sec = s;
    ts->tv_nsec = ns;
}

int ts_cmp(struct timespec *ts1, struct timespec *ts2) {
    return memcmp(ts1, ts2, sizeof(*ts1));
}

int main(int argn, char* argv[]) {
    struct timespec result1, result2, a, b;
    /* some normal value */
    ts_init(&a, 5, 345345);
    ts_init(&b, 6, 345345);

    timespecadd_v1(&result1, &a, &b);
    timespecadd_v2(&result2, &a, &b);
    assert(ts_cmp(&result1, &result2) == 0);

    /* timespec with ns over flow */
    ts_init(&a, 5, 345345);
    ts_init(&b, 6, __NS_PER_SEC - 10UL);
    timespecadd_v1(&result1, &a, &b);
    timespecadd_v2(&result2, &a, &b);
    assert(ts_cmp(&result1, &result2) == 0);

    /* timespec with nanoseconds > 1 secs*/
    ts_init(&a, 5, 345345);
    ts_init(&b, 6, __NS_PER_SEC + 235235UL);
    timespecadd_v1(&result1, &a, &b);
    timespecadd_v2(&result2, &a, &b);
    assert(ts_cmp(&result1, &result2) == 0);

    /* timespec with nanoseconds > 2 secs*/
    ts_init(&a, 5, 345345);
    ts_init(&b, 6, 2000000000UL + 235235UL);
    timespecadd_v1(&result1, &a, &b);
    timespecadd_v2(&result2, &a, &b);
    assert(ts_cmp(&result1, &result2) == 0);

    /* timespec with nanoseconds > 2 secs and ns overflow*/
    ts_init(&a, 5, __NS_PER_SEC - 10UL);
    ts_init(&b, 6, 2000000000UL + 235235UL);
    timespecadd_v1(&result1, &a, &b);
    timespecadd_v2(&result2, &a, &b);
    assert(ts_cmp(&result1, &result2) == 0);

    /* overflow seconds */
    ts_init(&a, 5, 345345);
    ts_init(&b, INT64_MAX - 3, 235235UL);
    ts_init(&result1, INT64_MAX, INT32_MAX);
    timespecadd_v2(&result2, &a, &b);
    assert(ts_cmp(&result1, &result2) == 0);

    /* overflow extra econds */
    ts_init(&a, 0, __NS_PER_SEC);
    ts_init(&b, INT64_MAX, 235235UL);
    ts_init(&result1, INT64_MAX, INT32_MAX);
    timespecadd_v2(&result2, &a, &b);
    assert(ts_cmp(&result1, &result2) == 0);

    /* negative result */
    ts_init(&a, 5, 345345);
    ts_init(&b, -6, 2000000000UL + 235235UL);
    ts_init(&result1, 5 -6 + 2, 345345+235235UL);
    timespecadd_v2(&result2, &a, &b);
    assert(ts_cmp(&result1, &result2) == 0);

    /* negative result */
    ts_init(&a, 5, -345345);
    ts_init(&b, -6, -2000000000UL - 235235UL);
    ts_init(&result1, 5 - 6 - 2, -345345-235235UL);
    timespecadd_v2(&result2, &a, &b);
    assert(ts_cmp(&result1, &result2) == 0);

    /* underflow second */
    ts_init(&a, -5, 345345);
    ts_init(&b, -INT64_MAX, 235235UL);
    ts_init(&result1, INT64_MIN, INT32_MIN);
    timespecadd_v2(&result2, &a, &b);
    assert(ts_cmp(&result1, &result2) == 0);

    /* underlow extra econds */
    ts_init(&a, 0, -__NS_PER_SEC);
    ts_init(&b, INT64_MIN, 235235UL);
    ts_init(&result1, INT64_MIN, INT32_MIN);
    timespecadd_v2(&result2, &a, &b);
    assert(ts_cmp(&result1, &result2) == 0);

}
