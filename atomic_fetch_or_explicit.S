//int _atomic_fetch_or_release(unsigned int *target, unsigned int value, uint64_t *spincount);
.global _atomic_fetch_or_release
_atomic_fetch_or_release:
        mov     x10, x0
        mov     x11, x2
        mov     x4, 0
    1:  add     x4, x4, 1
        ldxr    w0, [x10]
        orr     w2, w1, w0
        stlxr   w3, w2, [x10]
        cbnz    w3, 1b
        str     x4, [x11]
        ret

//int _atomic_fetch_or_relaxed(unsigned int *target, unsigned int value, uint64_t *spincount, unsigned cpu);
.global _atomic_fetch_or_relaxed
_atomic_fetch_or_relaxed:
#ifdef AF_NOPS
        mov     x10, x0     // address of sync object
        mov     x11, x2     //spincount
        mov     x12, x3     //cpu
        mov     x4, 0
    1:  add     x4, x4, 1
        ldxr    w0, [x10]
        orr     w2, w1, w0
        stxr    w3, w2, [x10]
        cbz     w3, suc2
        ands    w3, w4, #15
        bne     1b
        add     x0, x12, 1
        lsl     x0, x0, 1
    2:  nop    
        subs    x0, x0, 1
        cbnz    x0, 2b
        b 1b
suc2:   str     x4, [x11]
        ret
#elif defined AF_CAS
        mov     x10, x0
        mov     x11, x2     //spincount
        mov     x4, 0
    1:  add     x4, x4, 1
        ldr     w0, [x10]
        orr     w2, w1, w0
        casa    w0, w2, [x10]
        ldr     w3, [x10]
        cmp     w3, w0
        bne     1b
        str     x4, [x11]
        ret
#else
        mov     x10, x0
        mov     x11, x2
        mov     x4, 0
    1:  add     x4, x4, 1
        ldxr    w0, [x10]
        orr     w2, w1, w0
        stxr    w3, w2, [x10]
        cbnz    w3, 1b
        str     x4, [x11]
        ret
#endif
