.intel_syntax noprefix
.text

CharDiff_SpeedCheck:
cmp     word ptr [ebx+0x72], 0 # Skip if jumping
jz      short CharDiff_SpeedCheck+142
mov     eax, [ebx+44]
cmp     eax, dword ptr SlopeFix_SpeedCutoff    # Speed cutoff
jg      CharDiff_SlopeCheckSkip

mov     eax, [ebx+0xB4] # Skip if not on big hill
sar     eax, 0x10
cmp     eax, dword ptr SlopeFix_AngleCutoff # Cutoff angle
jle     CharDiff_SpeedCheck+142

CharDiff_SlopeCheckSkip:
test    byte ptr [ebp-0x13], 0x11 # Accelerating with fwd or button?
jz      short CharDiff_SpeedCheck+108
mov     eax, [ebx+0x84]
sar     eax, 0x10
test    eax, eax
jg      short CharDiff_SpeedCheck+108
mov     dh, byte ptr [ebp-0x14]
test    dh, 0x08           # Pressing left
jz      short CharDiff_SpeedCheck+73
test    dh, 0x80        # Pressing right
jnz     short CharDiff_SpeedCheck+108
