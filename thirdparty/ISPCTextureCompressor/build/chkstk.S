/* __chkstk for x86_64 MinGW - probes stack pages so the OS can grow the stack.
   Called by ISPC-generated code when a function needs > 4KB of stack.
   On entry: rax = number of bytes needed.
   Must preserve all registers except rax and r11. */
    .text
    .globl __chkstk
    .def __chkstk; .scl 2; .type 32; .endef
__chkstk:
    push    %rcx
    push    %rax
    cmp     $0x1000, %rax
    lea     0x18(%rsp), %rcx    /* original rsp before call+pushes */
    jb      .Ldone
.Lloop:
    sub     $0x1000, %rcx
    test    %rcx, (%rcx)        /* touch the page (stack probe) */
    sub     $0x1000, %rax
    cmp     $0x1000, %rax
    ja      .Lloop
.Ldone:
    sub     %rax, %rcx
    test    %rcx, (%rcx)
    pop     %rax
    pop     %rcx
    ret
