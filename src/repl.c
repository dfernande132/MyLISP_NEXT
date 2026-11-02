#include "diag_pila.h"
#include <intrinsic.h>
#include "teclado.h"
#include "cpu_next.h"
#include "control.h"
#include <stdio.h>
#include <conio.h>
#include <string.h>
#include <sys/ioctl.h>
#include "lexer.h"
#include "heap_ops.h"
#include "parser.h"
#include "printer.h"
#include "eval.h"
#include "load.h"
#include "messages.h"
#include "colors.h"
#include "entrada_multilinea.h"
#include "gc.h"
#ifdef MYLISP_BANCOS_CODIGO
#include "bancos_codigo.h"
#endif

extern unsigned char font_8x8_bbc_system[];

static void configurar_pantalla(void) {
    int modo = 66;   /* tilemap 80x32, con atributos de color */
    void *fuente = font_8x8_bbc_system;

    console_ioctl(IOCTL_GENCON_SET_MODE, &modo);
    console_ioctl(IOCTL_GENCON_SET_FONT32, &fuente);
}

static int es_comando(const char *s, const char *comando) {
    const char *p = s;
    static char buf[16];
    int i = 0;
    while (*p && i < 15) {
        char c = *p;
        if (c >= 'a' && c <= 'z') c = (char)(c - 32);
        buf[i++] = c;
        p++;
    }
    buf[i] = '\0';
    return strcmp(buf, comando) == 0;
}

static int es_comando_salida(const char *s) {
    return es_comando(s, "BYE");
}

/* --- callbacks para el modulo compartido de entrada multilinea --- */

static int leer_teclado(char *destino, int max) {
    return LeerTeclado(destino, max);
}

static void anunciar_prompt(int continuando) {
    INK(COLOR_PROMPT);
    printf("%s", continuando ? MSG_PROMPT_CONTINUA : MSG_PROMPT_REPL);
    INK(COLOR_ENTRADA);
}

static void reportar_error_largo(void) {
    INK(COLOR_ERROR);
    printf("%s\n", MSG_EXPRESION_LARGA);
}

static void print_banner(void) {
    configurar_pantalla();
    PAPER(COLOR_FONDO);
    TecladoInit();
    clrscr();

#ifdef MYLISP_BANCOS_CODIGO
    INK(COLOR_TITULO);
    CodigoBanner();
#endif
    INK(COLOR_VERSION);
    printf("\n%s\n", MSG_TITULO);
    printf("%s\n", MSG_VERSION);

    INK(COLOR_COPYRIGHT);
    printf("%s\n\n", MSG_COPYRIGHT);

    INK(COLOR_SEPARADOR);
    puts("  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+");
    INK(COLOR_INSTRUC);
    printf("\n%s\n", MSG_ESCRIBE_SALIR);
    puts("  Arrows: move/start/end. DELETE: backspace. Esc: cancel.\n");
}

int main(void) {
    CpuIniciar();
#ifdef MYLISP_BANCOS_CODIGO
    CodigoInit();
#endif
    intrinsic_ei(); /* FRAMES y repeticion del teclado. */
    print_banner();
    CpuInformeArranque();
    heap_ops_init();
    parser_init();
    eval_init();
#ifdef MYLISP_DIAG_PILA
    puts("DIAG PILA: MEM / MEMRESET");
    DiagIniciar();
#endif

    for (;;) {
        CellIdx root;
        int r;

        r = LeerExpresionCompleta(leer_teclado, es_comando_salida,
                                   anunciar_prompt, reportar_error_largo);
        if (r <= 0) break;   /* 0 = EOF real, -1 = BYE */
        if (es_comando(Line, "HELP")) {
            CodigoAyuda();
            continue;
        }

#ifdef MYLISP_DIAG_PILA
        if (strcmp(Line, "MEM") == 0) {
            DiagInforme();
            DiagIniciar();
            continue;
        }
        if (strcmp(Line, "MEMRESET") == 0) {
            DiagIniciar();
            continue;
        }
#endif
        ErrorFlag = 0;
        NextToken();
        if (LastType == TOK_EOF) continue;

        root = ParseExpr();

        if (HeapError) {
            RecuperarRecursos();
            continue;
        }
        if (ErrorFlag) {
            /* el mensaje especifico ya se mostro en el propio lexer/
               parser (error lexico, o expresion incompleta) -- no
               hace falta un aviso generico aqui encima */
            continue;
        }
        if (LastType != TOK_EOF) {
            INK(COLOR_ERROR);
            printf("%s%s\n", MSG_ERROR_LEXICO, LastText);
            continue;
        }

        EvalError = 0;
        {
            CellIdx resultado = Eval(root, GlobalEnv);
            if (HeapError) {
                RecuperarRecursos();
                continue;
            }
            if (NuevoPendiente) {
                NuevoPendiente = 0;
                heap_ops_init(); parser_init(); eval_init();
                continue;
            }
            if (!EvalError) {
                if (!EsVoid(resultado)) {
                    INK(COLOR_RESULTADO);
                    PrintExpr(resultado);
                    printf("\n");
                }
            }
            /* si EvalError esta activo, el mensaje especifico ya se
               mostro dentro de Eval/Apply -- no hace falta reserva */
        }

        /* GC automatico, silencioso, entre iteraciones del REPL --
           NUNCA a mitad de una evaluacion (aqui ya ha terminado del
           todo). Umbral 80%, igual que en MYLISP.pas. */
        if (DeberiaDispararseGC(UMBRAL_GC_REPL)) {
            CollectGarbage(T_CELL, GlobalEnv);
        }
    }

    INK(COLOR_INFO);
    printf("\n%s\n", MSG_HASTA_PRONTO);
    for (;;);
}