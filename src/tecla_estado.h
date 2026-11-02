/* Estado compartido por el sondeo Next y las pruebas de PC.
   FRAMES se lee continuamente mientras esperamos: resta modular de 8 bits.
   La evaluacion usa un sondeo independiente; ReiniciarTecla evita repetir
   una tecla retenida de una lectura anterior hasta observar su liberacion. */
#ifndef MYLISP_REP_INICIO
#define MYLISP_REP_INICIO 15
#endif
#ifndef MYLISP_REP_PERIODO
#define MYLISP_REP_PERIODO 2
#endif
static unsigned int ultima_tecla;
static unsigned char instante_tecla, repitiendo, espacio_pendiente;
static unsigned int bloqueo_tecla;
static void ReiniciarTecla(void) {
    if (ultima_tecla) bloqueo_tecla = ultima_tecla;
    ultima_tecla = 0; espacio_pendiente = 0; repitiendo = 0;
}
static unsigned int EventoTecla(unsigned int k, unsigned char ahora,
                               unsigned char caps) {
    /* Esc sintetico llega como SPACE mientras CAPS sigue pulsado.
       Consumir toda la pulsacion y su liberacion en cualquier orden. */
    if (bloqueo_tecla == 27) {
        if (k || caps) return 0;
        bloqueo_tecla = 0;
        return 0;
    }
    if (bloqueo_tecla) {
        if (k == bloqueo_tecla) return 0;
        bloqueo_tecla = 0;
    }
    if (espacio_pendiente) {
        if (caps) {
            espacio_pendiente = 0; ultima_tecla = 27;
            return 27;
        }
        if ((unsigned char)(ahora - instante_tecla) < 2) return 0;
        espacio_pendiente = 0; ultima_tecla = 32;
        instante_tecla = ahora; repitiendo = 0;
        return 32;
    }
    if (!k) { ultima_tecla = 0; return 0; }
    if (k == 32) {
        if (ultima_tecla == 27) return 0;
        if (ultima_tecla != 32) {
            espacio_pendiente = 1; instante_tecla = ahora;
            return 0;
        }
    }
    if (k != ultima_tecla) {
        ultima_tecla = k; instante_tecla = ahora; repitiendo = 0;
        return k;
    }
    if (k == 27 || k == 13) return 0;
    if ((unsigned char)(ahora - instante_tecla) >=
        (repitiendo ? MYLISP_REP_PERIODO : MYLISP_REP_INICIO)) {
        repitiendo = 1; instante_tecla = ahora;
        return k;
    }
    return 0;
}
