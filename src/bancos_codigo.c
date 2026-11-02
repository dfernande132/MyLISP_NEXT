#include <stdio.h>
#include <arch/zxn.h>
#include "heap_ops.h"
#include "bancos_codigo.h"
#include "colors.h"
#include "teclado.h"
extern const char AyudaTexto[];
unsigned char CodigoPaginaActual;
extern const char BannerArte[];
/* Codigo y pila residentes. El ISR no debe usar ni modificar MMU6.
   El estado local de cada puerta permite llamadas A-B-A anidadas. */
static void SeleccionarCodigo(unsigned char pagina) {
    if (CodigoPaginaActual != pagina) {
        ZXN_WRITE_MMU6(pagina);
        CodigoPaginaActual = pagina;
    }
}
void CodigoInit(void) { CodigoPaginaActual = ZXN_READ_REG(0x56); }
void CodigoAyuda(void) {
    unsigned char antes = CodigoPaginaActual;
    unsigned char lineas = 0;
    const char *p;
    SeleccionarCodigo(93);
    p = AyudaTexto;
    INK(COLOR_INFO);
    while (*p) {
        char c = *p++;
        putchar(c);
        if (c == '\n' && ++lineas == 20 && *p) {
            printf("-- Press any key to continue --");
            EsperarTecla();
            putchar('\n');
            lineas = 0;
        }
    }
    INK(COLOR_ENTRADA);
    SeleccionarCodigo(antes);
}
extern void Real_EsperarTecla(void);
void EsperarTecla(void) {
    unsigned char antes = CodigoPaginaActual;
    SeleccionarCodigo(95); Real_EsperarTecla(); SeleccionarCodigo(antes);
}
void CodigoBanner(void) {
    unsigned char antes = CodigoPaginaActual;
    SeleccionarCodigo(93);
    printf("%s", BannerArte);
    SeleccionarCodigo(antes);
}
extern void Real_eval_init(void);
void eval_init(void) {
    unsigned char antes = CodigoPaginaActual;
    SeleccionarCodigo(90);
    Real_eval_init();
    SeleccionarCodigo(antes);
}
extern CellIdx Real_EnvLookup(CellIdx env, unsigned int symidx);
CellIdx EnvLookup(CellIdx env, unsigned int symidx) {
    unsigned char antes = CodigoPaginaActual;
    CellIdx resultado;
    SeleccionarCodigo(90);
    resultado = Real_EnvLookup(env, symidx);
    SeleccionarCodigo(antes);
    return resultado;
}
extern CellIdx Real_ExtendEnv(CellIdx params, CellIdx args, CellIdx env);
CellIdx ExtendEnv(CellIdx params, CellIdx args, CellIdx env) {
    unsigned char antes = CodigoPaginaActual;
    CellIdx resultado;
    SeleccionarCodigo(90);
    resultado = Real_ExtendEnv(params, args, env);
    SeleccionarCodigo(antes);
    return resultado;
}
extern void Real_DefineGlobal(unsigned int symidx, CellIdx valor);
void DefineGlobal(unsigned int symidx, CellIdx valor) {
    unsigned char antes = CodigoPaginaActual;
    SeleccionarCodigo(90);
    Real_DefineGlobal(symidx, valor);
    SeleccionarCodigo(antes);
}
extern int Real_IsTruthy(CellIdx v) __z88dk_fastcall;
int IsTruthy(CellIdx v) __z88dk_fastcall {
    unsigned char antes = CodigoPaginaActual;
    int resultado;
    SeleccionarCodigo(90);
    resultado = Real_IsTruthy(v);
    SeleccionarCodigo(antes);
    return resultado;
}
extern int Real_EsVoid(CellIdx v) __z88dk_fastcall;
int EsVoid(CellIdx v) __z88dk_fastcall {
    unsigned char antes = CodigoPaginaActual;
    int resultado;
    SeleccionarCodigo(90);
    resultado = Real_EsVoid(v);
    SeleccionarCodigo(antes);
    return resultado;
}
extern int Real_EsBuiltin(unsigned int sym);
int EsBuiltin(unsigned int sym) {
    unsigned char antes = CodigoPaginaActual;
    int resultado;
    SeleccionarCodigo(90);
    resultado = Real_EsBuiltin(sym);
    SeleccionarCodigo(antes);
    return resultado;
}
extern CellIdx Real_AplicarBuiltin(unsigned int sym, CellIdx args);
CellIdx AplicarBuiltin(unsigned int sym, CellIdx args) {
    unsigned char antes = CodigoPaginaActual;
    CellIdx resultado;
    SeleccionarCodigo(90);
    resultado = Real_AplicarBuiltin(sym, args);
    SeleccionarCodigo(antes);
    return resultado;
}
extern CellIdx Real_EvalStatus(void);
CellIdx EvalStatus(void) {
    unsigned char antes = CodigoPaginaActual;
    CellIdx resultado;
    SeleccionarCodigo(90);
    resultado = Real_EvalStatus();
    SeleccionarCodigo(antes);
    return resultado;
}
extern CellIdx Real_EvalClean(void);
CellIdx EvalClean(void) {
    unsigned char antes = CodigoPaginaActual;
    CellIdx resultado;
    SeleccionarCodigo(90);
    resultado = Real_EvalClean();
    SeleccionarCodigo(antes);
    return resultado;
}
extern CellIdx Real_EvalNew(void);
CellIdx EvalNew(void) {
    unsigned char antes = CodigoPaginaActual;
    CellIdx resultado;
    SeleccionarCodigo(90);
    resultado = Real_EvalNew();
    SeleccionarCodigo(antes);
    return resultado;
}
extern CellIdx Real_EvalSymbols(void);
CellIdx EvalSymbols(void) {
    unsigned char antes = CodigoPaginaActual;
    CellIdx resultado;
    SeleccionarCodigo(90);
    resultado = Real_EvalSymbols();
    SeleccionarCodigo(antes);
    return resultado;
}
extern CellIdx Real_Eval(CellIdx expr, CellIdx env);
CellIdx Eval(CellIdx expr, CellIdx env) {
    unsigned char antes = CodigoPaginaActual;
    CellIdx resultado;
    SeleccionarCodigo(90);
    resultado = Real_Eval(expr, env);
    SeleccionarCodigo(antes);
    return resultado;
}
extern CellIdx Real_EvalIterativo(CellIdx expr, CellIdx env);
CellIdx EvalIterativo(CellIdx expr, CellIdx env) {
    unsigned char antes = CodigoPaginaActual;
    CellIdx resultado;
    SeleccionarCodigo(91);
    resultado = Real_EvalIterativo(expr, env);
    SeleccionarCodigo(antes);
    return resultado;
}
extern void Real_EvalIterativoReset(void);
void EvalIterativoReset(void) {
    unsigned char antes = CodigoPaginaActual;
    SeleccionarCodigo(91);
    Real_EvalIterativoReset();
    SeleccionarCodigo(antes);
}
extern void Real_SetLine(const char *texto) __z88dk_fastcall;
void SetLine(const char *texto) __z88dk_fastcall {
    unsigned char antes = CodigoPaginaActual;
    SeleccionarCodigo(92);
    Real_SetLine(texto);
    SeleccionarCodigo(antes);
}
extern void Real_NextToken(void);
void NextToken(void) {
    unsigned char antes = CodigoPaginaActual;
    SeleccionarCodigo(92);
    Real_NextToken();
    SeleccionarCodigo(antes);
}
extern void Real_CollectGarbage(CellIdx raiz1, CellIdx raiz2);
void CollectGarbage(CellIdx raiz1, CellIdx raiz2) {
    unsigned char antes = CodigoPaginaActual;
    SeleccionarCodigo(94);
    Real_CollectGarbage(raiz1, raiz2);
    SeleccionarCodigo(antes);
}
extern int Real_DeberiaDispararseGC(int umbral);
int DeberiaDispararseGC(int umbral) {
    unsigned char antes = CodigoPaginaActual;
    int resultado;
    SeleccionarCodigo(94);
    resultado = Real_DeberiaDispararseGC(umbral);
    SeleccionarCodigo(antes);
    return resultado;
}
extern long Real_LiveUsagePercent(void);
long LiveUsagePercent(void) {
    unsigned char antes = CodigoPaginaActual;
    long resultado;
    SeleccionarCodigo(94);
    resultado = Real_LiveUsagePercent();
    SeleccionarCodigo(antes);
    return resultado;
}

extern void Real_TecladoInit(void);
void TecladoInit(void) {
    unsigned char antes = CodigoPaginaActual;
    SeleccionarCodigo(95); Real_TecladoInit(); SeleccionarCodigo(antes);
}
extern int Real_LeerTeclado(char *destino, int max);
int LeerTeclado(char *destino, int max) {
    unsigned char antes = CodigoPaginaActual;
    int r;
    SeleccionarCodigo(95); r = Real_LeerTeclado(destino, max);
    SeleccionarCodigo(antes); return r;
}
