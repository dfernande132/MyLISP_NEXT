# MyLISP (ZX Spectrum Next) v1.0 — Technical Reference

LISP interpreter for the ZX Spectrum Next, written in C (Z88DK) under
NextZXOS. Designed as a computer algebra system (CAS) engine: it prioritizes
mathematical exactness over convenience, strict lexical scoping, and
arithmetic safety over concrete types.

It shares its design and philosophy with the original Sinclair QL version,
with one important structural difference: the evaluator is **iterative**,
not recursive — the recursion depth of a user's Lisp program does not depend
on the CPU's call stack. See §1 and §13.

---

## 1. General architecture

MyLISP implements a classic LISP interpreter with the following layers:

- **Lexer**: tokenizes the input character by character, detecting numeric
  literals (integers, rationals, floats), symbols, strings, and delimiters.
- **Parser**: builds the abstract syntax tree (AST) as a structure of linked
  pairs on the heap.
- **Evaluator**: `EvalIterativo`, a single loop with no C recursion, backed
  by an explicit stack of pending tasks that lives in paged memory separate
  from the main heap. **A user Lisp program's recursion depth is not limited
  by the CPU's call stack** — only by how much memory remains in the heap and
  the task stack, both far more generous than the native stack.
  `(factorial 10)`, with genuine 10-level recursion and several calls per
  level, is a standard reference test; depths of several hundred levels work
  without issue.
- **Heap**: a paged array of 32,000 cells managed by a mark-sweep collector.
- **REPL**: a read-eval-print loop with multi-line input support.

---

## 2. System limits

MyLISP operates within the following fixed limits. Programmers should keep
these in mind when designing their programs:

- **32,000 heap cells** available to store all the lists, numbers, symbols,
  and functions of the session. The garbage collector automatically reclaims
  cells that are no longer in use. Unlike deep recursion (which the iterative
  evaluator handles without issue), genuinely huge data structures can
  exhaust this space.

- **200 distinct symbols** at most per session. Every variable or function
  name that gets defined uses up one entry. The interpreter's own builtin
  symbols already consume some of these entries at startup.

- **8 characters** is the maximum significant length of a symbol name.
  Characters beyond the eighth are silently discarded. `MY-LONG-FUNCTION` and
  `MY-SHORT-FUNC` would be the same symbol (`MY-LONG-`) — the first eight
  characters of each function or variable name should be unique.

- **50 strings** can exist simultaneously on the heap. Under normal use this
  table doesn't run out, but programs that load many files in a row or build
  many strings at once can approach the limit.

- **500 characters** per expression — both in the interactive REPL and when
  loading with `LOAD`. Unlike other platforms where the console driver
  limits interactive input to a smaller number of characters, on the Next
  both contexts share the same, more generous, limit.

- **36 characters** maximum length for a string.

There is no separate table for floating-point numbers: each `TFLOAT` value
is packed directly inside its own cell.

---

## 3. Data types

MyLISP has seven internal cell types:

| Internal type | Description | Literal example |
|---|---|---|
| `TINT` | 32-bit signed integer | `42`, `-7` |
| `TRAT` | Exact rational, always reduced to lowest terms | `1/3`, `-5/2` |
| `TFLOAT` | Floating-point number (compiler-native, 6 bytes) | `3.0`, `-2.5` |
| `TSYM` | Symbol (LISP identifier) | `X`, `MY-FUN?` |
| `TSTRING` | Text string between double quotes | `"file.lsp"` |
| `TPAIR` | `(car . cdr)` pair, base structure of lists | `(1 2 3)` |
| `TCLOSURE` | Function with its captured lexical environment | — |

The predefined constants `T` (true) and `NIL` (false / empty list) are
special internal symbols that are always alive on the heap.

---

## 4. The numeric system: exact arithmetic

**Fundamental principle:** MyLISP never silently promotes an integer to a
float. This decision is deliberate and irrevocable: a CAS that turns `13`
into `13.000001` through silent overflow produces algebraically incorrect
results that propagate undetectably.

### Numeric types and their rules

**Integer (`TINT`):** range from −2,147,483,647 to +2,147,483,647 (32-bit
signed). If an operation threatens to exceed that range, it is interrupted
with an error before producing the incorrect result.

**Rational (`TRAT`):** generated automatically when `/` divides two integers
without an exact remainder. Always stored and operated on in lowest terms
(reduced by GCD). `(/ 4 6)` returns `2/3`, not `0.666`.

**Float (`TFLOAT`):** generated only when the literal carries an explicit
decimal point (`3.0`, not `3`). Once one operand is a float, the whole
operation produces a float. Float is an explicit input type, not an
automatic output of the system. Mixed integer/float arithmetic happens in a
single pass: `(+ 1 2.5)` → `3.5`.

### Division: `/` vs `DIV`/`MOD`

| Expression | Result | Type |
|---|---|---|
| `(/ 10 3)` | `10/3` | `TRAT` — exact fraction |
| `(/ 10 2)` | `5` | `TINT` — exact result |
| `(/ 1.0 3)` | `0.3333` | `TFLOAT` — one operand is a float |
| `(DIV 10 3)` | `3` | `TINT` — integer quotient, truncated toward zero |
| `(MOD 10 3)` | `1` | `TINT` — remainder, same sign as the dividend |

`DIV` and `MOD` only accept `TINT`. Passing a rational or float produces an
error.

### Equality: `=` vs `EQUAL`

| Expression | Result | Criterion |
|---|---|---|
| `(= 3 3.0)` | `T` | Same numeric value, ignores type |
| `(= 1/2 0.5)` | `T` | Same numeric value |
| `(EQUAL 3 3.0)` | `NIL` | Different types (`TINT` ≠ `TFLOAT`) |
| `(EQUAL 1/2 1/2)` | `T` | Same type and same value |
| `(EQ 'X 'X)` | `T` | Same internal symbol |
| `(EQ '(1 2) '(1 2))` | `NIL` | Different cells on the heap |

---

## 5. Syntax

### Identifiers

Symbols may contain letters, digits, and the characters `-`, `?`, `!`, and `<`
in any position except the first. Only the first 8 characters are
significant.

```lisp
MY-FUNCTION  ; valid
PAIR?        ; valid (convention: predicates end in ?)
STR<         ; valid (convention: comparators end in <)
COUNTER!     ; valid (unusual in pure LISP, more common in Scheme)
3X           ; invalid: cannot start with a digit
```

### Strings

Strings are written between double quotes and accept any character. Their
main use is passing file names to `LOAD`. Maximum 36 characters.

```lisp
"cas.lsp"          ; valid file name
"hello world"      ; string with a space
```

### Comments

The `;` character starts a comment that extends to the end of the line.
Valid both in the REPL and in files loaded with `LOAD`.

```lisp
; this is a full-line comment
(+ 1 2)   ; this is also a comment
```

### Rational literals

```lisp
1/3    ; stored directly as TRAT
4/8    ; automatically reduced to 1/2
```

---

## 6. Immutability

MyLISP does not implement `SETQ` or any form of mutation of existing
bindings. This is a **deliberate design decision**, not a technical
limitation. Once a symbol is bound to a value (via `DEFINE`, `DEFUN`, or a
function parameter), that binding is permanent within that environment.

`DEFINE` always adds a new binding in the global environment. Defining the
same symbol twice adds a new entry that shadows the previous one during
lookups, but does not modify the original cell.

For algorithms that require mutable state, the idiomatic MyLISP solution is
to use accumulator parameters in recursive functions:

```lisp
; Instead of mutating a variable, pass the state as a parameter
(DEFUN SUM-LIST (L ACC)
  (IF (NULL L) ACC
      (SUM-LIST (CDR L) (+ ACC (CAR L)))))

(SUM-LIST '(1 2 3 4 5) 0)   ; -> 15
```

---

## 7. Lexical scope and closures

`LAMBDA` captures the lexical environment at the moment of its creation.
This enables function factories and higher-order functions:

```lisp
(DEFINE make-adder
  (LAMBDA (n)
    (LAMBDA (x) (+ x n))))

(DEFINE add5 (make-adder 5))
(add5 10)   ; -> 15  (n is captured as 5)
```

**`DEFUN` as syntactic sugar:**
`(DEFUN F (X) body)` is exactly equivalent to
`(DEFINE F (LAMBDA (X) body))`.

**`LET` and the environment:**
All of `LET`'s binding values are evaluated in the outer environment, not
against each other:

```lisp
(LET ((X 1)
      (Y X))    ; X here is the outer X, not the just-defined 1
  (+ X Y))
```

**Single namespace (Lisp-1):**
MyLISP uses a single environment for both variables and functions. Defining
a variable with the same name as a builtin function overwrites it:

```lisp
(DEFINE LIST 5)   ; LIST no longer works as a function
(LIST 1 2 3)      ; ERROR: 5 is not a function
```

---

## 8. Special forms

Special forms do not evaluate all of their arguments in the standard way.

| Form | Evaluation |
|---|---|
| `(QUOTE expr)` or `'expr` | Does not evaluate `expr` |
| `(IF test then)` | Evaluates only `test`; if true, evaluates `then`; if false, returns `NIL` |
| `(IF test then else)` | Evaluates `test`; evaluates only the chosen branch |
| `(COND (t1 e1) ...)` | Evaluates tests in order; stops at the first true one |
| `(AND e1 e2 ...)` | Short-circuits: stops at the first `NIL`; returns the last value |
| `(OR e1 e2 ...)`  | Short-circuits: stops at the first non-`NIL`; returns that value |
| `(PROGN e1 e2 ...)` | Evaluates all in order; returns the value of the last one |
| `(LET ((v1 e1) ...) body)` | Evaluates the `ei` in the current environment; evaluates `body` in the new environment |
| `(LAMBDA (params) body)` | Creates a closure without evaluating the body |
| `(DEFINE sym val)` | Evaluates `val`; adds a binding in the global environment |
| `(DEFUN f (params) body)` | Sugar for `(DEFINE f (LAMBDA ...))` |
| `(LOAD "file")` | Does not evaluate the name; reads and evaluates the file |
| `(EVAL expr)` | Evaluates `expr`, then evaluates the result again in `GlobalEnv` |
| `(STATUS)` | Shows the state of the internal tables: heap, symbols, and strings. Returns `VOID` |
| `(CLEAN)` | Forces an immediate garbage collection, showing before/after. Returns `VOID` |
| `(NEW)` | Resets the interpreter after asking for confirmation. Erases all user definitions. Returns `VOID` |
| `(SYMBOLS)` | Lists all user-defined symbols in the global environment. Returns `VOID` |

---

## 9. Memory management and the garbage collector

### The heap

The heap is a paged array of 32,000 cells. Each cell holds a type tag, a
`car` field, and a `cdr` field (or, for `TFLOAT`, the 6 bytes of the
floating-point number packed directly). There is no dynamic memory
allocation from the operating system.

There is additionally an auxiliary string table (`strtab`, max. 50 entries,
36 characters each) and the symbol table (`symtab`, max. 200 entries, 8
characters each).

### The mark-sweep collector

The GC fires automatically between REPL iterations when the heap exceeds 80%
occupancy. The programmer **never invokes it manually under normal
conditions** and **never receives any message** when it runs this way — it
is completely transparent.

**Note on `LOAD`:** unlike the REPL's behavior, loading a file with `LOAD`
**does not automatically trigger the garbage collector**. For most files
this is not an issue — the 32,000-cell heap is generous. If a single file
were to generate enough live data to exhaust it, the programmer can force a
manual collection with `(CLEAN)` **from the REPL**, splitting the file into
smaller parts and loading them separately. Note: `(CLEAN)` written inside a
file that is itself being loaded via `LOAD` does not run the collection —
the interpreter detects this and skips it with a warning, precisely to avoid
this case. Run `(CLEAN)` from the REPL between one load and the next if you
need to.

### Heap exhaustion

If the heap runs out mid-evaluation, the interpreter sets the `HeapError`
and `ErrorFlag` flags to cut the current evaluation short in an orderly way.

---

## 10. The REPL and its limits

### Multi-line input

If an expression has unclosed parentheses at the end of a line, the REPL
shows `..` and waits for the continuation. Lines are accumulated internally
until the parentheses balance out.

### The 500-character limit

The interpreter measures the accumulated buffer length before requesting
each new line in multi-line mode. If adding the next line would push the
total past 500 characters, it shows:

```
ERR: expresion larga
```

This limit is the same in both the REPL and `LOAD` — unlike other platforms
where the console driver imposes a stricter limit on interactive input only.

### Exiting the interpreter

Type `BYE` at the prompt to quit.

---

## 11. The LOAD function

```lisp
(LOAD "cas.lsp")          ; file name in quotes
(LOAD 'MYLIB)              ; alternative with a symbol, no quotes
```

`LOAD` reads the file line by line, accumulating until the parentheses
balance out, and evaluates each complete expression. Results are shown just
as in the REPL. Errors in one expression **do not stop the load**: the
interpreter shows the error and continues with the next expression.

`LOAD` is **reentrant**: a file loaded with `LOAD` may itself contain
another call to `LOAD`, or to `EVAL`, without corrupting the outer load's
state.

See §9 for the garbage collector's behavior during `LOAD`.

---

## 12. Quick primitive reference

### List constructors and selectors

| Function | Description |
|---|---|
| `(CAR list)` | First element. Error if `list` is `NIL` or an atom |
| `(CDR list)` | Rest. Error if `list` is `NIL` or an atom |
| `(CONS x list)` | New pair with `x` as `car` and `list` as `cdr` |
| `(LIST e1 e2 ...)` | New list containing all the arguments |
| `(APPEND l1 l2)` | Concatenates two lists. `l1` is rebuilt; `l2` is shared |

### Predicates

| Function | Description |
|---|---|
| `(ATOM x)` | `T` if `x` is not a pair (number, symbol, string, `NIL`) |
| `(NULL x)` | `T` if `x` is `NIL` |
| `(NOT x)` | `T` if `x` is `NIL`; `NIL` otherwise |
| `(NUMBERP x)` | `T` if `x` is `TINT`, `TRAT`, or `TFLOAT` |
| `(SYMBOLP x)` | `T` if `x` is `TSYM` |
| `(LISTP x)` | `T` if `x` is `TPAIR` or `NIL` |
| `(EQ x y)` | `T` if they are the same symbol or the same cell in memory |
| `(EQUAL x y)` | `T` if they have the same structure and exact same type |

### Arithmetic

| Function | Description |
|---|---|
| `(+ e1 e2 ...)` | Variadic sum. `(+)` → `0` |
| `(- e1 e2 ...)` | Variadic subtraction. `(- x)` → negation |
| `(* e1 e2 ...)` | Variadic multiplication. `(*)` → `1` |
| `(/ e1 e2 ...)` | Exact division. Produces `TRAT` if there is no exact remainder |
| `(DIV a b)` | Integer quotient. `TINT` only. Truncates toward zero |
| `(MOD a b)` | Integer remainder. `TINT` only. Same sign as `a` |

### Comparators

| Function | Description |
|---|---|
| `(= a b)` | Numeric value equality (ignores type) |
| `(< a b)`, `(> a b)` | Strict less-than / greater-than |
| `(<= a b)`, `(>= a b)` | Less-or-equal / greater-or-equal |

### Utilities

| Function | Description |
|---|---|
| `(EVAL expr)` | Evaluates `expr` as code in the global environment |
| `(PRINT x)` | Prints `x` with quotes around strings and a trailing newline. Returns `x`. Use: debugging |
| `(DISPLAY x)` | Prints `x` without quotes and **without a trailing newline**. For `TSTRING`, shows the raw content. Returns a special value the REPL does not print. Use: user-facing output |
| `(NEWLINE)` | Emits a newline. No arguments. Natural complement to `DISPLAY` |
| `(SYMNAME sym)` | Converts a symbol to a string. Returns a `TSTRING` with the symbol's interned name. Truncated to 8 characters (the `SYMLEN` limit) |
| `(STR< a b)` | Alphabetic comparison. Returns `T` if `a` lexicographically precedes `b`. Accepts `TSTRING` or `TSYM` |
| `(STRCAT a b)` | Concatenates two values, converting them to text. Accepts `TSTRING`, `TSYM`, `TINT`, `TRAT`. Does not support `TFLOAT`. Result truncated to 36 characters |

---

## 13. Programming tips

### Strings vs. symbols: when to use each

MyLISP has two types for representing text: **symbols** (`TSYM`) and
**strings** (`TSTRING`). Although they may seem interchangeable at first
glance for some uses, they have very different characteristics that affect
performance and system limits.

| Characteristic | Symbol | String |
|---|---|---|
| Maximum length | 8 characters | 36 characters |
| Table used | `symtab` (200 entries) | `strtab` (50 entries) |
| GC handling | Permanent — never freed | Freed when no references remain |
| Self-evaluates to | Its value in the environment (or error) | The string itself |
| Typical use | Function and variable names | File names for `LOAD` |

**Rule of thumb:** use symbols for labels, test names, internal identifiers,
and any short text that repeats. Use strings only when you need text longer
than 8 significant characters or that will be passed to `LOAD`.

**Concrete example — test suite:**

A common mistake when writing test functions is using strings as labels:

```lisp
; BAD: each call creates a TSTRING cell in strtab (limit: 50)
(CHECK "sum test" (+ 1 2) 3)
(CHECK "sub test" (- 5 3) 2)
; ... with 50+ tests, the string table runs out
```

The solution is to use symbols as labels:

```lisp
; GOOD: symbols use symtab (limit: 200) and are permanent
(CHECK 'sum-test (+ 1 2) 3)
(CHECK 'sub-test (- 5 3) 2)
```

The visible output on a failing test is just as clear, and the program can
have many more tests without running into memory problems.

### DISPLAY vs. PRINT: when to use each

| | `PRINT` | `DISPLAY` |
|---|---|---|
| Strings | With quotes: `"hello"` | Without quotes: `hello` |
| Newline | Always, automatic | Never — use `(NEWLINE)` |
| Return value | The value itself (visible in the REPL) | `VOID` (the REPL does not print it) |
| Typical use | Debugging, seeing the exact type | End-user output |

To build an output line piece by piece:

```lisp
(DISPLAY "result: ")
(DISPLAY (+ 2 3))
(NEWLINE)
; prints: result: 5
```

### Comparing and sorting symbols

```lisp
(DEFUN SYM< (A B) (STR< (SYMNAME A) (SYMNAME B)))
(SYM< 'ALPHA 'BETA)   ; -> T
(SYM< 'ZETA 'ALPHA)   ; -> NIL
```

`STR<` also accepts symbols directly without going through `SYMNAME`, which
avoids creating an intermediate `TSTRING` cell:

```lisp
(STR< 'ALPHA 'BETA)   ; -> T  (more efficient than using SYMNAME)
```

### Deep recursion: a real advantage of this platform

Unlike a classic Lisp evaluator (where every recursive call in the program
consumes CPU stack), user recursion in MyLISP for the Next has no such
practical limit. You can write recursive functions hundreds of levels deep
without worrying about exhausting the stack:

```lisp
(DEFUN count (n) (if (= n 0) 0 (+ 1 (count (- n 1)))))
(count 500)   ; works fine
```

The real limit becomes the heap (32,000 cells) rather than call depth itself
— much more generous for the purely recursive, loop-free, mutation-free
style that is natural to LISP.

### Diagnostic and session-control tools

**`(STATUS)`** shows the state of the internal tables:

```
Heap: 245/32000 (1%)
Simbolos: 47/200
Strings: 12/50
```

**`(CLEAN)`** forces an immediate garbage collection (from the REPL). Useful
before operations that will generate a lot of temporary structure.

**`(NEW)`** resets the interpreter completely, asking for confirmation first.

**`(SYMBOLS)`** lists all user-defined symbols.

### Symbol names and the 8-character limit

Only the first 8 characters of a symbol name are significant:

```lisp
(DEFUN CALCULATE-SUM (X Y) (+ X Y))
(DEFUN CALCULATE-PRO (X Y) (* X Y))
; Both are stored as CALCULAT -- they are the same symbol
; The second definition overwrites the first
```

Function and variable names should be designed so that their first 8
characters are unique and descriptive.
