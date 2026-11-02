#pragma codeseg CODE_PAGE_95
#include <stdio.h>
#include <string.h>
#include <conio.h>
#include <input.h>
#include <arch/z80.h>
#include <arch/zxn.h>
#include "teclado.h"
#include "control.h"
#include "colors.h"
#include "tecla_estado.h"
#ifndef FRAMES_BAJO
#define FRAMES_BAJO (*(volatile unsigned char *)0x5C78)
#endif
static unsigned char *mapa;
static unsigned char atributos;
/* Colores de conio (no indices de la ULA). Fondo negro para los 15
   estilos de texto. El 15 se reserva al bloque del cursor rojo. */
static const unsigned char tintas[16] = PALETA_TINTAS;
void Real_TecladoInit(void) {
    unsigned char ctl, ant, s;
    ctl = ZXN_READ_REG(0x6b);
    atributos = (ctl & 0x20) == 0;
    mapa = (unsigned char *)(0x4000 +
               ((unsigned int)(ZXN_READ_REG(0x6e) & 0x3f) << 8));
    ant = ZXN_READ_REG(0x43);
    ZXN_WRITE_REG(0x43, 0x30 | ((ctl & 0x10) ? 0x40 : 0));
    for (s = 0; s < 16; ++s) {
        ZXN_WRITE_REG(0x40, s << 4);
        ZXN_WRITE_REG(0x41, s == ESTILO_CURSOR ? PALETA_CURSOR_FONDO : PALETA_FONDO);
        ZXN_WRITE_REG(0x41, tintas[s]);
    }
    ZXN_WRITE_REG(0x43, ant);
    /* ULA estandar: borde 1 usa PAPER 17. Seleccionar la paleta ULA
       activa (bit 1), preservando modos/selecciones de visualizacion. */
    ZXN_WRITE_REG(0x43, (ant & 0x0f) | ((ant & 2) ? 0x40 : 0));
    ZXN_WRITE_REG(0x40, 16 + INDICE_BORDE_ULA);
    ZXN_WRITE_REG(0x41, PALETA_BORDE);
    ZXN_WRITE_REG(0x43, ant);
    z80_outp(0x00fe, INDICE_BORDE_ULA);
}
void Real_EsperarTecla(void) {
    while (in_Inkey()) ;
    while (!in_Inkey()) ;
    while (in_Inkey()) ;
}
static unsigned int LeerTecla(void) {
    unsigned int k;
    for (;;) {
        k = EventoTecla(in_Inkey(), FRAMES_BAJO,
                       (z80_inp(0xfefe) & 1) == 0);
        if (k) return k;
    }
}
/* Toda la linea fisica (maximo 500 caracteres) cabe en 8 filas.
   Reservar filas con el scroll de la consola mantiene tambien su estado.
   Los caracteres y atributos se repintan directamente como en ed_next.c;
   el cursor no altera el buffer, ni deja atributos al aceptar/cancelar. */
int Real_LeerTeclado(char *destino, int max) {
    unsigned int longitud, posicion, pintados, i, celda;
    unsigned char x, y;
    unsigned int k;
    unsigned char *p;
    longitud = posicion = pintados = 0;
    x = wherex(); y = wherey();
    destino[0] = 0;
    ReiniciarTecla();
    if (EjecucionCancelada) {
        bloqueo_tecla = 27;
        EjecucionCancelada = 0;
    }
    for (;;) {
        while (y + (x + longitud) / 80 >= 32) {
            gotoxy(0,31); putchar('\n'); --y;
        }
        for (i = 0; i <= pintados || i <= longitud; ++i) {
            celda = (unsigned int)y * 80 + x + i;
            p = mapa + celda * (atributos ? 2 : 1);
            *p++ = i < longitud ? destino[i] : ' ';
            if (atributos) *p = i == posicion ? ESTILO_CURSOR << 4 : COLOR_ENTRADA << 4;
        }
        pintados = longitud;
        gotoxy((x + posicion) % 80, y + (x + posicion) / 80);
        k = LeerTecla();
        if (k == 13 || k == 27) {
            /* Borrar el cursor antes de que la salida pueda desplazarlo. */
            celda = (unsigned int)y * 80 + x + posicion;
            if (atributos) mapa[celda * 2 + 1] = COLOR_ENTRADA << 4;
            gotoxy((x + longitud) % 80, y + (x + longitud) / 80);
            putchar('\n');
            destino[longitud] = 0;
            return k == 27 ? 2 : 1;
        }
        if (k == 8) { if (posicion) --posicion; }
        else if (k == 9) { if (posicion < longitud) ++posicion; }
        else if (k == 11 || k == 1) posicion = 0; /* arriba: inicio */
        else if (k == 10 || k == 5) posicion = longitud; /* abajo: final, NO Enter */
        else if (k == 12) { /* DELETE de in_Inkey: borrar anterior */
            if (posicion) {
                --posicion;
                memmove(destino + posicion, destino + posicion + 1,
                        longitud - posicion);
                --longitud;
            }
        }
        else if (k == 127) { /* CTRL+DELETE: borrar bajo el cursor */
            if (posicion < longitud) {
                memmove(destino + posicion, destino + posicion + 1,
                        longitud - posicion);
                --longitud;
            }
        }
        else if (k >= 32 && k <= 126 && longitud < max) {
            memmove(destino + posicion + 1, destino + posicion,
                    longitud - posicion + 1);
            destino[posicion++] = k; ++longitud;
        }
    }
}
