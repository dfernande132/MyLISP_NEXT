/* =========================================================================
 * MyLISP Next - Aritmetica exacta con racionales (Etapa B)
 * =========================================================================
 * Port de racional.c de MyLISP65 (decisiones 8.3 de MEGA65): mismo modelo
 * y mismas reglas, para que los tres ports den el mismo resultado.
 *
 * Un valor exacto es un par (num, den) con den > 0; el entero es el caso
 * den == 1. Todo resultado se devuelve NORMALIZADO (reducido por el MCD,
 * signo en el numerador, colapsado a TINT si den == 1), de modo que cada
 * valor tiene UNA sola representacion y EQUAL sigue valiendo.
 *
 * Desbordamiento: cada operacion devuelve 0 si el resultado no cabe en
 * 32 bits con signo; quien llama levanta `ERR: desbordamiento`. Se reduce
 * ANTES de multiplicar (cruzando MCD) para retrasar el desbordamiento.
 *
 * Este fichero se INCLUYE desde eval.c (no se compila aparte): todo es
 * static, vive en la misma pagina de codigo que la aritmetica que lo usa
 * y no necesita puertas residentes. No hay recursion en ningun punto.
 * ========================================================================= */

typedef struct {
    long num;
    long den;   /* SIEMPRE > 0 tras normalizar */
} Racional;

#define RACIONAL_MAXLONG 2147483647L
/* 46340^2 = 2 147 395 600 < MAXLONG: por debajo de esto el producto cabe
   seguro y se evita la division de comprobacion. */
#define RAIZ_MAXLONG 46340L

/* AVISO (z88dk da80b5fe, clib classic): los operadores de ORDEN sobre long
   (< > <= >=) de sccz80 son INCORRECTOS cuando la resta interna a-b da un
   resultado con el byte alto 0x80 o desborda (l_long_cmp mira el signo y
   el cero del resultado sin tratar el desbordamiento): p. ej. 0 < 2147483645
   da falso y -2147483647 < 0 da cero. Medido en el simulador Z80N. Por eso
   TODA comparacion de orden de este fichero pasa por LongCmp/LongSigno, que
   comparan por mitades de 16 bits (verificadas contra Python con pares
   en los limites; == y != no se ven afectados y se usan tal cual). */
static int LongCmp(long a, long b) {
    int ah, bh;
    unsigned int al, bl;

    ah = (int)(a >> 16);
    bh = (int)(b >> 16);
    if (ah < bh) return -1;
    if (ah > bh) return 1;
    al = (unsigned int)(a & 0xFFFFL);
    bl = (unsigned int)(b & 0xFFFFL);
    if (al < bl) return -1;
    if (al > bl) return 1;
    return 0;
}

static int LongSigno(long v) {
    if ((int)(v >> 16) < 0) return -1;
    if (v == 0) return 0;
    return 1;
}

static long RacAbs(long v) {
    return (LongSigno(v) < 0) ? -v : v;
}

/* Euclides iterativo. Devuelve siempre >= 0. */
static long RacMcd(long a, long b) {
    long t;
    a = RacAbs(a);
    b = RacAbs(b);
    while (b != 0) {
        t = a % b;
        a = b;
        b = t;
    }
    return a;
}

static int EnteroSumar(long a, long b, long *r) {
    long limite;
    int sb = LongSigno(b);
    if (sb > 0) {
        limite = RACIONAL_MAXLONG - b;
        if (LongCmp(a, limite) > 0) return 0;
    } else if (sb < 0) {
        limite = -RACIONAL_MAXLONG - b;
        if (LongCmp(a, limite) < 0) return 0;
    }
    *r = a + b;
    return 1;
}

static int EnteroRestar(long a, long b, long *r) {
    long nb = -b;
    return EnteroSumar(a, nb, r);
}

static int EnteroMultiplicar(long a, long b, long *r) {
    long aa, bb, limite;

    if (a == 0 || b == 0) {
        *r = 0;
        return 1;
    }
    aa = RacAbs(a);
    bb = RacAbs(b);
    if (LongCmp(aa, RAIZ_MAXLONG) < 0 && LongCmp(bb, RAIZ_MAXLONG) < 0) {
        *r = a * b;
        return 1;
    }
    limite = RACIONAL_MAXLONG / bb;
    if (LongCmp(aa, limite) > 0) return 0;
    *r = a * b;
    return 1;
}

static void RacNormalizar(Racional *r) {
    long g;
    long n = r->num;
    long d = r->den;

    if (LongSigno(d) < 0) {
        n = -n;
        d = -d;
    }
    if (n == 0) {
        r->num = 0;
        r->den = 1;
        return;
    }
    g = RacMcd(n, d);
    if (g != 1) {
        n = n / g;
        d = d / g;
    }
    r->num = n;
    r->den = d;
}

/* a/b +- c/d con denominador comun b*(d/g), g = mcd(b, d). */
static int RacSumarConSigno(const Racional *a, const Racional *b, int restar,
                            Racional *r) {
    long g, db, bb, t1, t2, num, den;

    g = RacMcd(a->den, b->den);
    db = b->den / g;
    bb = a->den / g;

    if (!EnteroMultiplicar(a->num, db, &t1)) return 0;
    if (!EnteroMultiplicar(b->num, bb, &t2)) return 0;
    if (restar) {
        if (!EnteroRestar(t1, t2, &num)) return 0;
    } else {
        if (!EnteroSumar(t1, t2, &num)) return 0;
    }
    if (!EnteroMultiplicar(a->den, db, &den)) return 0;

    r->num = num;
    r->den = den;
    RacNormalizar(r);
    return 1;
}

/* (an/ad) * (bn/bd), cruzando MCD antes de multiplicar. */
static int RacMultiplicarPartes(long an, long ad, long bn, long bd,
                                Racional *r) {
    long g1, g2, n1, d2, n2, d1, num, den;

    g1 = RacMcd(an, bd);
    g2 = RacMcd(bn, ad);
    n1 = an / g1;
    d2 = bd / g1;
    n2 = bn / g2;
    d1 = ad / g2;

    if (!EnteroMultiplicar(n1, n2, &num)) return 0;
    if (!EnteroMultiplicar(d1, d2, &den)) return 0;

    r->num = num;
    r->den = den;
    RacNormalizar(r);
    return 1;
}

/* Compara a con b sin pasar por flotante: deja -1, 0 o 1 en *signo.
   Devuelve 0 si el producto cruzado desborda. */
static int RacComparar(const Racional *a, const Racional *b, int *signo) {
    long izq, der;
    int sa = LongSigno(a->num);
    int sb = LongSigno(b->num);

    if (sa < 0 && sb >= 0) { *signo = -1; return 1; }
    if (sa >= 0 && sb < 0) { *signo =  1; return 1; }
    if (!EnteroMultiplicar(a->num, b->den, &izq)) return 0;
    if (!EnteroMultiplicar(b->num, a->den, &der)) return 0;
    *signo = LongCmp(izq, der);
    return 1;
}

/* Lee una celda TINT o TRAT. 0 para cualquier otro tag (TFLOAT incluido:
   no es un valor exacto y no se mezcla aqui). */
static int RacDeCelda(CellIdx c, Racional *r) {
    unsigned char t = GetTag(c);
    CellIdx inner;

    if (t == TINT) {
        r->num = GetIntValue(c);
        r->den = 1;
        return 1;
    }
    if (t == TRAT) {
        inner = GetCar(c);
        r->num = GetIntValue(GetCar(inner));
        r->den = GetIntValue(GetCdr(inner));
        if (r->den == 0) return 0;   /* no deberia existir; defensivo */
        return 1;
    }
    return 0;
}

/* Unica forma correcta de construir un valor exacto: normaliza y colapsa
   a TINT cuando den == 1 (MakeRat es el constructor crudo). */
static CellIdx RacACelda(const Racional *r) {
    Racional n;

    n.num = r->num;
    n.den = r->den;
    RacNormalizar(&n);
    if (n.den == 1) return MakeInt(n.num);
    return MakeRat(n.num, n.den);
}

#ifndef MYLISP_SIN_FLOTANTES
static float RacComoFloat(const Racional *r) {
    float fn = (float)r->num;
    float fd = (float)r->den;
    return fn / fd;
}
#endif
