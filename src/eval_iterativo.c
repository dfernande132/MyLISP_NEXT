#include "diag_pila.h"
#include <stdio.h>
#include "eval_iterativo.h"
#include "pila_tareas.h"
#include "eval.h"
#include "messages.h"
#include "colors.h"
#include "control.h"
#ifndef SIN_LOAD
#include "load.h"
#endif

#define MODO_PROCESAR 0
#define MODO_ENTREGAR 1

/* Cache de simbolos especiales -- InternSym es idempotente, pedir el
   mismo nombre otra vez da el mismo indice que ya interno eval_init()
   al arrancar. Un solo array en vez de una funcion por simbolo (mas
   compacto), inicializado una vez de forma perezosa. */
#define SIM_IF      0
#define SIM_QUOTE   1
#define SIM_LAMBDA  2
#define SIM_DEFUN   3
#define SIM_STATUS  4
#define SIM_CLEAN   5
#define SIM_NEW     6
#define SIM_SYMBOLS 7
#define SIM_LET     8
#define SIM_PROGN   9
#define SIM_AND     10
#define SIM_OR      11
#define SIM_COND    12
#define SIM_DEFINE  13
#define SIM_T       14
#define SIM_NILSYM  15
#define SIM_LOAD    16
#define SIM_COUNT   17

static unsigned int cacheSimbolos[SIM_COUNT];
static int cacheSimbolosListo = 0;

static unsigned int Simbolo(int cual) {
    if (!cacheSimbolosListo) {
        cacheSimbolos[SIM_IF]      = InternSym("IF");
        cacheSimbolos[SIM_QUOTE]   = InternSym("QUOTE");
        cacheSimbolos[SIM_LAMBDA]  = InternSym("LAMBDA");
        cacheSimbolos[SIM_DEFUN]   = InternSym("DEFUN");
        cacheSimbolos[SIM_STATUS]  = InternSym("STATUS");
        cacheSimbolos[SIM_CLEAN]   = InternSym("CLEAN");
        cacheSimbolos[SIM_NEW]     = InternSym("NEW");
        cacheSimbolos[SIM_SYMBOLS] = InternSym("SYMBOLS");
        cacheSimbolos[SIM_LET]     = InternSym("LET");
        cacheSimbolos[SIM_PROGN]   = InternSym("PROGN");
        cacheSimbolos[SIM_AND]     = InternSym("AND");
        cacheSimbolos[SIM_OR]      = InternSym("OR");
        cacheSimbolos[SIM_COND]    = InternSym("COND");
        cacheSimbolos[SIM_DEFINE]  = InternSym("DEFINE");
        cacheSimbolos[SIM_T]       = InternSym("T");
        cacheSimbolos[SIM_NILSYM]  = InternSym("NIL");
        cacheSimbolos[SIM_LOAD]    = InternSym("LOAD");
        cacheSimbolosListo = 1;
    }
    return cacheSimbolos[cual];
}

void EvalIterativoReset(void) {
    cacheSimbolosListo = 0;
}

/* Iterativa (bucle simple, sin llamarse a si misma) -- segura para
   locales normales, no reentrante. Acotada por el numero de
   argumentos de UNA llamada (siempre pequeno), no por la profundidad
   general del programa. */
static CellIdx InvertirLista(CellIdx lista) {
    CellIdx resultado = NIL_CELL;
    while (GetTag(lista) == TPAIR) {
        resultado = Cons(GetCar(lista), resultado);
        lista = GetCdr(lista);
    }
    return resultado;
}

CellIdx EvalIterativo(CellIdx exprInicial, CellIdx envInicial) {
    int modo;
    unsigned char sondeo = 0;
    CellIdx valorEntregado = NIL_CELL;
    CellIdx resultadoFinal = NIL_CELL;
    unsigned char kind;
    CellIdx c1, c2, c3;
    unsigned int topeInicial = PilaTareasTope();
    DIAG_PILA();
    if (topeInicial == 0) {
        EjecucionCancelada = 0;
        PermitirNew = GetTag(exprInicial) == TPAIR &&
            GetTag(GetCar(exprInicial)) == TSYM &&
            GetSymIdx(GetCar(exprInicial)) == Simbolo(SIM_NEW) &&
            GetTag(GetCdr(exprInicial)) == TNIL;
    }

    /* NO se llama a PilaTareasInit() aqui -- eso reseteria el tope a 0
       y pisaria las tareas de una llamada externa todavia en marcha
       (LOAD y el builtin EVAL llaman a Eval()=EvalIterativo() desde
       DENTRO de una evaluacion en curso). PilaTareasInit() se llama
       UNA SOLA VEZ al arrancar el programa, no aqui. */
    PilaTareasEmpujar(TAREA_EVAL_EXPR, exprInicial, envInicial, NIL_CELL);
    modo = MODO_PROCESAR;

    for (;;) {
        if (PilaTareasTope() == topeInicial) {
            /* volvimos al nivel desde el que empezamos ESTA llamada
               -- no "vacia en general", que rompia las llamadas
               anidadas */
            resultadoFinal = valorEntregado;
            break;
        }
        if (++sondeo == 0 && ConsultarInterrupcion()) {
            EvalError = 1;
        }
        if (EjecucionCancelada || NuevoPendiente || HeapError) {
            if (HeapError) EvalError = 1;
            resultadoFinal = NIL_CELL;
            break;
        }

        PilaTareasLeerCima(&kind, &c1, &c2, &c3);

        if (modo == MODO_ENTREGAR && EvalError) {
            /* propagar el error hacia abajo sin hacer nada mas en
               esta tarea, igual que "if (EvalError) return NIL_CELL;"
               en cada nivel del evaluador recursivo */
            PilaTareasDesapilar();
            valorEntregado = NIL_CELL;
            continue;
        }

        if (modo == MODO_PROCESAR) {
            if (kind == TAREA_EVAL_EXPR) {
                CellIdx expr = c1, env = c2;
                unsigned char tag = GetTag(expr);

                if (tag == TNIL || tag == TINT || tag == TFLOAT ||
                    tag == TRAT || tag == TSTRING) {
                    valorEntregado = expr;
                    PilaTareasDesapilar();
                    modo = MODO_ENTREGAR;
                } else if (tag == TSYM) {
                    unsigned int symidx = GetSymIdx(expr);
                    /* NIL y T son seudo-constantes que se autoevaluan
                       -- NO son variables ligadas en ningun entorno,
                       asi que EnvLookup fallaria (correctamente) si
                       las tratasemos como cualquier otro simbolo */
                    if (symidx == Simbolo(SIM_NILSYM)) {
                        valorEntregado = NIL_CELL;
                    } else if (symidx == Simbolo(SIM_T)) {
                        valorEntregado = T_CELL;
                    } else {
                        valorEntregado = EnvLookup(env, symidx);
                    }
                    PilaTareasDesapilar();
                    modo = MODO_ENTREGAR;
                } else if (tag == TPAIR) {
                    CellIdx primero = GetCar(expr);
                    CellIdx resto = GetCdr(expr);
                    int esSimbolo = (GetTag(primero) == TSYM);
                    unsigned int simb = esSimbolo ? GetSymIdx(primero) : 0xFFFF;
                    int entregaDirecta = 0;

                    if (esSimbolo && simb == Simbolo(SIM_IF)) {
                        CellIdx test = GetCar(resto);
                        CellIdx tras = GetCdr(resto);
                        PilaTareasMutarCima(TAREA_IF_ESPERANDO, GetCar(tras), GetCdr(tras), env);
                        PilaTareasEmpujar(TAREA_EVAL_EXPR, test, env, 0);
                    } else if (esSimbolo && simb == Simbolo(SIM_QUOTE)) {
                        valorEntregado = GetCar(resto);
                        entregaDirecta = 1;
                    } else if (esSimbolo && simb == Simbolo(SIM_LAMBDA)) {
                        CellIdx params = GetCar(resto);
                        CellIdx cuerpo = GetCdr(resto);
                        valorEntregado = MakeClosure(params, cuerpo, env);
                        entregaDirecta = 1;
                    } else if (esSimbolo && simb == Simbolo(SIM_DEFUN)) {
                        CellIdx nombre = GetCar(resto);
                        CellIdx params = GetCar(GetCdr(resto));
                        CellIdx cuerpo = GetCdr(GetCdr(resto));
                        CellIdx closure = MakeClosure(params, cuerpo, env);
                        DefineGlobal(GetSymIdx(nombre), closure);
                        valorEntregado = nombre;
                        entregaDirecta = 1;
                    } else if (esSimbolo &&
                               (simb == Simbolo(SIM_STATUS) || simb == Simbolo(SIM_CLEAN) ||
                                simb == Simbolo(SIM_NEW) || simb == Simbolo(SIM_SYMBOLS))) {
                        /* STATUS/CLEAN/NEW/SYMBOLS: los cuatro hacen lo
                           mismo -- llamar sin argumentos y entregar el
                           resultado -- comparten una sola tabla en vez
                           de cuatro bloques casi identicos */
                        static CellIdx (*const funcs[4])(void) = {
                            EvalStatus, EvalClean, EvalNew, EvalSymbols
                        };
                        int i;
                        for (i = 0; i < 4; i++) {
                            if (simb == Simbolo(SIM_STATUS + i)) {
                                valorEntregado = funcs[i]();
                                break;
                            }
                        }
                        entregaDirecta = 1;
                    } else if (esSimbolo && simb == Simbolo(SIM_LET)) {
                        CellIdx bindings = GetCar(resto);
                        CellIdx cuerpo = GetCdr(resto);
                        PilaTareasMutarCima(TAREA_LET_ESPERANDO_BINDINGS, cuerpo, env, 0);
                        PilaTareasEmpujar(TAREA_LET_BINDINGS, bindings, NIL_CELL, env);
                    } else if (esSimbolo && simb == Simbolo(SIM_PROGN)) {
                        PilaTareasMutarCima(TAREA_SECUENCIA, resto, env, 0);
                        /* modo se queda en PROCESAR */
                    } else if (esSimbolo && simb == Simbolo(SIM_AND)) {
                        if (GetTag(resto) != TPAIR) {
                            valorEntregado = T_CELL;   /* (AND) = T */
                            entregaDirecta = 1;
                        } else {
                            PilaTareasMutarCima(TAREA_AND_ESPERANDO, resto, env, 0);
                            /* se reprocesa esta misma tarea ahora como AND_ESPERANDO */
                        }
                    } else if (esSimbolo && simb == Simbolo(SIM_OR)) {
                        if (GetTag(resto) != TPAIR) {
                            valorEntregado = NIL_CELL;   /* (OR) = NIL */
                            entregaDirecta = 1;
                        } else {
                            PilaTareasMutarCima(TAREA_OR_ESPERANDO, resto, env, 0);
                        }
                    } else if (esSimbolo && simb == Simbolo(SIM_COND)) {
                        if (GetTag(resto) != TPAIR) {
                            valorEntregado = NIL_CELL;   /* (COND) sin clausulas = NIL */
                            entregaDirecta = 1;
                        } else {
                            CellIdx primeraClausula = GetCar(resto);
                            CellIdx restoClausulas = GetCdr(resto);
                            CellIdx test = GetCar(primeraClausula);
                            PilaTareasMutarCima(TAREA_COND_ESPERANDO, primeraClausula, restoClausulas, env);
                            PilaTareasEmpujar(TAREA_EVAL_EXPR, test, env, 0);
                        }
                    } else if (esSimbolo && simb == Simbolo(SIM_DEFINE)) {
                        CellIdx nombre = GetCar(resto);
                        CellIdx valexpr = GetCar(GetCdr(resto));
                        PilaTareasMutarCima(TAREA_DEFINE_ESPERANDO, nombre, 0, 0);
                        PilaTareasEmpujar(TAREA_EVAL_EXPR, valexpr, env, 0);
#ifndef SIN_LOAD
                    } else if (esSimbolo && simb == Simbolo(SIM_LOAD)) {
                        /* (LOAD "fichero") o (LOAD nombre) -- el
                           argumento se inspecciona en crudo, SIN
                           evaluar, igual que en MYLISP.pas. DoLoad
                           puede evaluar expresiones del fichero, lo
                           que llama a Eval()=EvalIterativo() de forma
                           anidada -- seguro ahora que EvalIterativo
                           ya no resetea el tope de la pila a 0. */
                        CellIdx fnarg = GetCar(resto);
                        char nombre[MAXSTRLEN + 1];

                        if (GetTag(fnarg) == TSTRING) {
                            StrtabGet(GetStrIdx(fnarg), nombre);
                            DoLoad(nombre);
                            valorEntregado = T_CELL;
                        } else if (GetTag(fnarg) == TSYM) {
                            SymtabGet(GetSymIdx(fnarg), nombre);
                            DoLoad(nombre);
                            valorEntregado = T_CELL;
                        } else {
                            EvalError = 1;
                            INK(COLOR_ERROR_DESTACADO);
                            printf("%s\n", MSG_LOAD_ESPERA_STRING);
                            INK(COLOR_ERROR);
                            valorEntregado = NIL_CELL;
                        }
                        entregaDirecta = 1;
#endif
                    } else if (esSimbolo && EsBuiltin(simb)) {
                        /* builtin: no se evalua 'primero' (no esta
                           ligado en el entorno, se reconoce
                           directamente) -- va derecho a los args */
                        PilaTareasMutarCima(TAREA_APLICAR_BUILTIN, primero, env, 0);
                        PilaTareasEmpujar(TAREA_EVAL_ARGS, resto, NIL_CELL, env);
                    } else {
                        PilaTareasMutarCima(TAREA_APLICAR_FN, resto, env, 0);
                        PilaTareasEmpujar(TAREA_EVAL_EXPR, primero, env, 0);
                    }

                    if (entregaDirecta) {
                        PilaTareasDesapilar();
                        modo = MODO_ENTREGAR;
                    }
                    /* si no, modo se queda en PROCESAR: la nueva cima
                       es la tarea recien empujada */
                } else {
                    EvalError = 1;
                    INK(COLOR_ERROR_DESTACADO);
                    printf("%s\n", MSG_TIPO_NO_EVALUABLE);
                    INK(COLOR_ERROR);
                    valorEntregado = NIL_CELL;
                    PilaTareasDesapilar();
                    modo = MODO_ENTREGAR;
                }
            } else if (kind == TAREA_EVAL_ARGS) {
                CellIdx pendientes = c1, evaluados = c2;
                {
                    CellIdx env = c3;
                    if (GetTag(pendientes) != TPAIR) {
                        valorEntregado = InvertirLista(evaluados);
                        PilaTareasDesapilar();
                        modo = MODO_ENTREGAR;
                    } else {
                        CellIdx argExpr = GetCar(pendientes);
                        PilaTareasMutarCima(TAREA_EVAL_ARGS, GetCdr(pendientes), evaluados, env);
                        PilaTareasEmpujar(TAREA_EVAL_EXPR, argExpr, env, 0);
                        /* modo se queda en PROCESAR */
                    }
                }
            } else if (kind == TAREA_SECUENCIA) {
                CellIdx pendientes = c1, env = c2;
                if (GetTag(pendientes) != TPAIR) {
                    /* PROGN/cuerpo vacio -- NIL */
                    valorEntregado = NIL_CELL;
                    PilaTareasDesapilar();
                    modo = MODO_ENTREGAR;
                } else {
                    CellIdx actual = GetCar(pendientes);
                    CellIdx resto = GetCdr(pendientes);
                    if (GetTag(resto) != TPAIR) {
                        /* ultima expresion: posicion de cola, sustituye
                           esta tarea directamente por su evaluacion,
                           sin necesitar otra vuelta de ENTREGAR */
                        PilaTareasMutarCima(TAREA_EVAL_EXPR, actual, env, 0);
                    } else {
                        /* quedan mas: el valor de 'actual' se
                           descartara (ver ENTREGAR de TAREA_SECUENCIA) */
                        PilaTareasMutarCima(TAREA_SECUENCIA, resto, env, 0);
                        PilaTareasEmpujar(TAREA_EVAL_EXPR, actual, env, 0);
                    }
                    /* modo se queda en PROCESAR */
                }
            } else if (kind == TAREA_AND_ESPERANDO || kind == TAREA_OR_ESPERANDO) {
                /* misma estructura que SECUENCIA: evaluar la actual,
                   si no es la ultima seguir con el resto -- el
                   cortocircuito (mirar si es verdadera/falsa) se
                   decide en ENTREGAR, no aqui */
                CellIdx pendientes = c1, env = c2;
                CellIdx actual = GetCar(pendientes);
                CellIdx resto = GetCdr(pendientes);
                if (GetTag(resto) != TPAIR) {
                    /* ultima expresion: su valor es el resultado final
                       de AND/OR tal cual, sea verdadero o falso */
                    PilaTareasMutarCima(TAREA_EVAL_EXPR, actual, env, 0);
                } else {
                    PilaTareasMutarCima(kind, resto, env, 0);
                    PilaTareasEmpujar(TAREA_EVAL_EXPR, actual, env, 0);
                }
                /* modo se queda en PROCESAR */
            } else if (kind == TAREA_LET_BINDINGS) {
                CellIdx pendientes = c1;
                if (GetTag(pendientes) != TPAIR) {
                    /* no quedan bindings -- entregar el frame acumulado
                       (el orden no importa para EnvLookup) */
                    valorEntregado = c2;
                    PilaTareasDesapilar();
                    modo = MODO_ENTREGAR;
                } else {
                    /* NO avanzamos 'pendientes' todavia -- lo
                       necesitamos intacto para volver a extraer 'var'
                       cuando llegue el valor (ver ENTREGAR de
                       TAREA_LET_BINDINGS) */
                    CellIdx binding = GetCar(pendientes);
                    CellIdx valexpr = GetCar(GetCdr(binding));
                    PilaTareasEmpujar(TAREA_EVAL_EXPR, valexpr, c3, 0);
                    /* modo se queda en PROCESAR */
                }
            }
            /* TAREA_APLICAR_FN, TAREA_APLICAR_ARGS, TAREA_APLICAR_BUILTIN
               e TAREA_IF_ESPERANDO nunca se procesan "en frio" -- solo
               reciben resultados via ENTREGAR, ver abajo. No deberian
               aparecer aqui dado como se empujan. */
        } else {
            /* MODO_ENTREGAR (y ya sabemos que EvalError no esta activo,
               por el chequeo de arriba) -- kind/c1/c2/c3 ya vienen
               leidos de la cima desde el principio del bucle */
            if (kind == TAREA_APLICAR_FN) {
                CellIdx argsExpr = c1, env = c2;
                CellIdx fn = valorEntregado;
                PilaTareasMutarCima(TAREA_APLICAR_ARGS, fn, env, 0);
                PilaTareasEmpujar(TAREA_EVAL_ARGS, argsExpr, NIL_CELL, env);
                modo = MODO_PROCESAR;
            } else if (kind == TAREA_APLICAR_ARGS) {
                CellIdx fn = c1;
                CellIdx argsEvaluados = valorEntregado;

                /* Si fn es un simbolo citado (p.ej. F ligado a 'DOBLE
                   en vez de al cierre) se resuelve por nombre en
                   GlobalEnv -- el mismo sitio donde DEFUN liga las
                   funciones -- antes de rendirse. Permite pasar
                   funciones como 'NOMBRE citado en posicion de llamada
                   directa (MAP, FILTER, REDUCE...) sin pasar por APPLY. */
                if (GetTag(fn) == TSYM) {
                    fn = EnvLookup(GlobalEnv, GetSymIdx(fn));
                }

                if (EvalError) {
                    /* EnvLookup ya informo (simbolo no ligado) --
                       no duplicar aviso */
                    PilaTareasDesapilar();
                    valorEntregado = NIL_CELL;
                } else if (GetTag(fn) != TCLOSURE) {
                    EvalError = 1;
                    INK(COLOR_ERROR_DESTACADO);
                    printf("%s\n", MSG_NO_APLICABLE);
                    INK(COLOR_ERROR);
                    PilaTareasDesapilar();
                    valorEntregado = NIL_CELL;
                    /* modo se queda en ENTREGAR */
                } else {
                    CellIdx params = GetClosureParams(fn);
                    CellIdx cuerpo = GetClosureBody(fn);
                    CellIdx cenv = GetClosureEnv(fn);
                    CellIdx newenv = ExtendEnv(params, argsEvaluados, cenv);

                    if (EvalError) {
                        PilaTareasDesapilar();
                        valorEntregado = NIL_CELL;
                    } else {
                        /* SECUENCIA cubre cuerpos de UNA o VARIAS
                           expresiones por igual -- ya no hace falta
                           la limitacion del prototipo original */
                        PilaTareasMutarCima(TAREA_SECUENCIA, cuerpo, newenv, 0);
                        modo = MODO_PROCESAR;
                    }
                }
            } else if (kind == TAREA_EVAL_ARGS) {
                CellIdx pendientes = c1, evaluados = c2, env = c3;
                CellIdx evaluadosNuevo = Cons(valorEntregado, evaluados);
                PilaTareasMutarCima(TAREA_EVAL_ARGS, pendientes, evaluadosNuevo, env);
                modo = MODO_PROCESAR;
            } else if (kind == TAREA_APLICAR_BUILTIN) {
                CellIdx primero = c1;
                CellIdx argsEvaluados = valorEntregado;
                CellIdx resultado = AplicarBuiltin(GetSymIdx(primero), argsEvaluados);
                PilaTareasDesapilar();
                valorEntregado = resultado;
                /* modo se queda en ENTREGAR */
            } else if (kind == TAREA_SECUENCIA) {
                /* el valor entregado (de una expresion NO final) se
                   descarta -- la ultima expresion se gestiona en
                   posicion de cola en PROCESAR, sin pasar por aqui */
                modo = MODO_PROCESAR;   /* vuelve a examinar esta MISMA tarea */
            } else if (kind == TAREA_AND_ESPERANDO) {
                if (!IsTruthy(valorEntregado)) {
                    /* cortocircuito: NIL, abandonar el resto */
                    PilaTareasDesapilar();
                    valorEntregado = NIL_CELL;
                    /* modo se queda en ENTREGAR */
                } else {
                    modo = MODO_PROCESAR;   /* sigue con lo que quede */
                }
            } else if (kind == TAREA_OR_ESPERANDO) {
                if (IsTruthy(valorEntregado)) {
                    /* cortocircuito: se queda el valor tal cual */
                    PilaTareasDesapilar();
                    /* modo se queda en ENTREGAR, valorEntregado ya es el correcto */
                } else {
                    modo = MODO_PROCESAR;
                }
            } else if (kind == TAREA_COND_ESPERANDO) {
                CellIdx clausulaActual = c1, clausulasRestantes = c2, env = c3;

                if (IsTruthy(valorEntregado)) {
                    CellIdx cuerpo = GetCdr(clausulaActual);
                    if (GetTag(cuerpo) != TPAIR) {
                        /* clausula solo con test, sin cuerpo -- el
                           valor del propio test es el resultado */
                        PilaTareasDesapilar();
                        /* modo se queda en ENTREGAR */
                    } else {
                        PilaTareasMutarCima(TAREA_SECUENCIA, cuerpo, env, 0);
                        modo = MODO_PROCESAR;
                    }
                } else if (GetTag(clausulasRestantes) == TPAIR) {
                    CellIdx siguienteClausula = GetCar(clausulasRestantes);
                    CellIdx siguienteTest = GetCar(siguienteClausula);
                    PilaTareasMutarCima(TAREA_COND_ESPERANDO, siguienteClausula,
                                        GetCdr(clausulasRestantes), env);
                    PilaTareasEmpujar(TAREA_EVAL_EXPR, siguienteTest, env, 0);
                    modo = MODO_PROCESAR;
                } else {
                    /* ninguna clausula coincidio -- NIL */
                    PilaTareasDesapilar();
                    valorEntregado = NIL_CELL;
                    /* modo se queda en ENTREGAR */
                }
            } else if (kind == TAREA_DEFINE_ESPERANDO) {
                CellIdx nombre = c1;
                DefineGlobal(GetSymIdx(nombre), valorEntregado);
                PilaTareasDesapilar();
                valorEntregado = nombre;
                /* modo se queda en ENTREGAR */
            } else if (kind == TAREA_LET_BINDINGS) {
                CellIdx pendientes = c1, frameAcum = c2, envOriginal = c3;
                CellIdx binding = GetCar(pendientes);
                CellIdx var = GetCar(binding);
                CellIdx nuevoFrame = Cons(Cons(var, valorEntregado), frameAcum);
                PilaTareasMutarCima(TAREA_LET_BINDINGS, GetCdr(pendientes), nuevoFrame, envOriginal);
                modo = MODO_PROCESAR;   /* vuelve a examinar esta MISMA tarea, ya avanzada */
            } else if (kind == TAREA_LET_ESPERANDO_BINDINGS) {
                CellIdx cuerpo = c1, envOriginal = c2;
                CellIdx frame = valorEntregado;
                CellIdx newenv = Cons(frame, envOriginal);
                PilaTareasMutarCima(TAREA_SECUENCIA, cuerpo, newenv, 0);
                modo = MODO_PROCESAR;
            } else if (kind == TAREA_IF_ESPERANDO) {
                CellIdx ramaThen = c1, restoRamas = c2, env = c3;

                if (IsTruthy(valorEntregado)) {
                    PilaTareasMutarCima(TAREA_EVAL_EXPR, ramaThen, env, 0);
                    modo = MODO_PROCESAR;
                } else if (GetTag(restoRamas) == TPAIR) {
                    PilaTareasMutarCima(TAREA_EVAL_EXPR, GetCar(restoRamas), env, 0);
                    modo = MODO_PROCESAR;
                } else {
                    /* sin rama else: resultado NIL, igual que EvalIf */
                    PilaTareasDesapilar();
                    valorEntregado = NIL_CELL;
                    /* modo se queda en ENTREGAR */
                }
            }
        }
    }

    if (HeapError) EvalError = 1;
    while (PilaTareasTope() > topeInicial) PilaTareasDesapilar();
    if (topeInicial == 0) {
        PermitirNew = 0;
        if (EjecucionCancelada) {
            EvalError = 1; INK(COLOR_INFO);
            puts("Execution cancelled.");
        }
    }
    return resultadoFinal;
}