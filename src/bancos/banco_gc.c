/* Enlaza codigo en su pagina; RODATA/DATA/BSS siguen residentes. */
#pragma codeseg CODE_PAGE_94
#define CollectGarbage Real_CollectGarbage
#define DeberiaDispararseGC Real_DeberiaDispararseGC
#define LiveUsagePercent Real_LiveUsagePercent
#include "../gc.c"
