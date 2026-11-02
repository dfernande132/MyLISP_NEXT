#include "diag_pila.h"
#include <errno.h>
#include <stdio.h>
#include <arch/zxn/esxdos.h>
#include "lexer.h"
#include "heap_ops.h"
#include "parser.h"
#include "eval.h"
#include "printer.h"
#include "messages.h"
#include "colors.h"
#include "entrada_multilinea.h"
#include "gc.h"
#include "control.h"

#define TAM_BUFFER_LECTURA 512

static unsigned char buffer_lectura[TAM_BUFFER_LECTURA];
static int pos_buffer = 0;
static int fin_buffer = 0;
static int fin_fichero = 0;
static unsigned char handle_actual;

/* rellena buffer_lectura desde el fichero cuando se agota */
static void rellenar_buffer(void) {
    fin_buffer = esxdos_f_read(handle_actual, buffer_lectura, TAM_BUFFER_LECTURA);
    pos_buffer = 0;
    if (fin_buffer <= 0) {
        fin_fichero = 1;
        fin_buffer = 0;
    }
}

/* lee una linea del fichero (sin '\n' ni '\r') en destino, troceando
   buffer_lectura a mano -- devuelve 1 si hay linea, 0 si no queda nada.
   Firma compatible con LectorLinea del modulo compartido. */
static int leer_linea_fichero(char *destino, int max) {
    int j = 0;
    unsigned char c;

    for (;;) {
        if (pos_buffer >= fin_buffer) {
            if (fin_fichero) break;
            rellenar_buffer();
            if (fin_buffer <= 0) break;
        }
        c = buffer_lectura[pos_buffer++];
        if (c == '\n') {
            destino[j] = '\0';
            return 1;
        }
        if (c != '\r' && j < max - 1) {
            destino[j++] = (char)c;
        }
    }
    if (j > 0) {
        destino[j] = '\0';
        return 1;   /* ultima linea del fichero, sin salto final */
    }
    return 0;
}

/* antes, load.c no comprobaba en absoluto si una linea (o el
   acumulado) superaba MAXLEN -- un bug latente heredado del strcpy/
   strcat sin proteccion. El modulo compartido si lo comprueba; esto
   solo informa cuando ocurre. */
static void reportar_error_largo(void) {
    INK(COLOR_ERROR);
    printf("%s\n", MSG_EXPRESION_LARGA);
    INK(COLOR_ENTRADA);
}

void DoLoad(const char *nombre) {
    int r;
    CellIdx expr, resultado;
    DIAG_PILA();

    errno = 0;
    handle_actual = esxdos_f_open(nombre, ESXDOS_MODE_OPEN_EXIST | ESXDOS_MODE_R);
    if (errno != 0) {
        INK(COLOR_ERROR);
        printf("%s%s\n", MSG_FICHERO_NO_ENCONTRADO, nombre);
        INK(COLOR_ENTRADA);
        return;
    }

    INK(COLOR_INFO);
    printf("%s%s\n", MSG_CARGANDO, nombre);
    INK(COLOR_ENTRADA);

    pos_buffer = 0;
    fin_buffer = 0;
    fin_fichero = 0;

    while ((r = LeerExpresionCompleta(leer_linea_fichero, NULL, NULL,
                                       reportar_error_largo)) == 1) {
        if (ConsultarInterrupcion()) break;
        ErrorFlag = 0;
        NextToken();
        if (LastType != TOK_EOF) {
            expr = ParseExpr();
            if (HeapError) break;
            if (!ErrorFlag) {
                EvalError = 0;
                resultado = Eval(expr, GlobalEnv);
                if (EjecucionCancelada || HeapError) break;
                if (!EvalError) {
                    if (!EsVoid(resultado)) {
                        INK(COLOR_RESULTADO);
                        PrintExpr(resultado);
                        printf("\n");
                        INK(COLOR_ENTRADA);
                    }
                }
                /* si EvalError esta activo, el mensaje especifico ya
                   se mostro dentro de Eval/Apply -- no hace falta
                   un aviso generico aqui encima */

                /* GC entre expresiones al 50%: deja margen para la siguiente.
                   No se recolecta dentro de constructores con temporales C. */
                if (DeberiaDispararseGC(UMBRAL_GC_LOAD)) {
                    CollectGarbage(T_CELL, GlobalEnv);
                }
            }
        }
    }

    esxdos_f_close(handle_actual);
    INK(COLOR_INFO);
    if (!EjecucionCancelada && !HeapError) printf("%s\n", MSG_CARGA_COMPLETADA);
    INK(COLOR_ENTRADA);
}