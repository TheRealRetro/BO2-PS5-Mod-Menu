/* Stand-in for DB_FindXAssetHeader: the real 20-byte prologue, then returns
   (edi << 32) | (dx << 16) | cx so the test can see the arguments arrive. */
    .intel_syntax noprefix
    .text
    .globl fake_db_find
fake_db_find:
    .byte 0x55, 0x48, 0x89, 0xe5, 0x41, 0x57, 0x41, 0x56, 0x41, 0x55
    .byte 0x41, 0x54, 0x53, 0x48, 0x81, 0xec, 0xb8, 0x00, 0x00, 0x00
    mov     eax, edi
    shl     rax, 32
    movzx   edx, dx
    shl     edx, 16
    or      rax, rdx
    movzx   ecx, cx
    or      rax, rcx
    add     rsp, 0xb8
    pop     rbx
    pop     r12
    pop     r13
    pop     r14
    pop     r15
    pop     rbp
    ret
    .section .note.GNU-stack, "", @progbits
