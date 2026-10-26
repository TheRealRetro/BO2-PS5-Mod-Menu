/*
 * DB_FindXAssetHeader hook, copied into an executable allocation in the game.
 *
 * PS5 executable pages are execute-only: this code must never read from its
 * own allocation, so every address it needs (script name, script tree,
 * return address) is an imm64 operand filled in by the payload, and the data
 * itself lives in a separate read/write allocation.
 *
 * Only caller-saved registers that carry no arguments are touched (rax,
 * r8-r11), so the original call continues with edi/rsi/edx/ecx intact.
 *
 *   type 0x31 (scriptparsetree) + name == script name (case-insensitive,
 *   '\' == '/')  -> return script tree
 *   anything else -> original 20-byte prologue, then back into the game
 */
    .intel_syntax noprefix
    .section .rodata.deadops_hook, "a"
    .p2align 4
    .globl deadops_hook_begin, deadops_hook_name_imm, deadops_hook_tree_imm
    .globl deadops_hook_ret_imm, deadops_hook_end

deadops_hook_begin:
    cmp     edi, 0x31
    jne     .Lorig
    .byte   0x49, 0xbb              /* movabs r11, <script name> */
deadops_hook_name_imm:
    .quad   0
    mov     r10, rsi
.Lloop:
    movzx   r8d, byte ptr [r10]
    movzx   r9d, byte ptr [r11]
    lea     eax, [r8 - 0x41]
    cmp     eax, 25
    ja      .Lnot_upper
    or      r8d, 0x20
.Lnot_upper:
    cmp     r8d, 0x5c
    jne     .Lcompare
    mov     r8d, 0x2f
.Lcompare:
    cmp     r8d, r9d
    jne     .Lorig
    test    r9d, r9d
    je      .Lmatch
    inc     r10
    inc     r11
    jmp     .Lloop
.Lmatch:
    .byte   0x48, 0xb8              /* movabs rax, <script tree> */
deadops_hook_tree_imm:
    .quad   0
    ret
.Lorig:
    /* DB_FindXAssetHeader's first 20 bytes in PPSA34502 codzm.elf (0x1C5940):
       push rbp; mov rbp,rsp; push r15..r12; push rbx; sub rsp,0xb8 */
    .byte   0x55, 0x48, 0x89, 0xe5, 0x41, 0x57, 0x41, 0x56, 0x41, 0x55
    .byte   0x41, 0x54, 0x53, 0x48, 0x81, 0xec, 0xb8, 0x00, 0x00, 0x00
    /* rax is free here: the game's next instruction (+0x14) overwrites it. */
    .byte   0x48, 0xb8              /* movabs rax, <DB_FindXAssetHeader + 20> */
deadops_hook_ret_imm:
    .quad   0
    jmp     rax
deadops_hook_end:
