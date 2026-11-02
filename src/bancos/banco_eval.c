/* Enlaza codigo en su pagina; RODATA/DATA/BSS siguen residentes. */
#pragma codeseg CODE_PAGE_90
#define eval_init Real_eval_init
#define EnvLookup Real_EnvLookup
#define ExtendEnv Real_ExtendEnv
#define DefineGlobal Real_DefineGlobal
#define IsTruthy Real_IsTruthy
#define EsVoid Real_EsVoid
#define EsBuiltin Real_EsBuiltin
#define AplicarBuiltin Real_AplicarBuiltin
#define EvalStatus Real_EvalStatus
#define EvalClean Real_EvalClean
#define EvalNew Real_EvalNew
#define EvalSymbols Real_EvalSymbols
#define Eval Real_Eval
#include "../eval.c"
