#include "diag_pila.h"
#include "teclado.h"
#include "control.h"
#include "cpu_next.h"
#include <stdio.h>
#include <string.h>
#include "eval.h"
#include "lexer.h"
#include "parser.h"
#include "load.h"
#include "printer.h"
#include "messages.h"
#include "colors.h"
#include "gc.h"
#include "eval_iterativo.h"
#include "pila_tareas.h"

int EvalError = 0;
CellIdx GlobalEnv;
CellIdx T_CELL;

static unsigned int SYM_T, SYM_NIL, SYM_HELP;
static unsigned int SYM_PLUS, SYM_MINUS, SYM_TIMES, SYM_DIVIDE;
static unsigned int SYM_EQUALS, SYM_LT, SYM_GT, SYM_LE, SYM_GE;
/* Grupo A: primitivas de listas */
static unsigned int SYM_CAR, SYM_CDR, SYM_CONS, SYM_ATOM, SYM_EQ, SYM_NOT;
static unsigned int SYM_LIST, SYM_APPEND, SYM_EQUAL, SYM_NULL;
static unsigned int SYM_EVAL;
/* Grupo B: predicados de tipo */
static unsigned int SYM_NUMBERP, SYM_SYMBOLP, SYM_LISTP;
/* Grupo D: entrada/salida */
static unsigned int SYM_PRINT, SYM_DISPLAY, SYM_NEWLINE;
unsigned int SYM_VOID;   /* no static: repl.c/load.c necesitan comprobarlo */
/* Grupo F: aritmetica que falta */
static unsigned int SYM_DIV, SYM_MOD;
/* Grupo E: strings/simbolos */
static unsigned int SYM_SYMNAME, SYM_STRLT, SYM_STRCAT;
/* NOTA: SYM_IF/COND/DEFINE/LAMBDA/LOAD/AND/OR/PROGN/LET/DEFUN/STATUS/
   CLEAN/NEW/SYMBOLS ya NO se declaran aqui -- solo los usaba la
   maquinaria recursiva de formas especiales, ahora borrada porque
   EvalIterativo (eval_iterativo.c) la reimplementa por su cuenta,
   LOAD incluido, con su propio cache de simbolos. */

CellIdx EvalStatus(void);
CellIdx EvalClean(void);
CellIdx EvalNew(void);
CellIdx EvalSymbols(void);
int EsBuiltin(unsigned int sym);
CellIdx AplicarBuiltin(unsigned int sym, CellIdx args);
static CellIdx AppendLists(CellIdx a, CellIdx b);
static int EqualStruct(CellIdx a, CellIdx b);
static int NodeToStr(CellIdx idx, char *out);
static int ObtenerTextoEstricto(CellIdx c, char *out);

#ifndef MYLISP_SIN_FLOTANTES
/* Aritmetica mixta entero/flotante: promociona a float solo cuando
   hace falta, dejando el camino entero puro intacto (sin pasar por
   aqui) cuando todos los operandos son TINT -- para no arriesgar
   precision ni tocar el comportamiento ya probado con enteros. */
static float ValorComoFloat(CellIdx c) __z88dk_fastcall {
    return (GetTag(c) == TFLOAT) ? GetFloatValue(c) : (float)GetIntValue(c);
}
static int EsNumerico(CellIdx c) __z88dk_fastcall {
    unsigned char t = GetTag(c);
    return t == TINT || t == TFLOAT;
}
#endif

void eval_init(void) {
    EvalError = 0;
    PilaTareasInit();   /* una sola vez -- EvalIterativo YA NO la llama
                            (romperia las llamadas anidadas de LOAD/EVAL) */
    EvalIterativoReset();   /* invalida la cache de simbolos especiales --
                                imprescindible tras (NEW), que reinterna
                                todo desde cero con indices distintos */

    SYM_HELP   = InternSym("HELP");
    SYM_T      = InternSym("T");
    SYM_NIL    = InternSym("NIL");

    SYM_PLUS   = InternSym("+");
    SYM_MINUS  = InternSym("-");
    SYM_TIMES  = InternSym("*");
    SYM_DIVIDE = InternSym("/");
    SYM_EQUALS = InternSym("=");
    SYM_LT     = InternSym("<");
    SYM_GT     = InternSym(">");
    SYM_LE     = InternSym("<=");
    SYM_GE     = InternSym(">=");

    SYM_CAR    = InternSym("CAR");
    SYM_CDR    = InternSym("CDR");
    SYM_CONS   = InternSym("CONS");
    SYM_ATOM   = InternSym("ATOM");
    SYM_EQ     = InternSym("EQ");
    SYM_NOT    = InternSym("NOT");
    SYM_LIST   = InternSym("LIST");
    SYM_APPEND = InternSym("APPEND");
    SYM_EQUAL  = InternSym("EQUAL");
    SYM_NULL   = InternSym("NULL");

    SYM_EVAL   = InternSym("EVAL");

    SYM_NUMBERP = InternSym("NUMBERP");
    SYM_SYMBOLP = InternSym("SYMBOLP");
    SYM_LISTP   = InternSym("LISTP");

    SYM_PRINT   = InternSym("PRINT");
    SYM_DISPLAY = InternSym("DISPLAY");
    SYM_NEWLINE = InternSym("NEWLINE");
    SYM_VOID    = InternSym("VOID");

    SYM_DIV = InternSym("DIV");
    SYM_MOD = InternSym("MOD");

    SYM_SYMNAME = InternSym("SYMNAME");
    SYM_STRLT   = InternSym("STR<");
    SYM_STRCAT  = InternSym("STRCAT");

    T_CELL = MakeSym(SYM_T);

    /* GlobalEnv se crea UNA VEZ; a partir de aqui solo se muta en el
       sitio (DefineGlobal), nunca se reasigna -- igual que en Pascal */
    GlobalEnv = Cons(NIL_CELL, NIL_CELL);
}

int IsTruthy(CellIdx v) __z88dk_fastcall {
    return GetTag(v) != TNIL;
}

int EsVoid(CellIdx v) __z88dk_fastcall {
    return GetTag(v) == TSYM && GetSymIdx(v) == SYM_VOID;
}

CellIdx EnvLookup(CellIdx env, unsigned int symidx) {
    static CellIdx frame, binding, symcell;
    static char nombre[SYMLEN + 1];

    while (GetTag(env) == TPAIR) {
        frame = GetCar(env);
        while (GetTag(frame) == TPAIR) {
            binding = GetCar(frame);
            symcell = GetCar(binding);
            if (GetTag(symcell) == TSYM && GetSymIdx(symcell) == symidx) {
                return GetCdr(binding);
            }
            frame = GetCdr(frame);
        }
        env = GetCdr(env);
    }
    EvalError = 1;
    INK(COLOR_ERROR_DESTACADO);
    SymtabGet(symidx, nombre);
    printf("%s%s\n", MSG_SIMBOLO_NO_LIGADO, nombre);
    INK(COLOR_ERROR);
    return NIL_CELL;
}

CellIdx ExtendEnv(CellIdx params, CellIdx args, CellIdx env) {
    CellIdx frame = NIL_CELL;
    CellIdx binding;

    while (GetTag(params) == TPAIR) {
        if (GetTag(args) != TPAIR) {
            EvalError = 1;
            INK(COLOR_ERROR_DESTACADO);
            printf("%s\n", MSG_ARGUMENTOS_INCORRECTOS);
            INK(COLOR_ERROR);
            return NIL_CELL;
        }
        binding = Cons(GetCar(params), GetCar(args));
        frame = Cons(binding, frame);
        params = GetCdr(params);
        args = GetCdr(args);
    }
    if (GetTag(args) == TPAIR) {
        EvalError = 1;
        INK(COLOR_ERROR_DESTACADO);
        printf("%s\n", MSG_ARGUMENTOS_INCORRECTOS);
        INK(COLOR_ERROR);
        return NIL_CELL;
    }
    return Cons(frame, env);
}

void DefineGlobal(unsigned int symidx, CellIdx valor) {
    CellIdx binding = Cons(MakeSym(symidx), valor);
    CellIdx frameActual = GetCar(GlobalEnv);
    CellIdx frameNuevo = Cons(binding, frameActual);
    if (!HeapError) SetCar(GlobalEnv, frameNuevo);
}

/* Eval() es ahora un envoltorio de una linea sobre EvalIterativo()
   (eval_iterativo.c) -- todo EvalInterno/EvalIf/EvalCondForm/
   EvalDefine/EvalLambda/EvalAnd/EvalOr/EvalLet/EvalDefun/EvalBody se
   han BORRADO de aqui porque EvalIterativo ya reimplementa todo eso
   por su cuenta, con su propio mecanismo de tareas paginadas -- ya no
   dependemos de la pila de C para la recursion, asi que tampoco hace
   falta el chequeo de SP_FLOOR_SEGURO que tenia esta funcion antes.

   LOAD usa el evaluador reentrante; el lector de ficheros conserva
   estado compartido y LOAD anidado sigue pendiente. */
CellIdx Eval(CellIdx expr, CellIdx env) {
    return EvalIterativo(expr, env);
}

/* (STATUS) -- muestra el estado del heap. Sin coma flotante activa,
   no hay linea de "reales" (esa parte de MYLISP.pas no aplica ahora). */
CellIdx EvalStatus(void) {
    INK(COLOR_INFO);
    printf("%s%u/%u (%ld%%)\n", MSG_STATUS_HEAP, HeapNextFree(), (unsigned int)MAXCELL,
           LiveUsagePercent());
    printf("%s%u/%d\n", MSG_STATUS_SIMBOLOS, nsyms, MAXSYM);
    printf("%s%u/%d\n", MSG_STATUS_STRINGS, nstrs, MAXSTR);
    CpuInformeActual();
    INK(COLOR_ENTRADA);
    return MakeSym(SYM_VOID);
}

/* (CLEAN): el GC conserva ahora las referencias de todas las tareas,
   incluidos los entornos locales de LET y contextos EVAL/LOAD.
   El marcado de estructuras profundamente anidadas aun consume pila. */
CellIdx EvalClean(void) {
    INK(COLOR_GC);
    printf("%s%ld%% nstrs=%u\n", MSG_CLEAN_ANTES, LiveUsagePercent(), nstrs);
    CollectGarbage(T_CELL, GlobalEnv);
    printf("%s%ld%% nstrs=%u\n", MSG_CLEAN_DESPUES, LiveUsagePercent(), nstrs);
    INK(COLOR_ENTRADA);
    return MakeSym(SYM_VOID);
}

/* (NEW) -- solicita al REPL reiniciar heap, simbolos y GlobalEnv, pidiendo
   confirmacion primero. Equivale a MYLISP.pas InitHeap, que reinterna
   los simbolos builtin justo despues de resetear nsyms -- por eso
   el REPL ejecuta los tres init despues de retirar las tareas activas. */
CellIdx EvalNew(void) {
    char confirm[16];
    if (!PermitirNew) {
        EvalError = 1; INK(COLOR_ERROR);
        puts("ERR: use (NEW) directly at the prompt");
        return NIL_CELL;
    }

    INK(COLOR_INFO);
    printf("%s", MSG_NEW_CONFIRMAR);
    INK(COLOR_ENTRADA);
    if (LeerTeclado(confirm, sizeof(confirm) - 1) == 1 &&
        (confirm[0] == 's' || confirm[0] == 'S' ||
         confirm[0] == 'y' || confirm[0] == 'Y')) {
        INK(COLOR_INFO);
        printf("%s\n", MSG_NEW_REINICIANDO);
        NuevoPendiente = 1;
    } else {
        INK(COLOR_INFO);
        printf("%s\n", MSG_NEW_CANCELADO);
    }
    INK(COLOR_ENTRADA);
    return MakeSym(SYM_VOID);
}

/* (SYMBOLS) -- lista los simbolos definidos por el usuario (los que
   estan ligados en GlobalEnv) -- NO los builtins/formas especiales,
   que no viven ahi, se reconocen por comparacion directa en Eval. */
CellIdx EvalSymbols(void) {
    CellIdx frame = GetCar(GlobalEnv);
    CellIdx binding, symcell;
    char nombre[SYMLEN + 1];

    INK(COLOR_INFO);
    while (GetTag(frame) == TPAIR) {
        binding = GetCar(frame);
        symcell = GetCar(binding);
        if (GetTag(symcell) == TSYM) {
            SymtabGet(GetSymIdx(symcell), nombre);
            printf("%s\n", nombre);
        }
        frame = GetCdr(frame);
    }
    INK(COLOR_ENTRADA);
    return MakeSym(SYM_VOID);
}

int EsBuiltin(unsigned int sym) {
    if (sym == SYM_HELP) return 1;
    return sym == SYM_PLUS || sym == SYM_MINUS || sym == SYM_TIMES ||
           sym == SYM_DIVIDE || sym == SYM_EQUALS || sym == SYM_LT ||
           sym == SYM_GT || sym == SYM_LE || sym == SYM_GE ||
           sym == SYM_CAR || sym == SYM_CDR || sym == SYM_CONS ||
           sym == SYM_ATOM || sym == SYM_EQ || sym == SYM_NOT ||
           sym == SYM_LIST || sym == SYM_APPEND || sym == SYM_EQUAL ||
           sym == SYM_NULL || sym == SYM_EVAL ||
           sym == SYM_NUMBERP || sym == SYM_SYMBOLP || sym == SYM_LISTP ||
           sym == SYM_PRINT || sym == SYM_DISPLAY || sym == SYM_NEWLINE ||
           sym == SYM_DIV || sym == SYM_MOD ||
           sym == SYM_SYMNAME || sym == SYM_STRLT || sym == SYM_STRCAT;
}

#define OP_SUMA  0
#define OP_RESTA 1
#define OP_MULT  2
#define OP_DIV   3

/* +  y  x  recorren TODOS los argumentos desde el principio, sin
   tratamiento especial del primero -- comparten esta funcion, solo
   cambia el operador (suma/producto) y el valor inicial. */
static CellIdx AritAcumulativa(CellIdx args, int op) {
    long acumulador = (op == OP_MULT) ? 1 : 0;
#ifndef MYLISP_SIN_FLOTANTES
    float facc = (op == OP_MULT) ? 1.0f : 0.0f;
    int hayFloat = 0;
#endif
    while (GetTag(args) == TPAIR) {
        CellIdx c = GetCar(args);
#ifndef MYLISP_SIN_FLOTANTES
        if (!EsNumerico(c)) {
#else
        if (GetTag(c) != TINT) {
#endif
            EvalError = 1;
            INK(COLOR_ERROR);
            printf("%s\n", MSG_ESPERABA_ENTERO);
            return NIL_CELL;
        }
#ifndef MYLISP_SIN_FLOTANTES
        if (GetTag(c) == TFLOAT) {
            hayFloat = 1;
            if (op == OP_MULT) facc *= GetFloatValue(c); else facc += GetFloatValue(c);
        } else {
            long v = GetIntValue(c);
            if (op == OP_MULT) { acumulador *= v; facc *= (float)v; }
            else { acumulador += v; facc += (float)v; }
        }
#else
        {
            long v = GetIntValue(c);
            if (op == OP_MULT) acumulador *= v; else acumulador += v;
        }
#endif
        args = GetCdr(args);
    }
#ifndef MYLISP_SIN_FLOTANTES
    if (hayFloat) return MakeFloat(facc);
#endif
    return MakeInt(acumulador);
}

/* -  y  /  tratan el primer argumento aparte (para la negacion/
   division unaria) y luego recorren el resto -- comparten esta
   funcion, solo cambia el operador (resta/division). */
static CellIdx AritConPrimero(CellIdx args, int op) {
    CellIdx primero, resto;
    long acumulador;
#ifndef MYLISP_SIN_FLOTANTES
    float facc;
    int hayFloat;
#endif

    if (GetTag(args) != TPAIR) {
        EvalError = 1;
        INK(COLOR_ERROR);
        printf("%s\n", MSG_REQUIERE_ARGUMENTOS);
        return NIL_CELL;
    }
    primero = GetCar(args);
    resto = GetCdr(args);
#ifndef MYLISP_SIN_FLOTANTES
    if (!EsNumerico(primero)) {
#else
    if (GetTag(primero) != TINT) {
#endif
        EvalError = 1;
        INK(COLOR_ERROR);
        printf("%s\n", MSG_ESPERABA_ENTERO);
        return NIL_CELL;
    }

    acumulador = GetIntValue(primero);
#ifndef MYLISP_SIN_FLOTANTES
    facc = ValorComoFloat(primero);
    hayFloat = (GetTag(primero) == TFLOAT);
#endif

    if (op == OP_RESTA && GetTag(resto) != TPAIR) {
        /* negacion unaria: solo para resta con un solo argumento --
           (/ x) con un solo argumento cae en el bucle de abajo, que
           no hace nada, y devuelve x sin cambios (igual que antes) */
#ifndef MYLISP_SIN_FLOTANTES
        return hayFloat ? MakeFloat(-facc) : MakeInt(-acumulador);
#else
        return MakeInt(-acumulador);
#endif
    }

    while (GetTag(resto) == TPAIR) {
        CellIdx c = GetCar(resto);
#ifndef MYLISP_SIN_FLOTANTES
        if (!EsNumerico(c)) {
#else
        if (GetTag(c) != TINT) {
#endif
            EvalError = 1;
            INK(COLOR_ERROR);
            printf("%s\n", MSG_ESPERABA_ENTERO);
            return NIL_CELL;
        }
        if (op == OP_DIV) {
#ifndef MYLISP_SIN_FLOTANTES
            if (ValorComoFloat(c) == 0.0f) {
#else
            if (GetIntValue(c) == 0) {
#endif
                EvalError = 1;
                INK(COLOR_ERROR);
                printf("%s\n", MSG_DIVISION_CERO);
                return NIL_CELL;
            }
        }
#ifndef MYLISP_SIN_FLOTANTES
        if (GetTag(c) == TFLOAT) {
            hayFloat = 1;
            if (op == OP_DIV) facc /= GetFloatValue(c); else facc -= GetFloatValue(c);
        } else {
            long v = GetIntValue(c);
            if (op == OP_DIV) { acumulador /= v; facc /= (float)v; }
            else { acumulador -= v; facc -= (float)v; }
        }
#else
        {
            long v = GetIntValue(c);
            if (op == OP_DIV) acumulador /= v; else acumulador -= v;
        }
#endif
        resto = GetCdr(resto);
    }
#ifndef MYLISP_SIN_FLOTANTES
    return hayFloat ? MakeFloat(facc) : MakeInt(acumulador);
#else
    return MakeInt(acumulador);
#endif
}

CellIdx AplicarBuiltin(unsigned int sym, CellIdx args) {
    if (sym == SYM_HELP) {
        if (GetTag(args) != TNIL) {
            EvalError = 1; INK(COLOR_ERROR);
            puts("ERR: HELP takes no arguments");
            return NIL_CELL;
        }
        CodigoAyuda();
        return MakeSym(SYM_VOID);
    }
    if (sym == SYM_PLUS)   return AritAcumulativa(args, OP_SUMA);
    if (sym == SYM_TIMES)  return AritAcumulativa(args, OP_MULT);
    if (sym == SYM_MINUS)  return AritConPrimero(args, OP_RESTA);
    if (sym == SYM_DIVIDE) return AritConPrimero(args, OP_DIV);

    /* --- Grupo A: primitivas de listas --- */

    if (sym == SYM_CAR || sym == SYM_CDR) {
        if (GetTag(GetCar(args)) != TPAIR) {
            EvalError = 1;
            INK(COLOR_ERROR);
            printf("%s\n", MSG_REQUIERE_LISTA);
            return NIL_CELL;
        }
        return (sym == SYM_CAR) ? GetCar(GetCar(args)) : GetCdr(GetCar(args));
    }

    if (sym == SYM_CONS) {
        return Cons(GetCar(args), GetCar(GetCdr(args)));
    }

    if (sym == SYM_ATOM) {
        return (GetTag(GetCar(args)) == TPAIR) ? NIL_CELL : T_CELL;
    }

    if (sym == SYM_EQ) {
        CellIdx a = GetCar(args);
        CellIdx b = GetCar(GetCdr(args));
        if (a == b) return T_CELL;
        if (GetTag(a) == TSYM && GetTag(b) == TSYM &&
            GetSymIdx(a) == GetSymIdx(b)) {
            return T_CELL;
        }
        return NIL_CELL;
    }

    if (sym == SYM_NOT) {
        return IsTruthy(GetCar(args)) ? NIL_CELL : T_CELL;
    }

    if (sym == SYM_LIST) {
        /* args ya es la lista de argumentos evaluados: LIST es la
           identidad sobre esa lista */
        return args;
    }

    if (sym == SYM_APPEND) {
        return AppendLists(GetCar(args), GetCar(GetCdr(args)));
    }

    if (sym == SYM_EQUAL) {
        return EqualStruct(GetCar(args), GetCar(GetCdr(args))) ? T_CELL : NIL_CELL;
    }

    if (sym == SYM_NULL) {
        return (GetTag(GetCar(args)) == TNIL) ? T_CELL : NIL_CELL;
    }

    if (sym == SYM_EVAL) {
        /* (EVAL expr) -- el argumento ya viene evaluado una vez (es
           el dato-expresion); lo evaluamos otra vez en GlobalEnv para
           metaprogramacion: construir expresiones como datos y
           evaluarlas despues */
        return Eval(GetCar(args), GlobalEnv);
    }

    /* --- Grupo B: predicados de tipo --- */

    if (sym == SYM_NUMBERP) {
        unsigned char t = GetTag(GetCar(args));
        return (t == TINT || t == TRAT || t == TFLOAT) ? T_CELL : NIL_CELL;
    }

    if (sym == SYM_SYMBOLP) {
        return (GetTag(GetCar(args)) == TSYM) ? T_CELL : NIL_CELL;
    }

    if (sym == SYM_LISTP) {
        unsigned char t = GetTag(GetCar(args));
        return (t == TPAIR || t == TNIL) ? T_CELL : NIL_CELL;
    }

    /* --- Grupo D: entrada/salida ---
       DISPLAY/NEWLINE devuelven el simbolo VOID, que el REPL y LOAD
       reconocen para no imprimir el eco del resultado (evita el
       doble-print cuando la propia forma ya imprimio algo). */

    if (sym == SYM_PRINT) {
        /* (PRINT expr) imprime y devuelve el mismo valor, util para
           trazas de depuracion dentro de recursion */
        INK(COLOR_RESULTADO);
        PrintExpr(GetCar(args));
        printf("\n");
        return GetCar(args);
    }

    if (sym == SYM_DISPLAY) {
        /* igual que PrintExpr tipo por tipo, salvo que TSTRING se
           muestra sin comillas -- por eso no reutilizamos PrintExpr
           directamente para ese caso */
        CellIdx val = GetCar(args);
        INK(COLOR_RESULTADO);
        if (GetTag(val) == TSTRING) {
            char texto[MAXSTRLEN + 1];
            StrtabGet(GetStrIdx(val), texto);
            printf("%s", texto);
        } else {
            PrintExpr(val);
        }
        return MakeSym(SYM_VOID);
    }

    if (sym == SYM_NEWLINE) {
        printf("\n");
        return MakeSym(SYM_VOID);
    }

    /* --- Grupo F: aritmetica entera que faltaba --- */

    if (sym == SYM_DIV || sym == SYM_MOD) {
        long a, b;
        if (GetTag(GetCar(args)) != TINT || GetTag(GetCar(GetCdr(args))) != TINT) {
            EvalError = 1;
            INK(COLOR_ERROR);
            printf("%s\n", MSG_ESPERABA_ENTERO);
            return MakeInt(0);
        }
        a = GetIntValue(GetCar(args));
        b = GetIntValue(GetCar(GetCdr(args)));
        if (b == 0) {
            EvalError = 1;
            INK(COLOR_ERROR);
            printf("%s\n", MSG_DIVISION_CERO);
            return MakeInt(0);
        }
        return MakeInt((sym == SYM_DIV) ? (a / b) : (a % b));
    }

    /* --- Grupo E: strings/simbolos --- */

    if (sym == SYM_SYMNAME) {
        char nombre[SYMLEN + 1];
        if (GetTag(GetCar(args)) != TSYM) {
            EvalError = 1;
            INK(COLOR_ERROR);
            printf("%s\n", MSG_ESPERABA_SIMBOLO);
            return NIL_CELL;
        }
        SymtabGet(GetSymIdx(GetCar(args)), nombre);
        return MakeString(nombre);
    }

    if (sym == SYM_STRLT) {
        char sa[MAXSTRLEN + 1];
        char sb[MAXSTRLEN + 1];

        if (!ObtenerTextoEstricto(GetCar(args), sa) ||
            !ObtenerTextoEstricto(GetCar(GetCdr(args)), sb)) {
            EvalError = 1;
            INK(COLOR_ERROR);
            printf("%s\n", MSG_ESPERABA_SIMBOLO);
            return NIL_CELL;
        }
        return (strcmp(sa, sb) < 0) ? T_CELL : NIL_CELL;
    }

    if (sym == SYM_STRCAT) {
        char sa[MAXSTRLEN + 1];
        char sb[MAXSTRLEN + 1];
        char resultado[2 * (MAXSTRLEN + 1)];

        if (!NodeToStr(GetCar(args), sa) || !NodeToStr(GetCar(GetCdr(args)), sb)) {
            EvalError = 1;
            INK(COLOR_ERROR);
            printf("%s\n", MSG_ESPERABA_SIMBOLO);
            return NIL_CELL;
        }
        strcpy(resultado, sa);
        strcat(resultado, sb);
        if ((int)strlen(resultado) > STRLEN) {
            resultado[STRLEN] = '\0';
        }
        return MakeString(resultado);
    }

    /* --- comparadores: binarios, no variadicos --- */
    if (GetTag(args) != TPAIR || GetTag(GetCdr(args)) != TPAIR) {
        EvalError = 1;
        INK(COLOR_ERROR);
        printf("%s\n", MSG_ARGUMENTOS_INCORRECTOS);
        return NIL_CELL;
    }
    {
        int cmp;
#ifndef MYLISP_SIN_FLOTANTES
        if (GetTag(GetCar(args)) == TFLOAT || GetTag(GetCar(GetCdr(args))) == TFLOAT) {
            float a = ValorComoFloat(GetCar(args));
            float b = ValorComoFloat(GetCar(GetCdr(args)));
            cmp = (a < b) ? -1 : (a > b ? 1 : 0);
        } else
#endif
        {
            long a = GetIntValue(GetCar(args));
            long b = GetIntValue(GetCar(GetCdr(args)));
            cmp = (a < b) ? -1 : (a > b ? 1 : 0);
        }

        if (sym == SYM_EQUALS) return (cmp == 0) ? T_CELL : NIL_CELL;
        if (sym == SYM_LT)     return (cmp < 0)  ? T_CELL : NIL_CELL;
        if (sym == SYM_GT)     return (cmp > 0)  ? T_CELL : NIL_CELL;
        if (sym == SYM_LE)     return (cmp <= 0) ? T_CELL : NIL_CELL;
        return (cmp >= 0) ? T_CELL : NIL_CELL;   /* solo puede ser SYM_GE aqui */
    }
}

/* Convierte una celda a su representacion textual, para STRCAT.
   TSTRING, TSYM, TNIL, TINT, TRAT soportados; el resto (incluido
   TFLOAT, aunque ahora mismo esta desactivado) da error, igual que
   en el original ("STR no acepta reales en Pro Pascal"). 'out' debe
   tener espacio para al menos MAXSTRLEN+1 bytes. */
/* Extraccion de texto ESTRICTA (solo TSTRING/TSYM) para STR< -- a
   diferencia de NodeToStr, que abajo tambien acepta NIL/TINT/TRAT
   para STRCAT. Distintas de proposito: STR< exige simbolo/string,
   STRCAT acepta cualquier atomo convertible a texto. */
static int ObtenerTextoEstricto(CellIdx c, char *out) {
    if (GetTag(c) == TSTRING) { StrtabGet(GetStrIdx(c), out); return 1; }
    if (GetTag(c) == TSYM) { SymtabGet(GetSymIdx(c), out); return 1; }
    return 0;
}

static int NodeToStr(CellIdx idx, char *out) {
    switch (GetTag(idx)) {
        case TSTRING:
            StrtabGet(GetStrIdx(idx), out);
            return 1;
        case TSYM:
            SymtabGet(GetSymIdx(idx), out);
            return 1;
        case TNIL:
            strcpy(out, "NIL");
            return 1;
        case TINT:
            sprintf(out, "%ld", GetIntValue(idx));
            return 1;
        case TRAT: {
            CellIdx inner = GetCar(idx);
            sprintf(out, "%ld/%ld", GetIntValue(GetCar(inner)), GetIntValue(GetCdr(inner)));
            return 1;
        }
        default:
            return 0;
    }
}

/* recursiva (se llama a si misma) -- locales automaticos, NO static */
static CellIdx AppendLists(CellIdx a, CellIdx b) {
    DIAG_PILA();
    if (GetTag(a) == TNIL) {
        return b;
    }
    return Cons(GetCar(a), AppendLists(GetCdr(a), b));
}

/* recursiva (caso TPAIR se llama a si misma) -- locales automaticos,
   NO static. Distingue tipo exacto ademas de valor -- (EQUAL 3 3.0)
   seria NIL porque un entero y un flotante son tipos distintos,
   decision de diseno explicita del original pensando en el futuro CAS. */
static int EqualStruct(CellIdx a, CellIdx b) {
    unsigned char tagA = GetTag(a);
    unsigned char tagB = GetTag(b);
    DIAG_PILA();

    if (tagA != tagB) return 0;

    switch (tagA) {
        case TNIL:
            return 1;
        case TSYM:
            return GetSymIdx(a) == GetSymIdx(b);
        case TSTRING: {
            char textoA[MAXSTRLEN + 1];
            char textoB[MAXSTRLEN + 1];
            StrtabGet(GetStrIdx(a), textoA);
            StrtabGet(GetStrIdx(b), textoB);
            return strcmp(textoA, textoB) == 0;
        }
        case TINT:
            return GetIntValue(a) == GetIntValue(b);
#ifndef MYLISP_SIN_FLOTANTES
        case TFLOAT:
            return GetFloatValue(a) == GetFloatValue(b);
#endif
        case TRAT: {
            CellIdx innerA = GetCar(a);
            CellIdx innerB = GetCar(b);
            return GetIntValue(GetCar(innerA)) == GetIntValue(GetCar(innerB)) &&
                   GetIntValue(GetCdr(innerA)) == GetIntValue(GetCdr(innerB));
        }
        case TPAIR:
            return EqualStruct(GetCar(a), GetCar(b)) &&
                   EqualStruct(GetCdr(a), GetCdr(b));
        default:
            return 0;
    }
}