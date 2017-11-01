.intel_syntax noprefix
.text

. = 0x004CBE1A
#;fullscreen = 0x004CBE5F3
#;done = 0x004CBE6B

SetAspRatio:
    mov edx, [0x008FD484]
    test edx, edx
    je Fullscreen
    
#; Multiply width by 0.75 to go from "widescreen" -> "fullscreen"
    mov edi, [0x006E9898]
    mov esi, edi
    shr esi, 02
    sub edi, esi
    mov esi, [0x006E989C]
    jmp Done
    
. = 0x004CBE5F
Fullscreen:

. = 0x004CBE6B
Done:
