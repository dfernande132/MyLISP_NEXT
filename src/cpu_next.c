#include <stdio.h>
#include <arch/zxn.h>
#include "colors.h"
#include "cpu_next.h"
/* Captura al entrar en main: antes de configurar pantalla, bancos y heap.
   El CRT previo de esta compilacion no escribe el registro de velocidad.
   Lectura de ambos campos: solicitar 28 no garantiza velocidad efectiva. */
static unsigned char cpu_entrada;
static const char *cpu_mhz[] = { "3.5", "7", "14", "28" };
void CpuIniciar(void) {
    cpu_entrada = ZXN_READ_REG(0x07);
#ifdef MYLISP_CPU_28
    if ((cpu_entrada & 3) != 3) ZXN_WRITE_REG(0x07, 3);
#endif
}
static void MostrarVelocidad(const char *etiqueta, unsigned char r) {
    printf("CPU %s: set=%s MHz actual=%s MHz [07=%02X]\n", etiqueta,
           cpu_mhz[r & 3], cpu_mhz[(r >> 4) & 3], (unsigned int)r);
}
void CpuInformeActual(void) {
    unsigned char r = ZXN_READ_REG(0x07);
    INK(COLOR_INFO);
    MostrarVelocidad("now", r);
    INK(COLOR_ENTRADA);
}
void CpuInformeArranque(void) {
    unsigned char r = ZXN_READ_REG(0x07);
    INK(COLOR_INFO);
    printf("CPU mode: %s MHz\n", cpu_mhz[(r >> 4) & 3]);
    INK(COLOR_ENTRADA);
}
