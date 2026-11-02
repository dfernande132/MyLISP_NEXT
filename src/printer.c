#include "diag_pila.h"
#include <stdio.h>
#include "printer.h"

void PrintExpr(CellIdx idx) __z88dk_fastcall {
    /* locales automaticos (NO static): PrintExpr es recursiva (caso
       TPAIR se llama a si misma), asi que cada invocacion necesita su
       propia copia de estas variables -- si fueran static, dos
       llamadas anidadas (una lista dentro de otra) compartirian la
       misma variable y se corromperian entre si. */
    CellIdx inner, numcell, dencell;
    char nombre[SYMLEN + 1];
    char texto[MAXSTRLEN + 1];
    DIAG_PILA();

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

        case TPAIR:
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
                PrintExpr(idx);
            }
            printf(")");
            break;

        default:
            printf("?");
            break;
    }
}