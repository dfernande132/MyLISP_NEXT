#include "diag_pila.h"
#include <stdio.h>
#include "printer.h"

static unsigned char nivelImpresion;

/* Impresion de un atomo. Funcion HOJA aparte: sus buffers locales
   (nombre/texto) ya no forman parte del marco de la funcion recursiva,
   que asi queda en unos pocos bytes por nivel. */
static void PrintAtomo(CellIdx idx) __z88dk_fastcall {
    CellIdx inner, numcell, dencell;
    char nombre[SYMLEN + 1];
    char texto[MAXSTRLEN + 1];

    switch (GetTag(idx)) {
        case TNIL:
            printf("NIL");
            break;

        case TSYM:
            SymtabGet(GetSymIdx(idx), nombre);
            printf("%s", nombre);
            break;

        case TSTRING:
            StrtabGet(GetStrIdx(idx), texto);
            printf("\"%s\"", texto);
            break;

        case TINT:
            printf("%ld", GetIntValue(idx));
            break;

        case TRAT:
            inner   = GetCar(idx);
            numcell = GetCar(inner);
            dencell = GetCdr(inner);
            printf("%ld/%ld", GetIntValue(numcell), GetIntValue(dencell));
            break;

        case TFLOAT:
#ifdef MYLISP_SIN_FLOTANTES
            printf("(flotante)");
#else
            printf("%.4f", GetFloatValue(idx));
#endif
            break;

        default:
            printf("?");
            break;
    }
}

void PrintExpr(CellIdx idx) __z88dk_fastcall {
    DIAG_PILA();

    if (GetTag(idx) != TPAIR) {
        PrintAtomo(idx);
        return;
    }
    if (nivelImpresion >= PRINT_PROFUNDIDAD_MAX) {
        printf("...");
        return;
    }
    nivelImpresion++;
    printf("(");
    PrintExpr(GetCar(idx));
    idx = GetCdr(idx);
    while (GetTag(idx) == TPAIR) {
        printf(" ");
        PrintExpr(GetCar(idx));
        idx = GetCdr(idx);
    }
    if (GetTag(idx) != TNIL) {
        printf(" . ");
        PrintAtomo(idx);
    }
    printf(")");
    nivelImpresion--;
}
