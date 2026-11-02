; Diagnostico temporal: solo RAM residente y pila; no cambia MMU ni IFF.
SECTION code_compiler
PUBLIC _DiagPila, _DiagIniciar, _DiagCapturar, _DiagBase
EXTERN _DiagInicio, _DiagMin, _DiagMarca, __BSS_END_tail
defc _DiagBase = __BSS_END_tail

; Preserva AF/HL/DE; BC, IX e IY no se tocan.
; CALL + PUSH AF/HL/DE = 8 bytes. Sumarlos mide el SP del llamante
; inmediatamente ANTES de CALL, descontando el coste propio del sondeo.
_DiagPila:
    push af
    push hl
    push de
    ld hl,8
    add hl,sp
    ex de,hl
    ld hl,(_DiagMin)
    or a
    sbc hl,de
    jr c,diag_fin_muestra
    ld (_DiagMin),de
 diag_fin_muestra:
    pop de
    pop hl
    pop af
    ret

; Invocar desde el bucle REPL, con la pila ya desenrollada.
; Ref = SP antes del CALL. Rellenar SOLO [fin BSS, SP de entrada):
; nunca tocar el retorno ni los marcos activos del llamante.
_DiagIniciar:
    ld hl,2
    add hl,sp
    ld (_DiagInicio),hl
    ld (_DiagMin),hl
    dec hl
    dec hl
    ld de,__BSS_END_tail
    or a
    sbc hl,de
    ld b,h
    ld c,l
    ex de,hl
 diag_rellenar:
    ld a,b
    or c
    ret z
    ld (hl),$A5
    inc hl
    dec bc
    jr diag_rellenar

; Primera direccion del patron que ya no contiene A5, de abajo arriba.
; Incluye escrituras de runtime/interrupciones, pero no detecta reservas
; de pila que nunca se escriben ni escrituras casualmente iguales a A5.
; El muestreo SP complementa esta limitacion. Ninguna medida es una cota
; demostrada del peor caso del programa.
_DiagCapturar:
    ld hl,(_DiagInicio)
    dec hl
    dec hl
    ld de,__BSS_END_tail
    or a
    sbc hl,de
    ld b,h
    ld c,l
    ex de,hl
 diag_buscar:
    ld a,b
    or c
    jr z,diag_encontrado
    ld a,(hl)
    cp $A5
    jr nz,diag_encontrado
    inc hl
    dec bc
    jr diag_buscar
 diag_encontrado:
    ld (_DiagMarca),hl
    ret
