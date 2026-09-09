.text
.p2align 4
.global native_template_start
.global native_template_op
.global native_template_end
.hidden native_template_start
.hidden native_template_op
.hidden native_template_end
// Relocation-free one-instruction sandbox trampoline. Guest SP is never loaded.
// All host callee-saved state, including SIMD and Android's x18, stays on host SP.
native_template_start:
    sub sp, sp, #256
    stp x19, x20, [sp, #0]
    stp x21, x22, [sp, #16]
    stp x23, x24, [sp, #32]
    stp x25, x26, [sp, #48]
    stp x27, x28, [sp, #64]
    stp x29, x30, [sp, #80]
    str x18, [sp, #96]
    str x0, [sp, #104]
    stp d8, d9, [sp, #112]
    stp d10, d11, [sp, #128]
    stp d12, d13, [sp, #144]
    stp d14, d15, [sp, #160]
    mrs x1, fpcr
    mrs x2, fpsr
    stp x1, x2, [sp, #208]
    msr fpcr, xzr
    msr fpsr, xzr
    mov x30, x0
    ldr x1, [x30, #248]
    msr nzcv, x1
    ldp d0, d1, [x30, #256]
    ldp d2, d3, [x30, #272]
    ldp d4, d5, [x30, #288]
    ldp d6, d7, [x30, #304]
    ldp d8, d9, [x30, #320]
    ldp d10, d11, [x30, #336]
    ldp d12, d13, [x30, #352]
    ldp d14, d15, [x30, #368]
    ldp d16, d17, [x30, #384]
    ldp d18, d19, [x30, #400]
    ldp d20, d21, [x30, #416]
    ldp d22, d23, [x30, #432]
    ldp d24, d25, [x30, #448]
    ldp d26, d27, [x30, #464]
    ldp d28, d29, [x30, #480]
    ldp d30, d31, [x30, #496]
    ldp x0, x1, [x30, #0]
    ldp x2, x3, [x30, #16]
    ldp x4, x5, [x30, #32]
    ldp x6, x7, [x30, #48]
    ldp x8, x9, [x30, #64]
    ldp x10, x11, [x30, #80]
    ldp x12, x13, [x30, #96]
    ldp x14, x15, [x30, #112]
    ldp x16, x17, [x30, #128]
    ldp x18, x19, [x30, #144]
    ldp x20, x21, [x30, #160]
    ldp x22, x23, [x30, #176]
    ldp x24, x25, [x30, #192]
    ldp x26, x27, [x30, #208]
    ldp x28, x29, [x30, #224]
    ldr x30, [x30, #240]
native_template_op:
    nop
    stp x29, x30, [sp, #176]
    ldr x30, [sp, #104]
    stp x0, x1, [x30, #0]
    stp x2, x3, [x30, #16]
    stp x4, x5, [x30, #32]
    stp x6, x7, [x30, #48]
    stp x8, x9, [x30, #64]
    stp x10, x11, [x30, #80]
    stp x12, x13, [x30, #96]
    stp x14, x15, [x30, #112]
    stp x16, x17, [x30, #128]
    stp x18, x19, [x30, #144]
    stp x20, x21, [x30, #160]
    stp x22, x23, [x30, #176]
    stp x24, x25, [x30, #192]
    stp x26, x27, [x30, #208]
    str x28, [x30, #224]
    ldp x0, x1, [sp, #176]
    stp x0, x1, [x30, #232]
    mrs x0, nzcv
    str x0, [x30, #248]
    stp d0, d1, [x30, #256]
    stp d2, d3, [x30, #272]
    stp d4, d5, [x30, #288]
    stp d6, d7, [x30, #304]
    stp d8, d9, [x30, #320]
    stp d10, d11, [x30, #336]
    stp d12, d13, [x30, #352]
    stp d14, d15, [x30, #368]
    stp d16, d17, [x30, #384]
    stp d18, d19, [x30, #400]
    stp d20, d21, [x30, #416]
    stp d22, d23, [x30, #432]
    stp d24, d25, [x30, #448]
    stp d26, d27, [x30, #464]
    stp d28, d29, [x30, #480]
    stp d30, d31, [x30, #496]
    ldp x19, x20, [sp, #0]
    ldp x21, x22, [sp, #16]
    ldp x23, x24, [sp, #32]
    ldp x25, x26, [sp, #48]
    ldp x27, x28, [sp, #64]
    ldp x29, x30, [sp, #80]
    ldr x18, [sp, #96]
    ldp d8, d9, [sp, #112]
    ldp d10, d11, [sp, #128]
    ldp d12, d13, [sp, #144]
    ldp d14, d15, [sp, #160]
    ldp x0, x1, [sp, #208]
    msr fpcr, x0
    msr fpsr, x1
    add sp, sp, #256
    ret
native_template_end:
.section .note.GNU-stack,"",%progbits
