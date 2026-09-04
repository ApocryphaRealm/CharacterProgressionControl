; Character Progression Control - the skill-cap trampoline stub.
;
; WHY THIS IS ASSEMBLY AND NOT A BYTE ARRAY:
; the patch site is not a call we can redirect - it is a 9-byte instruction that loads the
; constant 100.0 into XMM8, and replacing it means jumping to code of our own that puts a
; different number there and jumps back. That code has to obey a register contract exactly. Hand
; encoding it as a blob of hex would be untestable and unreviewable; written here, the assembler
; validates every instruction at build time and the contract is readable.
;
; THE CONTRACT AT THE PATCH SITE (Skyrim SE 1.5.97), which is what makes this correct:
;   RSI   holds the skill's ActorValue id (6 = One-handed ... 23 = Enchanting)
;   XMM0  holds the skill's CURRENT level and must survive untouched - the code after the
;         patch site compares XMM0 against the cap, so clobbering it breaks the comparison
;   XMM8  is where the maximum must end up
; So: move the id into the first integer argument, preserve XMM0 across the call, take our
; float result out of XMM0, put it in XMM8, restore XMM0, and jump back.
;
; The shadow space matters. The x64 calling convention requires 32 bytes of shadow space for the
; callee, so RSP drops by 0x30: 0x20 of shadow plus room to park XMM0 at [RSP+0x28].
;
; Technique credited to the Skyrim Skill Uncapper lineage (Kassent, Vadfromnu, Elys, Kasplat) -
; the register contract was read from their published SE source, which their Nexus permissions
; allow. No code is copied: this is our own stub, our own settings and our own scan.

EXTERN CPC_GetSkillCap:PROC          ; float CPC_GetSkillCap(unsigned int skillId)
EXTERN CPC_SkillCapReturn:QWORD      ; filled in at install time - where to resume

.code

CPC_SkillCapStub PROC
    mov     rcx, rsi                        ; skill id -> first integer argument
    sub     rsp, 30h                        ; 20h shadow space + room for the saved XMM0
    movss   dword ptr [rsp + 28h], xmm0     ; preserve the current skill level
    call    CPC_GetSkillCap                 ; returns the cap in XMM0
    movss   xmm8, xmm0                      ; the cap belongs in XMM8
    movss   xmm0, dword ptr [rsp + 28h]     ; restore the current skill level
    add     rsp, 30h
    jmp     qword ptr [CPC_SkillCapReturn]  ; back to the instruction after the patch
CPC_SkillCapStub ENDP

END
