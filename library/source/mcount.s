.global __gnu_mcount_nc

.section .text

.type __gnu_mcount_nc, %function
__gnu_mcount_nc:
    push {r0, r1, r2, r3, lr}
    subs r1, lr, #0
    ldr r0, [sp, #0x14]
    bl __mcount_internal
    pop {r0, r1, r2, r3, r12, lr}
    bx r12