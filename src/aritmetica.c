/* Aritmetica exacta (Etapa B): +, -, *, / y comparadores numericos.
   Modulo propio y banco propio (pagina 92): en la pagina 90 no cabe junto a
   eval.c. Solo usa funciones residentes (heap, printf), sin recursion. */
#include <stdio.h>
#include "heap_ops.h"
#include "control.h"
#include "colors.h"
#include "messages.h"
#include "eval.h"
#include "aritmetica.h"

#include "racional.c"

/* Error de evaluacion con mensaje: un solo sitio en vez de repetir las
   tres lineas en cada comprobacion de la aritmetica. */
static void ErrorEval(const char *msg) __z88dk_fastcall {
    EvalError = 1;
    INK(COLOR_ERROR);
    printf("%s\n", msg);
}

/* Numero = TINT, TRAT o (si hay flotantes) TFLOAT. */
static int EsNumerico(CellIdx c) __z88dk_fastcall {
    unsigned char t = GetTag(c);
#ifndef MYLISP_SIN_FLOTANTES
    if (t == TFLOAT) return 1;
#endif
    return t == TINT || t == TRAT;
}

#ifndef MYLISP_SIN_FLOTANTES
/* Valor aproximado de un numero cualquiera, solo para cuando la operacion
   ya es flotante. */
static float ValorComoFloat(CellIdx c) __z88dk_fastcall {
    Racional r;
    if (GetTag(c) == TFLOAT) return GetFloatValue(c);
    RacDeCelda(c, &r);
    return RacComoFloat(&r);
}
#endif

/* 1 si el numero vale cero (un TRAT normalizado nunca es cero). */
static int EsCeroNumerico(CellIdx c) __z88dk_fastcall {
#ifndef MYLISP_SIN_FLOTANTES
    if (GetTag(c) == TFLOAT) return GetFloatValue(c) == 0.0f;
#endif
    return GetTag(c) == TINT && GetIntValue(c) == 0;
}


/* Aritmetica (Etapa B, mismos resultados que MEGA65 -- decisiones 8.3):
   dos dominios, el exacto (Racional; el entero es den == 1) y el flotante.
   Una primera pasada valida los tipos y detecta si hay algun TFLOAT; si lo
   hay, toda la operacion es flotante (el flotante es de ENTRADA, nunca una
   promocion automatica); si no, es exacta y el desbordamiento de 32 bits
   levanta `ERR: desbordamiento` antes de devolver un numero incorrecto.
   Una sola funcion para las cuatro operaciones: en el Z80 cada copia del
   bucle cuesta cientos de bytes de la pagina 90, que esta casi llena. */
CellIdx Aritmetica(CellIdx args, int op) {
    Racional acc;
    Racional v;
    Racional tmp;
    CellIdx lista;
    int hayFloat = 0;
    int conPrimero = (op == OP_RESTA || op == OP_DIV);
    int ok;
#ifndef MYLISP_SIN_FLOTANTES
    float facc;
#endif

    if (conPrimero && GetTag(args) != TPAIR) {
        ErrorEval(MSG_REQUIERE_ARGUMENTOS);
        return NIL_CELL;
    }

    /* pasada 1: tipos, flotantes y divisores cero (en el orden de los args) */
    for (lista = args; GetTag(lista) == TPAIR; lista = GetCdr(lista)) {
        CellIdx c = GetCar(lista);
        if (!EsNumerico(c)) {
            ErrorEval(MSG_ESPERABA_ENTERO);
            return NIL_CELL;
        }
#ifndef MYLISP_SIN_FLOTANTES
        if (GetTag(c) == TFLOAT) hayFloat = 1;
#endif
        if (op == OP_DIV && lista != args && EsCeroNumerico(c)) {
            ErrorEval(MSG_DIVISION_CERO);
            return NIL_CELL;
        }
    }

    lista = args;
    if (conPrimero) lista = GetCdr(args);

#ifndef MYLISP_SIN_FLOTANTES
    if (hayFloat) {
        if (conPrimero) {
            facc = ValorComoFloat(GetCar(args));
            if (op == OP_RESTA && GetTag(lista) != TPAIR) return MakeFloat(-facc);
        } else {
            facc = (op == OP_MULT) ? 1.0f : 0.0f;
        }
        for (; GetTag(lista) == TPAIR; lista = GetCdr(lista)) {
            float f = ValorComoFloat(GetCar(lista));
            if (op == OP_SUMA)       facc += f;
            else if (op == OP_RESTA) facc -= f;
            else if (op == OP_MULT)  facc *= f;
            else                     facc /= f;
        }
        return MakeFloat(facc);
    }
#else
    (void)hayFloat;
#endif

    if (conPrimero) {
        RacDeCelda(GetCar(args), &acc);
        if (op == OP_RESTA && GetTag(lista) != TPAIR) {
            /* negacion unaria; (/ x) con un solo argumento devuelve x
               (comportamiento heredado) */
            acc.num = -acc.num;
            return RacACelda(&acc);
        }
    } else {
        acc.num = (op == OP_MULT) ? 1 : 0;
        acc.den = 1;
    }

    for (; GetTag(lista) == TPAIR; lista = GetCdr(lista)) {
        RacDeCelda(GetCar(lista), &v);
        if (op == OP_SUMA)       ok = RacSumarConSigno(&acc, &v, 0, &tmp);
        else if (op == OP_RESTA) ok = RacSumarConSigno(&acc, &v, 1, &tmp);
        else if (op == OP_MULT)  ok = RacMultiplicarPartes(acc.num, acc.den, v.num, v.den, &tmp);
        else                     ok = RacMultiplicarPartes(acc.num, acc.den, v.den, v.num, &tmp);
        if (!ok) {
            ErrorEval(MSG_DESBORDAMIENTO);
            return NIL_CELL;
        }
        acc.num = tmp.num;
        acc.den = tmp.den;
    }
    return RacACelda(&acc);
}

/* Compara dos numeros: deja -1/0/1 en *cmp y devuelve 1; 0 si hubo
   error (ya mostrado). Con algun flotante se compara en flotante; si no,
   EXACTO por producto cruzado -- (= 1/3 0.3333) tiene que dar NIL. Si el
   producto cruzado desborda se avisa en vez de caer a flotante. Funcion
   aparte para que los dos Racional no engorden el marco de AplicarBuiltin. */
int CompararNumeros(CellIdx ca, CellIdx cb, int *cmp) {
    if (!EsNumerico(ca) || !EsNumerico(cb)) {
        ErrorEval(MSG_ESPERABA_ENTERO);
        return 0;
    }
#ifndef MYLISP_SIN_FLOTANTES
    if (GetTag(ca) == TFLOAT || GetTag(cb) == TFLOAT) {
        float a = ValorComoFloat(ca);
        float b = ValorComoFloat(cb);
        *cmp = (a < b) ? -1 : (a > b ? 1 : 0);
        return 1;
    }
#endif
    {
        Racional a, b;
        RacDeCelda(ca, &a);
        RacDeCelda(cb, &b);
        if (!RacComparar(&a, &b, cmp)) {
            ErrorEval(MSG_DESBORDAMIENTO);
            return 0;
        }
    }
    return 1;
}

