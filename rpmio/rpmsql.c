#include "system.h"

#include <popt.h>

#define	_RPMSQL_INTERNAL
#include <rpmsql.h>
#include <rpmmacro.h>

#if defined(WITH_SQLITE)
#define SQLITE_OS_UNIX 1
#define SQLITE_THREADSAFE 1
#define SQLITE_THREAD_OVERRIDE_LOCK -1
#define SQLITE_TEMP_STORE 1
#include <sqlite3.h>
#endif	/* WITH_SQLITE */

#if defined(HAVE_EDITLINE) && HAVE_EDITLINE==1
#include <editline/readline.h>
#elif defined(HAVE_READLINE) && HAVE_READLINE==1
# include <readline/readline.h>
# include <readline/history.h>
#else
# define readline(p) local_getline(p,stdin)
# define add_history(X)
# define read_history(X)
# define write_history(X)
# define stifle_history(X)
#endif

#include "debug.h"

/*@unchecked@*/
int _rpmsql_debug = 0;

/*@unchecked@*/ /*@relnull@*/
rpmsql _rpmsqlI = NULL;

/*@unchecked@*/
volatile int _rpmsqlSeenInterrupt;

#if defined(WITH_SQLITE)
/*@unchecked@*/
static struct rpmsql_s _sql;
#endif /* defined(WITH_SQLITE) */

/*==============================================================*/
#ifdef	UNUSED
static void rpmsqlDebugDump(rpmsql sql)
{
SQLDBG((stderr, "==> %s(%p) _rpmsqlI %p\n", __FUNCTION__, sql, _rpmsqlI));
    if (sql) {
	fprintf(stderr, "\t    flags: 0x%x\n", sql->flags);
	fprintf(stderr, "\t       av: %p[%u]\n", sql->av, (unsigned)argvCount(sql->av));
	fprintf(stderr, "\t        I: %p\n", sql->I);
	fprintf(stderr, "\t        S: %p\n", sql->S);
	fprintf(stderr, "\t     init: %s\n", sql->zInitFile);
	fprintf(stderr, "\t database: %s\n", sql->zDbFilename);
	fprintf(stderr, "\t    table: %s\n", sql->zDestTable);

	fprintf(stderr, "\t     mode: 0x%x\n", sql->mode);
	fprintf(stderr, "\t      cnt: 0x%x\n", sql->cnt);
	fprintf(stderr, "\t      iob: %p\n", sql->iob);
	fprintf(stderr, "\t      out: %p\n", sql->out);
	fprintf(stderr, "\t      log: %p\n", sql->pLog);
	fprintf(stderr, "\t    trace: %p\n", sql->iotrace);

	if (sql->explainPrev.valid) {
	    fprintf(stderr, "\t  explain:\n");
	    fprintf(stderr, "\t\t mode: 0x%x\n", sql->explainPrev.mode);
	    fprintf(stderr, "\t\tflags: 0x%x\n", sql->explainPrev.flags);
	}

	fprintf(stderr, "\tseparator: %.*s\n", (int)sizeof(sql->separator), sql->separator);
	fprintf(stderr, "\tnullvalue: %.*s\n", (int)sizeof(sql->nullvalue), sql->nullvalue);
	fprintf(stderr, "\t  outfile: %.*s\n", (int)sizeof(sql->outfile), sql->outfile);
	fprintf(stderr, "\t     home: %s\n", sql->zHome);
	fprintf(stderr, "\t   initrc: %s\n", sql->zInitrc);
	fprintf(stderr, "\t  history: %s\n", sql->zHistory);
	fprintf(stderr, "\t   prompt: %s\n", sql->zPrompt);
	fprintf(stderr, "\t continue: %s\n", sql->zContinue);
    }
}
#endif

#if defined(WITH_SQLITE)
/**
 * Print an error message and exit (if requested).
 * @param lvl		error level (non-zero exits)
 * @param fmt		msg format
 */
/*@mayexit@*/
static void
rpmsql_error(int lvl, const char *fmt, ...)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/
{
    va_list ap;

    va_start(ap, fmt);
    (void) fflush(NULL);
#ifdef	NOTYET
    (void) fprintf(stderr, "%s: ", __progname);
#endif
    (void) vfprintf(stderr, fmt, ap);
    va_end (ap);
    (void) fprintf(stderr, "\n");
    if (lvl)
	exit(EXIT_FAILURE);
}

/**
 * Check sqlite3 return, displaying error messages.
 * @param sql		sql interpreter
 * @return		SQLITE_OK on success
 */
static int rpmsqlCmd(rpmsql sql, const char * msg, void * _db, int rc)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/
{
    if (rc != SQLITE_OK || _rpmsql_debug) {
	sqlite3 * db = (sqlite3 *) (_db ? _db : sql->I);
	rpmsql_error(0, "sqlite3_%s: %s", msg,
		sqlite3_errmsg(db));
    }
    return rc;
}
#endif	/* defined(WITH_SQLITE) */

/*==============================================================*/

#if defined(WITH_SQLITE)
/*
** Begin timing an operation
*/
static void _rpmsqlBeginTimer(rpmsql sql)
{
    if (sql->enableTimer)
	getrusage(RUSAGE_SELF, &sql->sBegin);
}

/* Return the difference of two time_structs in seconds */
static double timeDiff(struct timeval *pStart, struct timeval *pEnd)
{
    return (pEnd->tv_usec - pStart->tv_usec) * 0.000001 +
	(double) (pEnd->tv_sec - pStart->tv_sec);
}

/*
** Print the timing results.
*/
static void _rpmsqlEndTimer(rpmsql sql)
{
    if (sql->enableTimer) {
	struct rusage sEnd;
	getrusage(RUSAGE_SELF, &sEnd);
	printf("CPU Time: user %f sys %f\n",
	       timeDiff(&sql->sBegin.ru_utime, &sEnd.ru_utime),
	       timeDiff(&sql->sBegin.ru_stime, &sEnd.ru_stime));
    }
}

#define BEGIN_TIMER(_sql)	 _rpmsqlBeginTimer(_sql)
#define END_TIMER(_sql)		 _rpmsqlEndTimer(_sql)
#define HAS_TIMER 1

#define ArraySize(X)  (int)(sizeof(X)/sizeof(X[0]))
#endif	/* defined(WITH_SQLITE) */

/*==============================================================*/

static rpmsql rpmsqlI(void)
	/*@globals _rpmsqlI @*/
	/*@modifies _rpmsqlI @*/
{
    if (_rpmsqlI == NULL)
	_rpmsqlI = rpmsqlNew(NULL, 0);
SQLDBG((stderr, "<== %s() _rpmsqlI %p\n", __FUNCTION__, _rpmsqlI));
    return _rpmsqlI;
}

#if defined(WITH_SQLITE)
static int rpmsqlFprintf(rpmsql sql, const char *fmt, ...)
{
    char b[BUFSIZ];
    size_t nb = sizeof(b);
    int rc;
    va_list ap;

    if (sql == NULL) sql = rpmsqlI();
assert(sql);

    /* Format the output */
    va_start(ap, fmt);
    rc = vsnprintf(b, nb, fmt, ap);
    va_end(ap);
    /* XXX just in case */
    if (!(rc >= 0 && rc < (int)nb))
	rc = nb - 1;
    b[rc] = '\0';

    /* Dispose of the output. */
    if (sql->out) {
	FILE * out = (FILE *) sql->out;
	size_t nw = fwrite(b, 1, rc, out);
assert((int)nw == rc);
    }
    if (sql->iob)
	(void) rpmiobAppend(sql->iob, b, 0);

    return rc;
}

/*
** This routine works like printf in that its first argument is a
** format string and subsequent arguments are values to be substituted
** in place of % fields.  The result of formatting this string
** is written to iotrace.
*/
#ifdef SQLITE_ENABLE_IOTRACE
static void iotracePrintf(const char *zFormat, ...)
{
    va_list ap;
    char *z;
    if (_rpmsqlI == NULL || _rpmsqlI->iotrace == NULL)
	return;
    va_start(ap, zFormat);
    z = sqlite3_vmprintf(zFormat, ap);
    va_end(ap);
    fprintf(_rpmsqlI->iotrace, "%s", z);
    sqlite3_free(z);
}
#endif

#if defined(SQLITE_CONFIG_LOG)
/*
** A callback for the sqlite3_log() interface.
*/
static void shellLog(void *pArg, int iErrCode, const char *zMsg)
{
    rpmsql sql = (rpmsql) pArg;
    if (sql && sql->pLog) {
	fprintf(sql->pLog, "(%d) %s\n", iErrCode, zMsg);
	fflush(sql->pLog);
    }
}
#endif

/*==============================================================*/
/*
** X is a pointer to the first byte of a UTF-8 character.  Increment
** X so that it points to the next character.  This only works right
** if X points to a well-formed UTF-8 string.
*/
#ifdef	NOTYET	/* XXX figger multibyte char's. */
#define sqliteNextChar(X)	while( (0xc0&*++(X))==0x80 ){}
#define sqliteCharVal(X)	sqlite3ReadUtf8(X)
#else
#define sqliteNextChar(X)	while( (     *++(X))       ) break
#define sqliteCharVal(X)	        (int)(*(X))
#endif

#include	<math.h>

/**
 * This is a macro that facilitates writting wrappers for math.h functions
 * it creates code for a function to use in SQlite that gets one numeric input
 * and returns a floating point value.
 *
 * Could have been implemented using pointers to functions but this way it's inline
 * and thus more efficient. Lower * ranking though...
 * 
 * Parameters:
 * name:      function name to de defined (eg: sinFunc)
 * function:  function defined in math.h to wrap (eg: sin)
 * domain:    boolean condition that CAN'T happen in terms of the input parameter rVal
 *            (eg: rval<0 for sqrt)
 */
#define GEN_MATH_WRAP_DOUBLE_1(name, function, domain) \
static void name(sqlite3_context *context, int argc, sqlite3_value **argv) {\
  double rVal = 0.0;\
assert(argc==1);\
  switch (sqlite3_value_type(argv[0])) {\
    case SQLITE_NULL:\
      sqlite3_result_null(context);\
      break;\
    default:\
      rVal = sqlite3_value_double(argv[0]);\
      if (domain)\
        sqlite3_result_error(context, "domain error", -1);\
      else\
        sqlite3_result_double(context, function(rVal));\
      break;\
  }\
}

/**
 * Example of GEN_MATH_WRAP_DOUBLE_1 usage
 * this creates function sqrtFunc to wrap the math.h standard function sqrt(x)=x^0.5
 * notice the domain rVal<0 is the condition that signals a domain error HAS occured
 */
GEN_MATH_WRAP_DOUBLE_1(sqrtFunc, sqrt, rVal < 0)

/* trignometric functions */
GEN_MATH_WRAP_DOUBLE_1(acosFunc, acos, rVal < -1.0 || rVal > 1.0)
GEN_MATH_WRAP_DOUBLE_1(asinFunc, asin, rVal < -1.0 || rVal > 1.0)
GEN_MATH_WRAP_DOUBLE_1(atanFunc, atan, 0)

/**
 * Many of systems don't have inverse hyperbolic trig functions so this will emulate
 * them on those systems in terms of log and sqrt (formulas are too trivial to demand 
 * written proof here)
 */
#ifdef REFERENCE
static double acosh(double x)
{
    return log(x + sqrt(x * x - 1.0));
}
#endif

GEN_MATH_WRAP_DOUBLE_1(acoshFunc, acosh, rVal < 1)
#ifdef REFERENCE
static double asinh(double x)
{
    return log(x + sqrt(x * x + 1.0));
}
#endif

GEN_MATH_WRAP_DOUBLE_1(asinhFunc, asinh, 0)
#ifdef REFERENCE
static double atanh(double x)
{
    return (1.0 / 2.0) * log((1 + x) / (1 - x));
}
#endif

GEN_MATH_WRAP_DOUBLE_1(atanhFunc, atanh, rVal > 1.0 || rVal < -1.0)

/**
 * math.h doesn't require cot (cotangent) so it's defined here
 */
static double cot(double x)
{
    return 1.0 / tan(x);
}

GEN_MATH_WRAP_DOUBLE_1(sinFunc, sin, 0)
GEN_MATH_WRAP_DOUBLE_1(cosFunc, cos, 0)
GEN_MATH_WRAP_DOUBLE_1(tanFunc, tan, 0)		/* XXX DOMAIN */
GEN_MATH_WRAP_DOUBLE_1(cotFunc, cot, 0)		/* XXX DOMAIN */

static double coth(double x)
{
    return 1.0 / tanh(x);
}

/**
 * Many systems don't have hyperbolic trigonometric functions so this will emulate
 * them on those systems directly from the definition in terms of exp
 */
#ifdef REFERENCE
static double sinh(double x)
{
    return (exp(x) - exp(-x)) / 2.0;
}
#endif
GEN_MATH_WRAP_DOUBLE_1(sinhFunc, sinh, 0)

#ifdef REFERENCE
static double cosh(double x)
{
    return (exp(x) + exp(-x)) / 2.0;
}
#endif
GEN_MATH_WRAP_DOUBLE_1(coshFunc, cosh, 0)

#ifdef REFERENCE
static double tanh(double x)
{
    return sinh(x) / cosh(x);
}
#endif
GEN_MATH_WRAP_DOUBLE_1(tanhFunc, tanh, 0)
GEN_MATH_WRAP_DOUBLE_1(cothFunc, coth, 0)	/* XXX DOMAIN */

/**
 * Some systems lack log in base 10. This will emulate.
 */
#ifdef REFERENCE
static double log10(double x)
{
    static double l10 = -1.0;
    if (l10 < 0.0) {
	l10 = log(10.0);
    }
    return log(x) / l10;
}
#endif
GEN_MATH_WRAP_DOUBLE_1(logFunc, log, rVal <= 0.0)
GEN_MATH_WRAP_DOUBLE_1(log10Func, log10, rVal <= 0.0)
GEN_MATH_WRAP_DOUBLE_1(expFunc, exp, 0)

/**
 * Fallback for systems where math.h doesn't define M_PI
 */
#ifndef M_PI
/**
 * static double PI = acos(-1.0);
 * #define M_PI (PI)
 */
#define M_PI 3.14159265358979323846
#endif

/**
 * Convert Degrees into Radians.
 */
static double deg2rad(double x)
{
    return x * M_PI / 180.0;
}

/**
 * Convert Radians into Degrees.
 */
static double rad2deg(double x)
{
    return 180.0 * x / M_PI;
}
GEN_MATH_WRAP_DOUBLE_1(rad2degFunc, rad2deg, 0)
GEN_MATH_WRAP_DOUBLE_1(deg2radFunc, deg2rad, 0)

/**
 * constant function that returns the value of PI=3.1415...
 */
static void piFunc(sqlite3_context * context,
		int argc, sqlite3_value ** argv)
{
    sqlite3_result_double(context, M_PI);
}

/**
 * Implements the sqrt function, it has the peculiarity of returning an integer when the
 * the argument is an integer.
 * Since SQLite isn't strongly typed (almost untyped actually) this is a bit pedantic
 */
static void squareFunc(sqlite3_context * context,
		int argc, sqlite3_value ** argv)
{
    double rVal = 0.0;
    int64_t iVal;

assert(argc == 2);
    switch (sqlite3_value_type(argv[0])) {
    case SQLITE_INTEGER:
	iVal = sqlite3_value_int64(argv[0]);
	sqlite3_result_int64(context, iVal * iVal);
	break;
    case SQLITE_NULL:
	sqlite3_result_null(context);
	break;
    default:
	rVal = sqlite3_value_double(argv[0]);
	sqlite3_result_double(context, rVal * rVal);
	break;
    }
}

/**
 * Wraps the pow math.h function.
 * When both the base and the exponent are integers the result should be integer
 * (see sqrt just before this). Here the result is always double
 */
static void powerFunc(sqlite3_context * context,
		int argc, sqlite3_value ** argv)
{
    double r1 = 0.0;
    double r2 = 0.0;

    assert(argc == 2);

    if (sqlite3_value_type(argv[0]) == SQLITE_NULL
	|| sqlite3_value_type(argv[1]) == SQLITE_NULL) {
	sqlite3_result_null(context);
    } else {
	r1 = sqlite3_value_double(argv[0]);
	r2 = sqlite3_value_double(argv[1]);
	if (r1 <= 0.0) {
	    /* base must be positive */
	    sqlite3_result_error(context, "domain error", -1);
	} else {
	    sqlite3_result_double(context, pow(r1, r2));
	}
    }
}

/**
 * atan2 wrapper.
 */
static void atn2Func(sqlite3_context * context,
		int argc, sqlite3_value ** argv)
{
    double r1 = 0.0;
    double r2 = 0.0;

    assert(argc == 2);

    if (sqlite3_value_type(argv[0]) == SQLITE_NULL
	|| sqlite3_value_type(argv[1]) == SQLITE_NULL) {
	sqlite3_result_null(context);
    } else {
	r1 = sqlite3_value_double(argv[0]);
	r2 = sqlite3_value_double(argv[1]);
	sqlite3_result_double(context, atan2(r1, r2));
    }
}

/**
 * Implementation of the sign() function.
 * return one of 3 possibilities +1,0 or -1 when the argument is respectively
 * positive, 0 or negative.
 * When the argument is NULL the result is also NULL (completly conventional)
 */
static void signFunc(sqlite3_context * context,
		int argc, sqlite3_value ** argv)
{
    double rVal = 0.0;
    int64_t iVal;

assert(argc == 1);
    switch (sqlite3_value_type(argv[0])) {
    case SQLITE_INTEGER:
	iVal = sqlite3_value_int64(argv[0]);
	iVal = (iVal > 0) ? 1 : (iVal < 0) ? -1 : 0;
	sqlite3_result_int64(context, iVal);
	break;
    case SQLITE_NULL:
	sqlite3_result_null(context);
	break;
    default:
	/* 2nd change below. Line for abs was: if( rVal<0 ) rVal = rVal * -1.0;  */

	rVal = sqlite3_value_double(argv[0]);
	rVal = (rVal > 0) ? 1 : (rVal < 0) ? -1 : 0;
	sqlite3_result_double(context, rVal);
	break;
    }
}

/**
 * smallest integer value not less than argument.
 */
static void ceilFunc(sqlite3_context * context,
		int argc, sqlite3_value ** argv)
{
    double rVal = 0.0;
    int64_t iVal;

assert(argc == 1);
    switch (sqlite3_value_type(argv[0])) {
    case SQLITE_INTEGER:
	iVal = sqlite3_value_int64(argv[0]);
	sqlite3_result_int64(context, iVal);
	break;
    case SQLITE_NULL:
	sqlite3_result_null(context);
	break;
    default:
	rVal = sqlite3_value_double(argv[0]);
	sqlite3_result_int64(context, ceil(rVal));
	break;
    }
}

/**
 * largest integer value not greater than argument.
 */
static void floorFunc(sqlite3_context * context,
		int argc, sqlite3_value ** argv)
{
    double rVal = 0.0;
    int64_t iVal;

assert(argc == 1);
    switch (sqlite3_value_type(argv[0])) {
    case SQLITE_INTEGER:
	iVal = sqlite3_value_int64(argv[0]);
	sqlite3_result_int64(context, iVal);
	break;
    case SQLITE_NULL:
	sqlite3_result_null(context);
	break;
    default:
	rVal = sqlite3_value_double(argv[0]);
	sqlite3_result_int64(context, floor(rVal));
	break;
    }
}

/*
** Given a string (s) in the first argument and an integer (n) in the second returns the 
** string that constains s contatenated n times
*/
static void replicateFunc(sqlite3_context * context,
		int argc, sqlite3_value ** argv)
{
    unsigned char *z;		/* input string */
    unsigned char *zo;		/* result string */
    int iCount;			/* times to repeat */
    size_t nLen;		/* length of the input string (no multibyte considerations) */
    size_t nTLen;		/* length of the result string (no multibyte considerations) */
    int i = 0;

    if (argc != 2 || SQLITE_NULL == sqlite3_value_type(argv[0]))
	return;

    iCount = sqlite3_value_int64(argv[1]);

    if (iCount < 0) {
	sqlite3_result_error(context, "domain error", -1);
    } else {
	nLen = sqlite3_value_bytes(argv[0]);
	nTLen = nLen * iCount;
	z = xmalloc(nTLen + 1);
	zo = xmalloc(nLen + 1);
	strcpy((char *) zo, (char *) sqlite3_value_text(argv[0]));

	for (i = 0; i < iCount; ++i)
	    strcpy((char *) (z + i * nLen), (char *) zo);

	sqlite3_result_text(context, (char *) z, -1, free);
	zo = _free(zo);
    }
}

static void properFunc(sqlite3_context * context,
		int argc, sqlite3_value ** argv)
{
    const unsigned char *z;	/* input string */
    unsigned char *zo;		/* output string */
    unsigned char *zt;		/* iterator */
    char r;
    int c = 1;

assert(argc == 1);
    if (SQLITE_NULL == sqlite3_value_type(argv[0])) {
	sqlite3_result_null(context);
	return;
    }

    z = sqlite3_value_text(argv[0]);
    zo = (unsigned char *) xstrdup((const char *)z);
    zt = zo;

    while ((r = *(z++)) != 0) {
	if (xisblank(r)) {
	    c = 1;
	} else {
	    r = (c == 1) ? xtoupper(r) : xtolower(r);
	    c = 0;
	}
	*(zt++) = r;
    }
    *zt = '\0';

    sqlite3_result_text(context, (char *) zo, -1, free);
}

#ifdef	NOTYET	/* XXX figger multibyte char's. */
/*
** given an input string (s) and an integer (n) adds spaces at the begining of  s
** until it has a length of n characters.
** When s has a length >= n it's a NOP
** padl(NULL) = NULL
*/
static void padlFunc(sqlite3_context * context,
		int argc, sqlite3_value ** argv)
{
    size_t ilen;		/* length to pad to */
    size_t zl;			/* length of the input string (UTF-8 chars) */
    size_t i;
    const char *zi;		/* input string */
    char *zo;			/* output string */
    char *zt;

assert(argc == 2);
    if (sqlite3_value_type(argv[0]) == SQLITE_NULL) {
	sqlite3_result_null(context);
    } else {
	zi = (const char *) sqlite3_value_text(argv[0]);
	ilen = sqlite3_value_int64(argv[1]);
	/* check domain */
	if (ilen < 0) {
	    sqlite3_result_error(context, "domain error", -1);
	    return;
	}
	zl = sqlite3utf8CharLen(zi, -1);
	if (zl >= ilen) {
	    /* string is longer than the requested pad length, return the same string (dup it) */
	    sqlite3_result_text(context, xstrdup(zi), -1, free);
	} else {
	    zo = xmalloc(strlen(zi) + ilen - zl + 1);
	    zt = zo;
	    for (i = 1; i + zl <= ilen; ++i)
		*(zt++) = ' ';
	    /* no need to take UTF-8 into consideration here */
	    strcpy(zt, zi);
	    sqlite3_result_text(context, zo, -1, free);
	}
    }
}

/*
** given an input string (s) and an integer (n) appends spaces at the end of  s
** until it has a length of n characters.
** When s has a length >= n it's a NOP
** padl(NULL) = NULL
*/
static void padrFunc(sqlite3_context * context,
		int argc, sqlite3_value ** argv)
{
    size_t ilen;		/* length to pad to */
    size_t zl;			/* length of the input string (UTF-8 chars) */
    size_t zll;			/* length of the input string (bytes) */
    size_t i;
    const char *zi;		/* input string */
    char *zo;			/* output string */
    char *zt;

assert(argc == 2);
    if (sqlite3_value_type(argv[0]) == SQLITE_NULL) {
	sqlite3_result_null(context);
    } else {
	int64_t _ilen;
	zi = (const char *) sqlite3_value_text(argv[0]);
	_ilen = sqlite3_value_int64(argv[1]);
	/* check domain */
	if (_ilen < 0) {
	    sqlite3_result_error(context, "domain error", -1);
	    return;
	}
	ilen = _ilen;
	zl = sqlite3utf8CharLen(zi, -1);
	if (zl >= ilen) {
	    /* string is longer than the requested pad length, return the same string (dup it) */
	    sqlite3_result_text(context, xstrdup(zi), -1, free);
	} else {
	    zll = strlen(zi);
	    zo = xmalloc(zll + ilen - zl + 1);
	    zt = strcpy(zo, zi) + zll;
	    for (i = 1; i + zl <= ilen; ++i)
		*(zt++) = ' ';
	    *zt = '\0';
	    sqlite3_result_text(context, zo, -1, free);
	}
    }
}

/*
** given an input string (s) and an integer (n) appends spaces at the end of  s
** and adds spaces at the begining of s until it has a length of n characters.
** Tries to add has many characters at the left as at the right.
** When s has a length >= n it's a NOP
** padl(NULL) = NULL
*/
static void padcFunc(sqlite3_context * context,
		int argc, sqlite3_value ** argv)
{
    size_t ilen;		/* length to pad to */
    size_t zl;			/* length of the input string (UTF-8 chars) */
    size_t zll;			/* length of the input string (bytes) */
    size_t i;
    const char *zi;		/* input string */
    char *zo;			/* output string */
    char *zt;

assert(argc == 2);
    if (sqlite3_value_type(argv[0]) == SQLITE_NULL) {
	sqlite3_result_null(context);
    } else {
	int64_t _ilen;
	zi = (const char *) sqlite3_value_text(argv[0]);
	_ilen = sqlite3_value_int64(argv[1]);
	/* check domain */
	if (_ilen < 0) {
	    sqlite3_result_error(context, "domain error", -1);
	    return;
	}
	ilen = _ilen;
	zl = sqlite3utf8CharLen(zi, -1);
	if (zl >= ilen) {
	    /* string is longer than the requested pad length, return the same string (dup it) */
	    sqlite3_result_text(context, xstrdup(zi), -1, free);
	} else {
	    zll = strlen(zi);
	    zo = xmalloc(zll + ilen - zl + 1);
	    zt = zo;
	    for (i = 1; 2 * i + zl <= ilen; ++i)
		*(zt++) = ' ';
	    strcpy(zt, zi);
	    zt += zll;
	    for (; i + zl <= ilen; ++i)
		*(zt++) = ' ';
	    *zt = '\0';
	    sqlite3_result_text(context, zo, -1, free);
	}
    }
}
#endif

/*
** given 2 string (s1,s2) returns the string s1 with the characters NOT in s2 removed
** assumes strings are UTF-8 encoded
*/
static void strfilterFunc(sqlite3_context * context,
		int argc, sqlite3_value ** argv)
{
    const char *zi1;		/* first parameter string (searched string) */
    const char *zi2;		/* second parameter string (vcontains valid characters) */
    const char *z1;
    const char *z21;
    const char *z22;
    char *zo;			/* output string */
    char *zot;
    int c1;
    int c2;

assert(argc == 2);

    if (sqlite3_value_type(argv[0]) == SQLITE_NULL
	|| sqlite3_value_type(argv[1]) == SQLITE_NULL) {
	sqlite3_result_null(context);
    } else {
	zi1 = (const char *) sqlite3_value_text(argv[0]);
	zi2 = (const char *) sqlite3_value_text(argv[1]);
	zo = xmalloc(strlen(zi1) + 1);
	zot = zo;
	z1 = zi1;
	while ((c1 = sqliteCharVal(z1)) != 0) {
	    z21 = zi2;
	    while ((c2 = sqliteCharVal(z21)) != 0 && c2 != c1)
		sqliteNextChar(z21);
	    if (c2 != 0) {
		z22 = z21;
		sqliteNextChar(z22);
		strncpy(zot, z21, z22 - z21);
		zot += z22 - z21;
	    }
	    sqliteNextChar(z1);
	}
	*zot = '\0';

	sqlite3_result_text(context, zo, -1, free);
    }
}

/*
** Given a string z1, retutns the (0 based) index of it's first occurence
** in z2 after the first s characters.
** Returns -1 when there isn't a match.
** updates p to point to the character where the match occured.
** This is an auxiliary function.
*/
static int _substr(const char *z1, const char *z2, int s, const char **p)
{
    int c = 0;
    int rVal = -1;
    const char *zt1;
    const char *zt2;
    int c1;
    int c2;

    if (*z1 == '\0')
	return -1;

    while ((sqliteCharVal(z2) != 0) && (c++) < s)
	sqliteNextChar(z2);

    c = 0;
    while ((sqliteCharVal(z2)) != 0) {
	zt1 = z1;
	zt2 = z2;

	do {
	    c1 = sqliteCharVal(zt1);
	    c2 = sqliteCharVal(zt2);
	    sqliteNextChar(zt1);
	    sqliteNextChar(zt2);
	} while (c1 == c2 && c1 != 0 && c2 != 0);

	if (c1 == 0) {
	    rVal = c;
	    break;
	}

	sqliteNextChar(z2);
	++c;
    }
    if (p)
	*p = z2;
    return rVal >= 0 ? rVal + s : rVal;
}

/*
** given 2 input strings (s1,s2) and an integer (n) searches from the nth character
** for the string s1. Returns the position where the match occured.
** Characters are counted from 1.
** 0 is returned when no match occurs.
*/

static void charindexFunc(sqlite3_context * context,
		int argc, sqlite3_value ** argv)
{
    const char *z1;	/* s1 string */
    const char *z2;	/* s2 string */
    int s = 0;
    int rVal = 0;

assert(argc == 2 || argc == 3);
    if (SQLITE_NULL == sqlite3_value_type(argv[0])
     || SQLITE_NULL == sqlite3_value_type(argv[1])) {
	sqlite3_result_null(context);
	return;
    }

    z1 = (const char *) sqlite3_value_text(argv[0]);
    z2 = (const char *) sqlite3_value_text(argv[1]);
    if (argc == 3) {
	s = sqlite3_value_int(argv[2]) - 1;
	if (s < 0)
	    s = 0;
    } else {
	s = 0;
    }

    rVal = _substr(z1, z2, s, NULL);
    sqlite3_result_int(context, rVal + 1);
}

/*
** given a string (s) and an integer (n) returns the n leftmost (UTF-8) characters
** if the string has a length <= n or is NULL this function is NOP
*/
static void leftFunc(sqlite3_context * context,
		int argc, sqlite3_value ** argv)
{
    int c = 0;
    int cc = 0;
    int l = 0;
    const unsigned char *z;	/* input string */
    const unsigned char *zt;
    unsigned char *rz;		/* output string */

assert(argc == 2);
    if (SQLITE_NULL == sqlite3_value_type(argv[0])
     || SQLITE_NULL == sqlite3_value_type(argv[1])) {
	sqlite3_result_null(context);
	return;
    }

    z = sqlite3_value_text(argv[0]);
    l = sqlite3_value_int(argv[1]);
    zt = z;

    while (sqliteCharVal(zt) && c++ < l)
	sqliteNextChar(zt);

    cc = zt - z;

    rz = xmalloc(zt - z + 1);
    strncpy((char *) rz, (char *) z, zt - z);
    *(rz + cc) = '\0';
    sqlite3_result_text(context, (char *) rz, -1, free);
}

/*
** given a string (s) and an integer (n) returns the n rightmost (UTF-8) characters
** if the string has a length <= n or is NULL this function is NOP
*/
static void rightFunc(sqlite3_context * context,
		int argc, sqlite3_value ** argv)
{
    int l = 0;
    int c = 0;
    int cc = 0;
    const char *z;
    const char *zt;
    const char *ze;
    char *rz;

assert(argc == 2);
    if (SQLITE_NULL == sqlite3_value_type(argv[0])
     || SQLITE_NULL == sqlite3_value_type(argv[1])) {
	sqlite3_result_null(context);
	return;
    }

    z = (const char *) sqlite3_value_text(argv[0]);
    l = sqlite3_value_int(argv[1]);
    zt = z;

    while (sqliteCharVal(zt) != 0) {
	sqliteNextChar(zt);
	++c;
    }

    ze = zt;
    zt = z;

    cc = c - l;
    if (cc < 0)
	cc = 0;

    while (cc-- > 0) {
	sqliteNextChar(zt);
    }

    rz = xmalloc(ze - zt + 1);
    strcpy((char *) rz, (char *) (zt));
    sqlite3_result_text(context, (char *) rz, -1, free);
}

/*
** removes the whitespaces at the begining of a string.
*/
static const char * ltrim(const char *s)
{
    while (*s == ' ')
	++s;
    return s;
}

/*
** removes the whitespaces at the end of a string.
** !mutates the input string!
*/
static const char * rtrim(char *s)
{
    char *ss = s + strlen(s) - 1;
    while (ss >= s && *ss == ' ')
	--ss;
    *(ss + 1) = '\0';
    return s;
}

/*
**  Removes the whitespace at the begining of a string
*/
static void ltrimFunc(sqlite3_context * context,
		int argc, sqlite3_value ** argv)
{
    const char *z;

assert(argc == 1);
    if (SQLITE_NULL == sqlite3_value_type(argv[0])) {
	sqlite3_result_null(context);
	return;
    }
    z = (const char *) sqlite3_value_text(argv[0]);
    sqlite3_result_text(context, xstrdup(ltrim(z)), -1, free);
}

/*
**  Removes the whitespace at the end of a string
*/
static void rtrimFunc(sqlite3_context * context,
		int argc, sqlite3_value ** argv)
{
    const char *z;

assert(argc == 1);
    if (SQLITE_NULL == sqlite3_value_type(argv[0])) {
	sqlite3_result_null(context);
	return;
    }
    z = (const char *) sqlite3_value_text(argv[0]);
    sqlite3_result_text(context, rtrim(xstrdup(z)), -1, free);
}

/*
**  Removes the whitespace at the begining and end of a string
*/
static void trimFunc(sqlite3_context * context,
		int argc, sqlite3_value ** argv)
{
    const char *z;

assert(argc == 1);
    if (SQLITE_NULL == sqlite3_value_type(argv[0])) {
	sqlite3_result_null(context);
	return;
    }
    z = (const char *) sqlite3_value_text(argv[0]);
    sqlite3_result_text(context, rtrim(xstrdup(ltrim(z))), -1, free);
}

/*
** given a pointer to a string s1, the length of that string (l1), a new string (s2)
** and it's length (l2) appends s2 to s1.
** All lengths in bytes.
** This is just an auxiliary function
*/
static void _append(char **s1, int l1, const char *s2, int l2)
{
    *s1 = xrealloc(*s1, (l1 + l2 + 1) * sizeof(char));
    strncpy((*s1) + l1, s2, l2);
    *(*(s1) + l1 + l2) = '\0';
}

/*
** given strings s, s1 and s2 replaces occurrences of s1 in s by s2
*/
static void replaceFunc(sqlite3_context * context,
		int argc, sqlite3_value ** argv)
{
    const char *z1;		/* string s (first parameter) */
    const char *z2;		/* string s1 (second parameter) string to look for */
    const char *z3;		/* string s2 (third parameter) string to replace occurrences of s1 with */
    size_t lz1;
    size_t lz2;
    size_t lz3;
    int lzo = 0;
    char *zo = 0;
    int ret = 0;
    const char *zt1;
    const char *zt2;

assert(argc == 3);
    if (SQLITE_NULL == sqlite3_value_type(argv[0])) {
	sqlite3_result_null(context);
	return;
    }

    z1 = (const char *)sqlite3_value_text(argv[0]);
    z2 = (const char *)sqlite3_value_text(argv[1]);
    z3 = (const char *)sqlite3_value_text(argv[2]);
    /* handle possible null values */
    if (z2 == NULL)
	z2 = "";
    if (z3 == NULL)
	z3 = "";

    lz1 = strlen(z1);
    lz2 = strlen(z2);
    lz3 = strlen(z3);

#if 0
    /* special case when z2 is empty (or null) nothing will be changed */
    if (0 == lz2) {
	sqlite3_result_text(context, xstrdup(z1), -1, free);
	return;
    }
#endif

    zt1 = z1;
    zt2 = z1;

    while (1) {
	ret = _substr(z2, zt1, 0, &zt2);

	if (ret < 0)
	    break;

	_append(&zo, lzo, zt1, zt2 - zt1);
	lzo += zt2 - zt1;
	_append(&zo, lzo, z3, lz3);
	lzo += lz3;

	zt1 = zt2 + lz2;
    }
    _append(&zo, lzo, zt1, lz1 - (zt1 - z1));
    sqlite3_result_text(context, zo, -1, free);
}

/*
** given a string returns the same string but with the characters in reverse order
*/
static void reverseFunc(sqlite3_context * context,
		int argc, sqlite3_value ** argv)
{
    const char *z;
    const char *zt;
    char *rz;
    char *rzt;
    size_t l;
    int i;

assert(argc == 1);
    if (SQLITE_NULL == sqlite3_value_type(argv[0])) {
	sqlite3_result_null(context);
	return;
    }
    z = (const char *)sqlite3_value_text(argv[0]);
    l = strlen(z);
    rz = xmalloc(l + 1);
    rzt = rz + l;
    *(rzt--) = '\0';

    zt = z;
    while (sqliteCharVal(zt) != 0) {
	z = zt;
	sqliteNextChar(zt);
	for (i = 1; zt - i >= z; ++i)
	    *(rzt--) = *(zt - i);
    }

    sqlite3_result_text(context, rz, -1, free);
}

static void expandFunc(sqlite3_context * context,
		int argc, sqlite3_value ** argv)
{
    sqlite3_result_text(context,
    	rpmExpand((const char *)sqlite3_value_text(argv[0]), NULL), -1, free);
}

typedef struct rpmsqlCF_s * rpmsqlCF;
struct rpmsqlCF_s {
    const char * zName;
    int8_t nArg;
    uint8_t argType;		/* 0: none.  1: db  2: (-1) */
    uint8_t eTextRep;		/* SQLITE_UTF8 or SQLITE_UTF16 */
    uint8_t  needCollSeq;
    void (*xFunc)  (sqlite3_context *, int, sqlite3_value **);
#ifdef	NOTYET
    void (*xStep)  (sqlite3_context *, int, sqlite3_value **);
    void (*xFinal) (sqlite3_context *);
#endif
};

static struct rpmsqlCF_s __CF[] = {
    /* math.h extensions */
  { "acos",		1, 0, SQLITE_UTF8,	0, acosFunc },
  { "asin",		1, 0, SQLITE_UTF8,	0, asinFunc },
  { "atan",		1, 0, SQLITE_UTF8,	0, atanFunc },
  { "atn2",		2, 0, SQLITE_UTF8,	0, atn2Func },
    /* XXX alias */
  { "atan2",		2, 0, SQLITE_UTF8,	0, atn2Func },
  { "acosh",		1, 0, SQLITE_UTF8,	0, acoshFunc },
  { "asinh",		1, 0, SQLITE_UTF8,	0, asinhFunc },
  { "atanh",		1, 0, SQLITE_UTF8,	0, atanhFunc },

#ifdef	NOTYET
  { "difference",	2, 0, SQLITE_UTF8,	0, differenceFunc },
#endif
  { "degrees",		1, 0, SQLITE_UTF8,	0, rad2degFunc },
  { "radians",		1, 0, SQLITE_UTF8,	0, deg2radFunc },

  { "cos",		1, 0, SQLITE_UTF8,	0, cosFunc },
  { "sin",		1, 0, SQLITE_UTF8,	0, sinFunc },
  { "tan",		1, 0, SQLITE_UTF8,	0, tanFunc },
  { "cot",		1, 0, SQLITE_UTF8,	0, cotFunc },
  { "cosh",		1, 0, SQLITE_UTF8,	0, coshFunc  },
  { "sinh",		1, 0, SQLITE_UTF8,	0, sinhFunc },
  { "tanh",		1, 0, SQLITE_UTF8,	0, tanhFunc },
  { "coth",		1, 0, SQLITE_UTF8,	0, cothFunc },

  { "exp",		1, 0, SQLITE_UTF8,	0, expFunc },
  { "log",		1, 0, SQLITE_UTF8,	0, logFunc },
  { "log10",		1, 0, SQLITE_UTF8,	0, log10Func },
  { "power",		2, 0, SQLITE_UTF8,	0, powerFunc },
  { "sign",		1, 0, SQLITE_UTF8,	0, signFunc },
  { "sqrt",		1, 0, SQLITE_UTF8,	0, sqrtFunc },
  { "square",		1, 0, SQLITE_UTF8,	0, squareFunc },

  { "ceil",		1, 0, SQLITE_UTF8,	0, ceilFunc },
  { "floor",		1, 0, SQLITE_UTF8,	0, floorFunc },

  { "pi",		0, 0, SQLITE_UTF8,	1, piFunc },

    /* string extensions */
  { "replicate",	2, 0, SQLITE_UTF8,	0, replicateFunc },
  { "charindex",	2, 0, SQLITE_UTF8,	0, charindexFunc },
  { "charindex",	3, 0, SQLITE_UTF8,	0, charindexFunc },
  { "leftstr",		2, 0, SQLITE_UTF8,	0, leftFunc },
  { "rightstr",		2, 0, SQLITE_UTF8,	0, rightFunc },
  { "ltrim",		1, 0, SQLITE_UTF8,	0, ltrimFunc },
  { "rtrim",		1, 0, SQLITE_UTF8,	0, rtrimFunc },
  { "trim",		1, 0, SQLITE_UTF8,	0, trimFunc },
  { "replace",		3, 0, SQLITE_UTF8,	0, replaceFunc },
  { "reverse",		1, 0, SQLITE_UTF8,	0, reverseFunc },
  { "proper",		1, 0, SQLITE_UTF8,	0, properFunc },
#ifdef	NOTYET	/* XXX figger multibyte char's. */
  { "padl",		2, 0, SQLITE_UTF8,	0, padlFunc },
  { "padr",		2, 0, SQLITE_UTF8,	0, padrFunc },
  { "padc",		2, 0, SQLITE_UTF8,	0, padcFunc },
#endif
  { "strfilter",	2, 0, SQLITE_UTF8,	0, strfilterFunc },

    /* RPM extensions. */
  { "expand",		1, 0, SQLITE_UTF8,	0, expandFunc		},
  { NULL,		0, 0, 0,		0, NULL			}
};
static rpmsqlCF _CF = __CF;

static int _rpmsqlLoadCF(rpmsql sql)
{
    sqlite3 * db = (sqlite3 *)sql->I;
    rpmsqlCF CF;
    int rc = 0;

SQLDBG((stderr, "--> %s(%p)\n", __FUNCTION__, sql));
    for (CF = _CF; CF->zName != NULL; CF++) {
	void * _pApp = NULL;
	int xx;

	switch (CF->argType) {
	default:
	case 0:	 _pApp = NULL;		break;
	case 1:	 _pApp = (void *)db;	break;
	case 2:	 _pApp = (void *)-1;	break;
	}

	xx = rpmsqlCmd(sql, "create_function", db,
		sqlite3_create_function(db, CF->zName, CF->nArg, CF->eTextRep,
			_pApp,	CF->xFunc, NULL, NULL));
SQLDBG((stderr, "\t%s(%s) xx %d\n", "sqlite3_create_function", CF->zName, xx));
	if (xx && rc == 0)
	    rc = xx;

#ifdef	NOTYET
	if (CF->needColSeq) {
	    FuncDef *pFunc = sqlite3FindFunction(db, CF->zName,
				strlen(CF_>zName), CF->nArg, CF->eTextRep, 0);
	    if (pFunc) pFunc->needCollSeq = 1;
	}
#endif

    }
SQLDBG((stderr, "<-- %s(%p) rc %d\n", __FUNCTION__, sql, rc));
    return rc;
}

/*==============================================================*/

/*
 ** Make sure the database is open.  If it is not, then open it.  If
 ** the database fails to open, print an error message and exit.
 */
static int _rpmsqlOpenDB(rpmsql sql)
{
    int rc = -1;	/* assume failure */
    sqlite3 * db;

assert(sql);

    db = (sqlite3 *)sql->I;
    if (db == NULL) {
	int rc;
	rc = rpmsqlCmd(sql, "open", db,		/* XXX watchout: arg order */
		sqlite3_open(sql->zDbFilename, &db));
	sql->I = db;
	if (db && rc == SQLITE_OK) {
	    (void) _rpmsqlLoadCF(sql);
	}

	if (db == NULL || sqlite3_errcode(db) != SQLITE_OK) {
	    /* XXX rpmlog */
	    fprintf(stderr, "Error: unable to open database \"%s\": %s\n",
		    sql->zDbFilename, sqlite3_errmsg(db));
	    goto exit;
	}
	/* Enable extension loading (if not disabled). */
	if (!F_ISSET(sql, NOLOAD))
	    (void) rpmsqlCmd(sql, "enable_load_extension", db,
			sqlite3_enable_load_extension(db, 1));
    }
    rc = 0;

exit:
SQLDBG((stderr, "<-- %s(%p) rc %d %s\n", __FUNCTION__, sql, rc, sql->zDbFilename));
    return rc;
}

#endif	/* defined(WITH_SQLITE) */

/*==============================================================*/

#if defined(WITH_SQLITE)
/*
** Determines if a string is a number of not.
*/
static int isNumber(const char *z, int *realnum)
{
    if (*z == '-' || *z == '+')
	z++;
    if (!isdigit(*z))
	return 0;
    z++;
    if (realnum)
	*realnum = 0;
    while (isdigit(*z))
	z++;
    if (*z == '.') {
	z++;
	if (!isdigit(*z))
	    return 0;
	while (isdigit(*z))
	    z++;
	if (realnum)
	    *realnum = 1;
    }
    if (*z == 'e' || *z == 'E') {
	z++;
	if (*z == '+' || *z == '-')
	    z++;
	if (!isdigit(*z))
	    return 0;
	while (isdigit(*z))
	    z++;
	if (realnum)
	    *realnum = 1;
    }
    return *z == 0;
}

/*
** Compute a string length that is limited to what can be stored in
** lower 30 bits of a 32-bit signed integer.
*/
static int strlen30(const char *z)
{
    const char *z2 = z;
    while (*z2)
	z2++;
    return 0x3fffffff & (int) (z2 - z);
}
#endif	/* defined(WITH_SQLITE) */

/*==============================================================*/
#if defined(WITH_SQLITE)
/*
** Output the given string as a hex-encoded blob (eg. X'1234' )
*/
static void output_hex_blob(rpmsql sql, const void *pBlob, int nBlob)
{
    char *zBlob = (char *) pBlob;
    int i;

SQLDBG((stderr, "--> %s(%p,%p[%u])\n", __FUNCTION__, sql, pBlob, (unsigned)nBlob));
    rpmsqlFprintf(sql, "X'");
    for (i = 0; i < nBlob; i++)
	rpmsqlFprintf(sql, "%02x", zBlob[i]);
    rpmsqlFprintf(sql, "'");
}

/*
** Output the given string as a quoted string using SQL quoting conventions.
*/
static void output_quoted_string(rpmsql sql, const char *z)
{
    int i;
    int nSingle = 0;
SQLDBG((stderr, "--> %s(%p,%s)\n", __FUNCTION__, sql, z));
    for (i = 0; z[i]; i++) {
	if (z[i] == '\'')
	    nSingle++;
    }
    if (nSingle == 0) {
	rpmsqlFprintf(sql, "'%s'", z);
    } else {
	rpmsqlFprintf(sql, "'");
	while (*z) {
	    for (i = 0; z[i] && z[i] != '\''; i++)
		;
	    if (i == 0) {
		rpmsqlFprintf(sql, "''");
		z++;
	    } else if (z[i] == '\'') {
		rpmsqlFprintf(sql, "%.*s''", i, z);
		z += i + 1;
	    } else {
		rpmsqlFprintf(sql, "%s", z);
		break;
	    }
	}
	rpmsqlFprintf(sql, "'");
    }
}

/*
** Output the given string as a quoted according to C or TCL quoting rules.
*/
static void output_c_string(rpmsql sql, const char *z)
{
    unsigned int c;
SQLDBG((stderr, "--> %s(%p,%s)\n", __FUNCTION__, sql, z));
    rpmsqlFprintf(sql, "\"");
    while ((c = *(z++)) != 0) {
	if (c == '\\')
	    rpmsqlFprintf(sql, "\\\\");
	else if (c == '\t')
	    rpmsqlFprintf(sql, "\\t");
	else if (c == '\n')
	    rpmsqlFprintf(sql, "\\n");
	else if (c == '\r')
	    rpmsqlFprintf(sql, "\\r");
	else if (!isprint(c))
	    rpmsqlFprintf(sql, "\\%03o", c & 0xff);
	else
	    rpmsqlFprintf(sql, "%c", c);
    }
    rpmsqlFprintf(sql, "\"");
}

/*
** Output the given string with characters that are special to
** HTML escaped.
*/
static void output_html_string(rpmsql sql, const char *z)
{
    int i;
SQLDBG((stderr, "--> %s(%p,%s)\n", __FUNCTION__, sql, z));
    while (*z) {
	for (i = 0; z[i]
	     && z[i] != '<'
	     && z[i] != '&'
	     && z[i] != '>' && z[i] != '\"' && z[i] != '\''; i++) {
	}
	if (i > 0)
	    rpmsqlFprintf(sql, "%.*s", i, z);
	if (z[i] == '<')
	    rpmsqlFprintf(sql, "&lt;");
	else if (z[i] == '&')
	    rpmsqlFprintf(sql, "&amp;");
	else if (z[i] == '>')
	    rpmsqlFprintf(sql, "&gt;");
	else if (z[i] == '\"')
	    rpmsqlFprintf(sql, "&quot;");
	else if (z[i] == '\'')
	    rpmsqlFprintf(sql, "&#39;");
	else
	    break;
	z += i + 1;
    }
}

/*
** If a field contains any character identified by a 1 in the following
** array, then the string must be quoted for CSV.
*/
static const char needCsvQuote[] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
};

/*
** Output a single term of CSV.  Actually, sql->separator is used for
** the separator, which may or may not be a comma.  sql->nullvalue is
** the null value.  Strings are quoted using ANSI-C rules.  Numbers
** appear outside of quotes.
*/
static void output_csv(rpmsql sql, const char *z, int bSep)
{
SQLDBG((stderr, "--> %s(%p,%s,0x%x)\n", __FUNCTION__, sql, z, bSep));
    if (z == 0) {
	rpmsqlFprintf(sql, "%s", sql->nullvalue);
    } else {
	int i;
	int nSep = strlen30(sql->separator);
	for (i = 0; z[i]; i++) {
	    if (needCsvQuote[((unsigned char *) z)[i]]
		|| (z[i] == sql->separator[0] &&
		    (nSep == 1 || memcmp(z, sql->separator, nSep) == 0))) {
		i = 0;
		break;
	    }
	}
	if (i == 0) {
	    rpmsqlFprintf(sql, "\"");
	    for (i = 0; z[i]; i++) {
		if (z[i] == '"')
		    rpmsqlFprintf(sql, "\"");
		rpmsqlFprintf(sql, "%c", z[i]);
	    }
	    rpmsqlFprintf(sql, "\"");
	} else {
	    rpmsqlFprintf(sql, "%s", z);
	}
    }
    if (bSep)
	rpmsqlFprintf(sql, "%s", sql->separator);
}

/*
** This is the callback routine that the shell
** invokes for each row of a query result.
*/
static int _rpmsqlShellCallback(void *pArg, int nArg, char **azArg, char **azCol,
			  int *aiType)
{
    rpmsql sql = (rpmsql) pArg;
    int w;
    int i;

SQLDBG((stderr, "--> %s(%p,%d,%p,%p,%p)\n", __FUNCTION__, pArg, nArg, azArg, azCol, aiType));
    switch (sql->mode) {
    case RPMSQL_MODE_LINE:
	w = 5;
	if (azArg == 0)
	    break;
	for (i = 0; i < nArg; i++) {
	    int len = strlen30(azCol[i] ? azCol[i] : "");
	    if (len > w)
		w = len;
	}
	if (sql->cnt++ > 0)
	    rpmsqlFprintf(sql, "\n");
	for (i = 0; i < nArg; i++)
	    rpmsqlFprintf(sql, "%*s = %s\n", w, azCol[i],
			azArg[i] ? azArg[i] : sql->nullvalue);
	break;
    case RPMSQL_MODE_EXPLAIN:
    case RPMSQL_MODE_COLUMN:
	if (sql->cnt++ == 0) {
	    for (i = 0; i < nArg; i++) {
		int n;
		w = (i < ArraySize(sql->colWidth) ? sql->colWidth[i] : 0);

		if (w <= 0) {
		    w = strlen30(azCol[i] ? azCol[i] : "");
		    if (w < 10)
			w = 10;
		    n = strlen30(azArg && azArg[i]
				? azArg[i] : sql-> nullvalue);
		    if (w < n)
			w = n;
		}
		if (i < ArraySize(sql->actualWidth))
		    sql->actualWidth[i] = w;
		if (F_ISSET(sql, SHOWHDR)) {
		    rpmsqlFprintf(sql, "%-*.*s%s", w, w, azCol[i],
				i == nArg - 1 ? "\n" : "  ");
		}
	    }
	    if (F_ISSET(sql, SHOWHDR)) {
		for (i = 0; i < nArg; i++) {
		    w = (i < ArraySize(sql->actualWidth)
				? sql->actualWidth[i] : 10);

		    rpmsqlFprintf(sql, "%-*.*s%s", w, w,
		"-----------------------------------"
		"----------------------------------------------------------",
				i == nArg - 1 ? "\n" : "  ");
		}
	    }
	}
	if (azArg == 0)
	    break;
	for (i = 0; i < nArg; i++) {
	    w = (i < ArraySize(sql->actualWidth) ? sql->actualWidth[i] : 10);
	    if (sql->mode == RPMSQL_MODE_EXPLAIN && azArg[i] &&
		strlen30(azArg[i]) > w) {
		w = strlen30(azArg[i]);
	    }
	    rpmsqlFprintf(sql, "%-*.*s%s", w, w,
			azArg[i] ? azArg[i] : sql->nullvalue,
			i == nArg - 1 ? "\n" : "  ");
	}
	break;
    case RPMSQL_MODE_SEMI:
    case RPMSQL_MODE_LIST:
	if (sql->cnt++ == 0 && F_ISSET(sql, SHOWHDR)) {
	    for (i = 0; i < nArg; i++)
		rpmsqlFprintf(sql, "%s%s", azCol[i],
			    i == nArg - 1 ? "\n" : sql->separator);
	}

	if (azArg == 0)
	    break;
	for (i = 0; i < nArg; i++) {
	    char *z = azArg[i];
	    if (z == 0)
		z = sql->nullvalue;
	    rpmsqlFprintf(sql, "%s", z);
	    if (i < nArg - 1)
		rpmsqlFprintf(sql, "%s", sql->separator);
	    else if (sql->mode == RPMSQL_MODE_SEMI)
		rpmsqlFprintf(sql, ";\n");
	    else
		rpmsqlFprintf(sql, "\n");
	}
	break;
    case RPMSQL_MODE_HTML:
	if (sql->cnt++ == 0 && F_ISSET(sql, SHOWHDR)) {
	    rpmsqlFprintf(sql, "<TR>");
	    for (i = 0; i < nArg; i++) {
		rpmsqlFprintf(sql, "<TH>");
		output_html_string(sql, azCol[i]);
		rpmsqlFprintf(sql, "</TH>\n");
	    }
	    rpmsqlFprintf(sql, "</TR>\n");
	}
	if (azArg == 0)
	    break;
	rpmsqlFprintf(sql, "<TR>");
	for (i = 0; i < nArg; i++) {
	    rpmsqlFprintf(sql, "<TD>");
	    output_html_string(sql, azArg[i] ? azArg[i] : sql->nullvalue);
	    rpmsqlFprintf(sql, "</TD>\n");
	}
	rpmsqlFprintf(sql, "</TR>\n");
	break;
    case RPMSQL_MODE_TCL:
	if (sql->cnt++ == 0 && F_ISSET(sql, SHOWHDR)) {
	    for (i = 0; i < nArg; i++) {
		output_c_string(sql, azCol[i] ? azCol[i] : "");
		rpmsqlFprintf(sql, "%s", sql->separator);
	    }
	    rpmsqlFprintf(sql, "\n");
	}
	if (azArg == 0)
	    break;
	for (i = 0; i < nArg; i++) {
	    output_c_string(sql, azArg[i] ? azArg[i] : sql->nullvalue);
	    rpmsqlFprintf(sql, "%s", sql->separator);
	}
	rpmsqlFprintf(sql, "\n");
	break;
    case RPMSQL_MODE_CSV:
	if (sql->cnt++ == 0 && F_ISSET(sql, SHOWHDR)) {
	    for (i = 0; i < nArg; i++)
		output_csv(sql, azCol[i] ? azCol[i] : "", i < nArg - 1);
	    rpmsqlFprintf(sql, "\n");
	}
	if (azArg == 0)
	    break;
	for (i = 0; i < nArg; i++) 
	    output_csv(sql, azArg[i], i < nArg - 1);
	rpmsqlFprintf(sql, "\n");
	break;
    case RPMSQL_MODE_INSERT:
	sql->cnt++;
	if (azArg == 0)
	    break;
	rpmsqlFprintf(sql, "INSERT INTO %s VALUES(", sql->zDestTable);
	for (i = 0; i < nArg; i++) {
	    char *zSep = i > 0 ? "," : "";
	    if ((azArg[i] == 0) || (aiType && aiType[i] == SQLITE_NULL)) {
		rpmsqlFprintf(sql, "%sNULL", zSep);
	    } else if (aiType && aiType[i] == SQLITE_TEXT) {
		if (zSep[0])
		    rpmsqlFprintf(sql, "%s", zSep);
		output_quoted_string(sql, azArg[i]);
	    } else if (aiType
			   && (aiType[i] == SQLITE_INTEGER
			       || aiType[i] == SQLITE_FLOAT)) {
		rpmsqlFprintf(sql, "%s%s", zSep, azArg[i]);
	    } else if (aiType && aiType[i] == SQLITE_BLOB && sql->S) {
		sqlite3_stmt * pStmt = (sqlite3_stmt *)sql->S;
		const void *pBlob = sqlite3_column_blob(pStmt, i);
		int nBlob = sqlite3_column_bytes(pStmt, i);
		if (zSep[0])
		    rpmsqlFprintf(sql, "%s", zSep);
		output_hex_blob(sql, pBlob, nBlob);
	    } else if (isNumber(azArg[i], 0)) {
		rpmsqlFprintf(sql, "%s%s", zSep, azArg[i]);
	    } else {
		if (zSep[0])
		    rpmsqlFprintf(sql, "%s", zSep);
		output_quoted_string(sql, azArg[i]);
	    }
	}
	rpmsqlFprintf(sql, ");\n");
	break;
    }
    return 0;
}

/*
** This is the callback routine that the SQLite library
** invokes for each row of a query result.
*/
static int callback(void *pArg, int nArg, char **azArg, char **azCol)
{
    /* since we don't have type info, call the _rpmsqlShellCallback with a NULL value */
    return _rpmsqlShellCallback(pArg, nArg, azArg, azCol, NULL);
}

/*
** Set the destination table field of the rpmsql object to
** the name of the table given.  Escape any quote characters in the
** table name.
*/
static void set_table_name(rpmsql sql, const char *zName)
{
    int i, n;
    int needQuote;
    char *z;

SQLDBG((stderr, "--> %s(%p,%s)\n", __FUNCTION__, sql, zName));
    sql->zDestTable = _free(sql->zDestTable);
    if (zName == NULL)
	return;
    needQuote = !isalpha((unsigned char) *zName) && *zName != '_';
    for (i = n = 0; zName[i]; i++, n++) {
	if (!isalnum((unsigned char) zName[i]) && zName[i] != '_') {
	    needQuote = 1;
	    if (zName[i] == '\'')
		n++;
	}
    }
    if (needQuote)
	n += 2;
    z = sql->zDestTable = xmalloc(n + 1);
    n = 0;
    if (needQuote)
	z[n++] = '\'';
    for (i = 0; zName[i]; i++) {
	z[n++] = zName[i];
	if (zName[i] == '\'')
	    z[n++] = '\'';
    }
    if (needQuote)
	z[n++] = '\'';
    z[n] = 0;
}

/* zIn is either a pointer to a NULL-terminated string in memory obtained
** from malloc(), or a NULL pointer. The string pointed to by zAppend is
** added to zIn, and the result returned in memory obtained from malloc().
** zIn, if it was not NULL, is freed.
**
** If the third argument, quote, is not '\0', then it is used as a 
** quote character for zAppend.
*/
static char *appendText(char *zIn, char const *zAppend, char quote)
{
    int len;
    int i;
    int nAppend = strlen30(zAppend);
    int nIn = (zIn ? strlen30(zIn) : 0);

SQLDBG((stderr, "--> %s(%s,%s,0x%02x)\n", __FUNCTION__, zIn, zAppend, quote));
    len = nAppend + nIn + 1;
    if (quote) {
	len += 2;
	for (i = 0; i < nAppend; i++) {
	    if (zAppend[i] == quote)
		len++;
	}
    }

    zIn = (char *) xrealloc(zIn, len);

    if (quote) {
	char *zCsr = &zIn[nIn];
	*zCsr++ = quote;
	for (i = 0; i < nAppend; i++) {
	    *zCsr++ = zAppend[i];
	    if (zAppend[i] == quote)
		*zCsr++ = quote;
	}
	*zCsr++ = quote;
	*zCsr++ = '\0';
	assert((zCsr - zIn) == len);
    } else {
	memcpy(&zIn[nIn], zAppend, nAppend);
	zIn[len - 1] = '\0';
    }

    return zIn;
}


/*
** Execute a query statement that has a single result column.  Print
** that result column on a line by itself with a semicolon terminator.
**
** This is used, for example, to show the schema of the database by
** querying the SQLITE_MASTER table.
*/
static int run_table_dump_query(rpmsql sql, sqlite3 * db,
				const char *zSelect, const char *zFirstRow)
{
    sqlite3_stmt * pSelect;
    int rc;
SQLDBG((stderr, "--> %s(%p,%p,%s,%s)\n", __FUNCTION__, sql, db, zSelect, zFirstRow));
    rc = rpmsqlCmd(sql, "prepare", db,
	sqlite3_prepare(db, zSelect, -1, &pSelect, 0));
    if (rc || pSelect == NULL)
	return rc;

    while ((rc = sqlite3_step(pSelect)) == SQLITE_ROW) {
	if (zFirstRow) {
	    rpmsqlFprintf(sql, "%s", zFirstRow);
	    zFirstRow = NULL;
	}
	rpmsqlFprintf(sql, "%s;\n", sqlite3_column_text(pSelect, 0));
    }

    return rpmsqlCmd(sql, "finalize", db,
	sqlite3_finalize(pSelect));
}
#endif	/* defined(WITH_SQLITE) */

/*==============================================================*/

#if defined(WITH_SQLITE)
/*
** This routine reads a line of text from FILE in, stores
** the text in memory obtained from malloc() and returns a pointer
** to the text.  NULL is returned at end of file, or if malloc()
** fails.
**
** The interface is like "readline" but no command-line editing
** is done.
*/
static char *local_getline(char *zPrompt, FILE * in)
{
    int nLine = 100;
    char * zLine = xmalloc(nLine);
    int n = 0;
    int eol = 0;

SQLDBG((stderr, "--> %s(%s,%p)\n", __FUNCTION__, zPrompt, in));
    if (zPrompt && *zPrompt) {
	printf("%s", zPrompt);
	fflush(stdout);
    }
    while (!eol) {
	if (n + 100 > nLine) {
	    nLine = nLine * 2 + 100;
	    zLine = xrealloc(zLine, nLine);
	}
	if (fgets(&zLine[n], nLine - n, in) == 0) {
	    if (n == 0) {
		free(zLine);
		return 0;
	    }
	    zLine[n] = 0;
	    eol = 1;
	    break;
	}
	while (zLine[n])
	    n++;
	if (n > 0 && zLine[n - 1] == '\n') {
	    n--;
	    if (n > 0 && zLine[n - 1] == '\r')
		n--;
	    zLine[n] = 0;
	    eol = 1;
	}
    }
    zLine = xrealloc(zLine, n + 1);
    return zLine;
}

/*
** Retrieve a single line of input text.
**
** zPrior is a string of prior text retrieved.  If not the empty
** string, then issue a continuation prompt.
*/
static char *rpmsqlInputOneLine(rpmsql sql, const char *zPrior, FILE * in)
{
    const char *zPrompt;
    char *zResult;
SQLDBG((stderr, "--> %s(%s,%p)\n", __FUNCTION__, zPrior, in));
    if (in != 0) {
	return local_getline(0, in);
    }
    if (zPrior && zPrior[0]) {
	zPrompt = sql->zContinue;
    } else {
	zPrompt = sql->zPrompt;
    }
    zResult = readline((char *)zPrompt);
#if defined(HAVE_READLINE) && HAVE_READLINE==1
    if (zResult && *zResult)
	add_history(zResult);
#endif
    return zResult;
}
#endif	/* defined(WITH_SQLITE) */

/*==============================================================*/

#if defined(WITH_SQLITE)
/*
** Allocate space and save off current error string.
*/
static char *save_err_msg(sqlite3 * db)
{
    const char * s = sqlite3_errmsg(db);
    int nb = strlen30(s) + 1;
    return memcpy(xmalloc(nb), s, nb);
}

/*
** Execute a statement or set of statements.  Print 
** any result rows/columns depending on the current mode 
** set via the supplied callback.
**
** This is very similar to SQLite's built-in sqlite3_exec() 
** function except it takes a slightly different callback 
** and callback data argument.
*/
static int _rpmsqlShellExec(rpmsql sql, const char *zSql,
		      int (*xCallback) (void *, int, char **, char **, int *),
		      char **pzErrMsg
    )
{
    sqlite3 * db = (sqlite3 *) sql->I;
    sqlite3_stmt * pStmt = NULL;	/* Statement to execute. */
    int rc = SQLITE_OK;		/* Return Code */
    const char *zLeftover;	/* Tail of unprocessed SQL */

SQLDBG((stderr, "--> %s(%p,%s,%p,%p)\n", __FUNCTION__, sql, zSql, xCallback, pzErrMsg));
    if (pzErrMsg)
	*pzErrMsg = NULL;

    while (zSql[0] && rc == SQLITE_OK) {
	rc = rpmsqlCmd(sql, "prepare_v2", db,
		sqlite3_prepare_v2(db, zSql, -1, &pStmt, &zLeftover));
	if (rc)
	    goto bottom;

	/* this happens for a comment or white-space */
	if (pStmt == NULL)
	    goto bottom;

	/* echo the sql statement if echo on */
	if (F_ISSET(sql, ECHO)) {
	    const char *zStmtSql = sqlite3_sql(pStmt);
	    rpmsqlFprintf(sql, "%s\n", zStmtSql ? zStmtSql : zSql);
	}

	/* perform the first step.  this will tell us if we
	 ** have a result set or not and how wide it is.
	 */
	rc = sqlite3_step(pStmt);
	/* if we have a result set... */
	if (rc == SQLITE_ROW) {
	    /* if we have a callback... */
	    if (xCallback) {
		/* allocate space for col name ptr, value ptr, and type */
		int nCol = sqlite3_column_count(pStmt);
		size_t nb = 3 * nCol * sizeof(const char *) + 1;
		char ** azCols = xmalloc(nb);		/* Result names */
		char ** azVals = &azCols[nCol];		/* Result values */
		int * aiTypes = (int *) &azVals[nCol];	/* Result types */
		int i;

		/* save off ptrs to column names */
		for (i = 0; i < nCol; i++)
		    azCols[i] = (char *) sqlite3_column_name(pStmt, i);

		/* save off the prepared statment handle and reset row count */
		sql->S = (void *) pStmt;
		sql->cnt = 0;
		do {
		    /* extract the data and data types */
		    for (i = 0; i < nCol; i++) {
			azVals[i] = (char *) sqlite3_column_text(pStmt, i);
			aiTypes[i] = sqlite3_column_type(pStmt, i);
			if (!azVals[i] && (aiTypes[i] != SQLITE_NULL)) {
			    rc = SQLITE_NOMEM;
			    break;	/* from for */
			}
		    }	/* end for */

		    /* if data and types extraction failed... */
		    if (rc != SQLITE_ROW)
			break;

		    /* call the supplied callback with the result row data */
		    if (xCallback (sql, nCol, azVals, azCols, aiTypes)) {
			rc = SQLITE_ABORT;
			break;
		    }
		    rc = sqlite3_step(pStmt);
		} while (rc == SQLITE_ROW);
		azCols = _free(azCols);
		sql->S = NULL;
	    } else {
		do {
		    rc = sqlite3_step(pStmt);
		} while (rc == SQLITE_ROW);
	    }
	}

	/* Finalize the statement just executed. If this fails, save a 
	 ** copy of the error message. Otherwise, set zSql to point to the
	 ** next statement to execute. */
	rc = rpmsqlCmd(sql, "finalize", db,
		sqlite3_finalize(pStmt));

bottom:
	/* On error, retrieve message and exit. */
	if (rc) {
	    if (pzErrMsg)
		*pzErrMsg = save_err_msg(db);
	    break;
	}

	/* Move to next sql statement */
	zSql = zLeftover;
	while (isspace(zSql[0]))
	    zSql++;
    }				/* end while */

    return rc;
}
#endif	/* defined(WITH_SQLITE) */

/*==============================================================*/

#if defined(WITH_SQLITE)

/*
** This is a different callback routine used for dumping the database.
** Each row received by this callback consists of a table name,
** the table type ("index" or "table") and SQL to create the table.
** This routine should print text sufficient to recreate the table.
*/
static int dump_callback(void *pArg, int nArg, char **azArg, char **azCol)
{
    rpmsql sql = (rpmsql) pArg;
    sqlite3 * db = (sqlite3 *) sql->I;
    int rc;
    const char *zTable;
    const char *zType;
    const char *zSql;
    const char *zPrepStmt = 0;

SQLDBG((stderr, "--> %s(%p,%d,%p,%p)\n", __FUNCTION__, pArg, nArg, azArg, azCol));
    azCol = azCol;
    if (nArg != 3)
	return 1;
    zTable = azArg[0];
    zType = azArg[1];
    zSql = azArg[2];

    if (strcmp(zTable, "sqlite_sequence") == 0) {
	zPrepStmt = "DELETE FROM sqlite_sequence;\n";
    } else if (strcmp(zTable, "sqlite_stat1") == 0) {
	rpmsqlFprintf(sql, "ANALYZE sqlite_master;\n");
    } else if (strncmp(zTable, "sqlite_", 7) == 0) {
	return 0;
    } else if (strncmp(zSql, "CREATE VIRTUAL TABLE", 20) == 0) {
	char *zIns;
	if (!F_ISSET(sql, WRITABLE)) {
	    rpmsqlFprintf(sql, "PRAGMA writable_schema=ON;\n");
	    sql->flags |= RPMSQL_FLAGS_WRITABLE;
	}
	zIns =
	    sqlite3_mprintf
	    ("INSERT INTO sqlite_master(type,name,tbl_name,rootpage,sql)"
	     "VALUES('table','%q','%q',0,'%q');", zTable, zTable, zSql);
	rpmsqlFprintf(sql, "%s\n", zIns);
	sqlite3_free(zIns);
	return 0;
    } else {
	rpmsqlFprintf(sql, "%s;\n", zSql);
    }

    if (strcmp(zType, "table") == 0) {
	sqlite3_stmt * pTableInfo = NULL;
	char *zSelect = 0;
	char *zTableInfo = 0;
	char *zTmp = 0;
	int nRow = 0;

	zTableInfo = appendText(zTableInfo, "PRAGMA table_info(", 0);
	zTableInfo = appendText(zTableInfo, zTable, '"');
	zTableInfo = appendText(zTableInfo, ");", 0);

	rc = rpmsqlCmd(sql, "prepare", db,
		sqlite3_prepare(db, zTableInfo, -1, &pTableInfo, 0));
	zTableInfo = _free(zTableInfo);
	if (rc != SQLITE_OK || !pTableInfo) {
	    return 1;
	}

	zSelect = appendText(zSelect, "SELECT 'INSERT INTO ' || ", 0);
	zTmp = appendText(zTmp, zTable, '"');
	if (zTmp) {
	    zSelect = appendText(zSelect, zTmp, '\'');
	}
	zSelect = appendText(zSelect, " || ' VALUES(' || ", 0);
	rc = sqlite3_step(pTableInfo);
	while (rc == SQLITE_ROW) {
	    const char *zText =
		(const char *) sqlite3_column_text(pTableInfo, 1);
	    zSelect = appendText(zSelect, "quote(", 0);
	    zSelect = appendText(zSelect, zText, '"');
	    rc = sqlite3_step(pTableInfo);
	    if (rc == SQLITE_ROW) {
		zSelect = appendText(zSelect, ") || ',' || ", 0);
	    } else {
		zSelect = appendText(zSelect, ") ", 0);
	    }
	    nRow++;
	}
	rc = rpmsqlCmd(sql, "finalize", db,
		sqlite3_finalize(pTableInfo));
	if (rc != SQLITE_OK || nRow == 0) {
	    zSelect = _free(zSelect);
	    return 1;
	}

	zSelect = appendText(zSelect, "|| ')' FROM  ", 0);
	zSelect = appendText(zSelect, zTable, '"');

	rc = run_table_dump_query(sql, db, zSelect, zPrepStmt);
	if (rc == SQLITE_CORRUPT) {
	    zSelect = appendText(zSelect, " ORDER BY rowid DESC", 0);
	    rc = run_table_dump_query(sql, db, zSelect, NULL);
	}
	if (zSelect)
	    free(zSelect);
    }
    return 0;
}

/*
** Run zQuery.  Use dump_callback() as the callback routine so that
** the contents of the query are output as SQL statements.
**
** If we get a SQLITE_CORRUPT error, rerun the query after appending
** "ORDER BY rowid DESC" to the end.
*/
static int run_schema_dump_query(rpmsql sql,
				 const char *zQuery, char **pzErrMsg)
{
    sqlite3 * db = (sqlite3 *) sql->I;
    int rc;

SQLDBG((stderr, "--> %s(%p,%s,%p)\n", __FUNCTION__, sql, zQuery, pzErrMsg));
    rc = sqlite3_exec(db, zQuery, dump_callback, sql, pzErrMsg);
    if (rc == SQLITE_CORRUPT) {
	char *zQ2;
	int len = strlen30(zQuery);
	if (pzErrMsg)
	    sqlite3_free(*pzErrMsg);
	zQ2 = xmalloc(len + 100);
	sqlite3_snprintf(sizeof(zQ2), zQ2, "%s ORDER BY rowid DESC",
			 zQuery);
	rc = sqlite3_exec(db, zQ2, dump_callback, sql, pzErrMsg);
	zQ2 = _free(zQ2);
    }
    return rc;
}

/*
** Text of a help message
*/
static char zHelp[] =
    ".backup ?DB? FILE      Backup DB (default \"main\") to FILE\n"
    ".bail ON|OFF           Stop after hitting an error.  Default OFF\n"
    ".databases             List names and files of attached databases\n"
    ".dump ?TABLE? ...      Dump the database in an SQL text format\n"
    "                         If TABLE specified, only dump tables matching\n"
    "                         LIKE pattern TABLE.\n"
    ".echo ON|OFF           Turn command echo on or off\n"
    ".exit                  Exit this program\n"
    ".explain ?ON|OFF?      Turn output mode suitable for EXPLAIN on or off.\n"
    "                         With no args, it turns EXPLAIN on.\n"
    ".header(s) ON|OFF      Turn display of headers on or off\n"
    ".help                  Show this message\n"
    ".import FILE TABLE     Import data from FILE into TABLE\n"
    ".indices ?TABLE?       Show names of all indices\n"
    "                         If TABLE specified, only show indices for tables\n"
    "                         matching LIKE pattern TABLE.\n"
#ifdef SQLITE_ENABLE_IOTRACE
    ".iotrace FILE          Enable I/O diagnostic logging to FILE\n"
#endif
    ".load FILE ?ENTRY?     Load an extension library\n"
    ".log FILE|off          Turn logging on or off.  FILE can be stderr/stdout\n"
    ".mode MODE ?TABLE?     Set output mode where MODE is one of:\n"
    "                         csv      Comma-separated values\n"
    "                         column   Left-aligned columns.  (See .width)\n"
    "                         html     HTML <table> code\n"
    "                         insert   SQL insert statements for TABLE\n"
    "                         line     One value per line\n"
    "                         list     Values delimited by .separator string\n"
    "                         tabs     Tab-separated values\n"
    "                         tcl      TCL list elements\n"
    ".nullvalue STRING      Print STRING in place of NULL values\n"
    ".output FILENAME       Send output to FILENAME\n"
    ".output stdout         Send output to the screen\n"
    ".prompt MAIN CONTINUE  Replace the standard prompts\n"
    ".quit                  Exit this program\n"
    ".read FILENAME         Execute SQL in FILENAME\n"
    ".restore ?DB? FILE     Restore content of DB (default \"main\") from FILE\n"
    ".schema ?TABLE?        Show the CREATE statements\n"
    "                         If TABLE specified, only show tables matching\n"
    "                         LIKE pattern TABLE.\n"
    ".separator STRING      Change separator used by output mode and .import\n"
    ".show                  Show the current values for various settings\n"
    ".tables ?TABLE?        List names of tables\n"
    "                         If TABLE specified, only list tables matching\n"
    "                         LIKE pattern TABLE.\n"
    ".timeout MS            Try opening locked tables for MS milliseconds\n"
    ".width NUM1 NUM2 ...   Set column widths for \"column\" mode\n";

static char zTimerHelp[] =
    ".timer ON|OFF          Turn the CPU timer measurement on or off\n";

/*
** Do C-language style dequoting.
**
**    \t    -> tab
**    \n    -> newline
**    \r    -> carriage return
**    \NNN  -> ascii character NNN in octal
**    \\    -> backslash
*/
static void resolve_backslashes(char *z)
{
    int i, j;
    char c;
    for (i = j = 0; (c = z[i]) != 0; i++, j++) {
	if (c == '\\') {
	    c = z[++i];
	    if (c == 'n') {
		c = '\n';
	    } else if (c == 't') {
		c = '\t';
	    } else if (c == 'r') {
		c = '\r';
	    } else if (c >= '0' && c <= '7') {
		c -= '0';
		if (z[i + 1] >= '0' && z[i + 1] <= '7') {
		    i++;
		    c = (c << 3) + z[i] - '0';
		    if (z[i + 1] >= '0' && z[i + 1] <= '7') {
			i++;
			c = (c << 3) + z[i] - '0';
		    }
		}
	    }
	}
	z[j] = c;
    }
    z[j] = 0;
}

/*
** Interpret zArg as a boolean value.  Return either 0 or 1.
*/
static int booleanValue(const char * zArg)
{
    int val = atoi(zArg);
    if (!strcasecmp(zArg, "on") || !strcasecmp(zArg, "yes"))
	val = 1;
SQLDBG((stderr, "<-- %s(%s) val %d\n", __FUNCTION__, zArg, val));
    return val;
}

static const char *modeDescr[] = {
    "line",
    "column",
    "list",
    "semi",
    "html",
    "insert",
    "tcl",
    "csv",
    "explain",
};


/* forward ref @*/
static int rpmsqlInput(rpmsql sql, void * _in);

/**
 * Process .foo SQLITE3 meta command.
 *
 * @return		0 on success, 1 on error, 2 to exit
 */
static int rpmsqlMetaCommand(rpmsql sql, char *zLine)
{
    sqlite3 * db = (sqlite3 *)sql->I;
    int i = 1;
    int nArg = 0;
    int n, c;
    int rc = 0;
    char *azArg[50];

SQLDBG((stderr, "--> %s(%p,%s)\n", __FUNCTION__, sql, zLine));
    /* Parse the input line into tokens.  */
    while (zLine[i] && nArg < ArraySize(azArg)) {
	while (isspace((unsigned char) zLine[i])) {
	    i++;
	}
	if (zLine[i] == 0)
	    break;
	if (zLine[i] == '\'' || zLine[i] == '"') {
	    int delim = zLine[i++];
	    azArg[nArg++] = &zLine[i];
	    while (zLine[i] && zLine[i] != delim) {
		i++;
	    }
	    if (zLine[i] == delim) {
		zLine[i++] = 0;
	    }
	    if (delim == '"')
		resolve_backslashes(azArg[nArg - 1]);
	} else {
	    azArg[nArg++] = &zLine[i];
	    while (zLine[i] && !isspace((unsigned char) zLine[i])) {
		i++;
	    }
	    if (zLine[i])
		zLine[i++] = 0;
	    resolve_backslashes(azArg[nArg - 1]);
	}
    }

    /* Process the input line. */
    if (nArg == 0)
	return 0;		/* no tokens, no error */
    n = strlen30(azArg[0]);
    c = azArg[0][0];
    if (c == 'b' && n >= 3 && strncmp(azArg[0], "backup", n) == 0
	&& nArg > 1 && nArg < 4) {
	const char *zDestFile;
	const char *zDb;
	sqlite3 * pDest;
	sqlite3_backup *pBackup;
	if (nArg == 2) {
	    zDestFile = azArg[1];
	    zDb = "main";
	} else {
	    zDestFile = azArg[2];
	    zDb = azArg[1];
	}
	rc = rpmsqlCmd(sql, "open", pDest,
		sqlite3_open(zDestFile, &pDest));
	if (rc) {
#ifdef	DYING
	    fprintf(stderr, "Error: cannot open \"%s\"\n", zDestFile);
#endif
	    (void) rpmsqlCmd(sql, "close", pDest,
		sqlite3_close(pDest));
	    return 1;
	}
	_rpmsqlOpenDB(sql);
	pBackup = sqlite3_backup_init(pDest, "main", db, zDb);
	if (pBackup == NULL) {
	    fprintf(stderr, "Error: %s\n", sqlite3_errmsg(pDest));
	    (void) rpmsqlCmd(sql, "close", pDest,
			sqlite3_close(pDest));
	    return 1;
	}
	while ((rc = sqlite3_backup_step(pBackup, 100)) == SQLITE_OK) {
	}
	(void) rpmsqlCmd(sql, "backup_finish", pBackup,
			sqlite3_backup_finish(pBackup));
	if (rc == SQLITE_DONE) {
	    rc = 0;
	} else {
	    fprintf(stderr, "Error: %s\n", sqlite3_errmsg(pDest));
	    rc = 1;
	}
	(void) rpmsqlCmd(sql, "close", pDest,
			sqlite3_close(pDest));
    } else
     if (c == 'b' && n >= 3 && strncmp(azArg[0], "bail", n) == 0
	     && nArg > 1 && nArg < 3) {
	if (booleanValue(azArg[1]))
	    sql->flags |= RPMSQL_FLAGS_BAIL;
	else
	    sql->flags &= ~RPMSQL_FLAGS_BAIL;
    } else
     if (c == 'd' && n > 1 && strncmp(azArg[0], "databases", n) == 0
	     && nArg == 1) {
	/* XXX recursion b0rkage lies here. */
	uint32_t _flags = sql->flags;
	uint32_t _mode = sql->mode;
	char *zErrMsg = 0;
	_rpmsqlOpenDB(sql);
	sql->flags |= RPMSQL_FLAGS_SHOWHDR;
	sql->mode = RPMSQL_MODE_COLUMN;
	sql->colWidth[0] = 3;
	sql->colWidth[1] = 15;
	sql->colWidth[2] = 58;
	sql->cnt = 0;
	sqlite3_exec(db, "PRAGMA database_list; ", callback, sql, &zErrMsg);
	if (zErrMsg) {
	    fprintf(stderr, "Error: %s\n", zErrMsg);
	    sqlite3_free(zErrMsg);
	    rc = 1;
	}
	sql->mode = _mode;
	sql->flags = _flags;
    } else
     if (c == 'd' && strncmp(azArg[0], "dump", n) == 0 && nArg < 3) {
	char * t;
	_rpmsqlOpenDB(sql);
	/* When playing back a "dump", the content might appear in an order
	 ** which causes immediate foreign key constraints to be violated.
	 ** So disable foreign-key constraint enforcement to prevent problems. */
	rpmsqlFprintf(sql, "PRAGMA foreign_keys=OFF;\n");
	rpmsqlFprintf(sql, "BEGIN TRANSACTION;\n");
	sql->flags &= ~RPMSQL_FLAGS_WRITABLE;
	sqlite3_exec(db, "PRAGMA writable_schema=ON", 0, 0, 0);
	if (nArg == 1) {
	    t = rpmExpand("SELECT name, type, sql FROM sqlite_master"
			  " WHERE sql NOT NULL AND type=='table'"
			  " AND name!='sqlite_sequence'", NULL);
	    run_schema_dump_query(sql, t, NULL);
	    t = _free(t);
	    t = rpmExpand("SELECT name, type, sql FROM sqlite_master"
			  " WHERE name=='sqlite_sequence'", NULL);
	    run_schema_dump_query(sql, t, NULL);
	    t = _free(t);
	    t = rpmExpand("SELECT sql FROM sqlite_master"
			  " WHERE sql NOT NULL AND type IN ('index','trigger','view')", NULL);
	    run_table_dump_query(sql, db, t, NULL);
	    t = _free(t);
	} else {
	    int i;
	    for (i = 1; i < nArg; i++) {
		t = rpmExpand(  "SELECT name, type, sql FROM sqlite_master"
				" WHERE tbl_name LIKE '", azArg[i], "'"
				" AND type=='table' AND sql NOT NULL", NULL);
		run_schema_dump_query(sql, t, NULL);
		t = _free(t);
		t = rpmExpand(  "SELECT sql FROM sqlite_master"
				" WHERE sql NOT NULL"
				" AND type IN ('index','trigger','view')"
				" AND tbl_name LIKE '", azArg[i], "'", NULL);
		run_table_dump_query(sql, db, t, NULL);
		t = _free(t);
	    }
	}
	if (F_ISSET(sql, WRITABLE)) {
	    rpmsqlFprintf(sql, "PRAGMA writable_schema=OFF;\n");
	    sql->flags &= ~RPMSQL_FLAGS_WRITABLE;
	}
	(void) rpmsqlCmd(sql, "exec", db,
		sqlite3_exec(db, "PRAGMA writable_schema=OFF", 0, 0, 0));
	rpmsqlFprintf(sql, "COMMIT;\n");
    } else
     if (c == 'e' && strncmp(azArg[0], "echo", n) == 0 && nArg > 1 && nArg < 3) {
	if (booleanValue(azArg[1]))
	    sql->flags |= RPMSQL_FLAGS_ECHO;
	else
	    sql->flags &= ~RPMSQL_FLAGS_ECHO;
    } else
     if (c == 'e' && strncmp(azArg[0], "exit", n) == 0 && nArg == 1) {
	rc = 2;
    } else
     if (c == 'e' && strncmp(azArg[0], "explain", n) == 0 && nArg < 3) {
	int val = nArg >= 2 ? booleanValue(azArg[1]) : 1;
	if (val == 1) {
	    if (!sql->explainPrev.valid) {
		sql->explainPrev.valid = 1;
		sql->explainPrev.mode = sql->mode;
		sql->explainPrev.flags = sql->flags;
		memcpy(sql->explainPrev.colWidth, sql->colWidth,
		       sizeof(sql->colWidth));
	    }
	    /* We could put this code under the !p->explainValid
	     ** condition so that it does not execute if we are already in
	     ** explain mode. However, always executing it allows us an easy
	     ** way to reset to explain mode in case the user previously
	     ** did an .explain followed by a .width, .mode or .header
	     ** command.
	     */
	    sql->mode = RPMSQL_MODE_EXPLAIN;
	    sql->flags |= RPMSQL_FLAGS_SHOWHDR;
	    memset(sql->colWidth, 0, ArraySize(sql->colWidth));
	    sql->colWidth[0] = 4;	/* addr */
	    sql->colWidth[1] = 13;	/* opcode */
	    sql->colWidth[2] = 4;	/* P1 */
	    sql->colWidth[3] = 4;	/* P2 */
	    sql->colWidth[4] = 4;	/* P3 */
	    sql->colWidth[5] = 13;	/* P4 */
	    sql->colWidth[6] = 2;	/* P5 */
	    sql->colWidth[7] = 13;	/* Comment */
	} else if (sql->explainPrev.valid) {
	    sql->explainPrev.valid = 0;
	    sql->mode = sql->explainPrev.mode;
	    sql->flags = sql->explainPrev.flags;
	    memcpy(sql->colWidth, sql->explainPrev.colWidth,
		   sizeof(sql->colWidth));
	}
    } else
     if (c == 'h' && (strncmp(azArg[0], "header", n) == 0 ||
			  strncmp(azArg[0], "headers", n) == 0)
	     && nArg > 1 && nArg < 3) {
	if (booleanValue(azArg[1]))
	    sql->flags |= RPMSQL_FLAGS_SHOWHDR;
	else
	    sql->flags &= ~RPMSQL_FLAGS_SHOWHDR;
    } else
     if (c == 'h' && strncmp(azArg[0], "help", n) == 0) {
	fprintf(stderr, "%s", zHelp);
	if (HAS_TIMER) {
	    fprintf(stderr, "%s", zTimerHelp);
	}
    } else
     if (c == 'i' && strncmp(azArg[0], "import", n) == 0 && nArg == 3) {
	char *zTable = azArg[2];	/* Insert data into this table */
	char *zFile = azArg[1];	/* The file from which to extract data */
	sqlite3_stmt * pStmt = NULL;/* A statement */
	int nCol;		/* Number of columns in the table */
	int nByte;		/* Number of bytes in an SQL string */
	int i, j;		/* Loop counters */
	int nSep;		/* Number of bytes in sql->separator[] */
	char *zSql;		/* An SQL statement */
	char *zLine;		/* A single line of input from the file */
	char **azCol;		/* zLine[] broken up into columns */
	char *zCommit;		/* How to commit changes */
	FILE *in;		/* The input file */
	int lineno = 0;		/* Line number of input file */

	_rpmsqlOpenDB(sql);
	nSep = strlen30(sql->separator);
	if (nSep == 0) {
	    fprintf(stderr,
		    "Error: non-null separator required for import\n");
	    return 1;
	}
	zSql = sqlite3_mprintf("SELECT * FROM '%q'", zTable);
assert(zSql != NULL);
	nByte = strlen30(zSql);
	rc = rpmsqlCmd(sql, "prepare", db,
		sqlite3_prepare(db, zSql, -1, &pStmt, 0));
	sqlite3_free(zSql);
	if (rc) {
#ifdef	DYING
	    sqlite3 * db = (sqlite3 *)sql->I;
	    fprintf(stderr, "Error: %s\n", sqlite3_errmsg(db));
#endif
	    if (pStmt) {
		(void) rpmsqlCmd(sql, "finalize", db,
			sqlite3_finalize(pStmt));
	    }
	    return 1;
	}
	nCol = sqlite3_column_count(pStmt);
	(void) rpmsqlCmd(sql, "finalize", db,
			sqlite3_finalize(pStmt));
	pStmt = 0;
	if (nCol == 0)
	    return 0;		/* no columns, no error */
	zSql = xmalloc(nByte + 20 + nCol * 2);
	sqlite3_snprintf(nByte + 20, zSql, "INSERT INTO '%q' VALUES(?",
			 zTable);
	j = strlen30(zSql);
	for (i = 1; i < nCol; i++) {
	    zSql[j++] = ',';
	    zSql[j++] = '?';
	}
	zSql[j++] = ')';
	zSql[j] = 0;
	rc = rpmsqlCmd(sql, "prepare", db,
		sqlite3_prepare(db, zSql, -1, &pStmt, 0));
	zSql = _free(zSql);
	if (rc) {
#ifdef	DYING
	    sqlite3 * db = (sqlite3 *)sql->I;
	    fprintf(stderr, "Error: %s\n", sqlite3_errmsg(db));
#endif
	    if (pStmt) {
		(void) rpmsqlCmd(sql, "finalize", db,
			sqlite3_finalize(pStmt));
	    }
	    return 1;
	}
	in = fopen(zFile, "rb");
	if (in == NULL) {
	    fprintf(stderr, "Error: cannot open \"%s\"\n", zFile);
	    (void) rpmsqlCmd(sql, "finalize", db,
			sqlite3_finalize(pStmt));
	    return 1;
	}
	azCol = malloc(sizeof(azCol[0]) * (nCol + 1));
	if (azCol == NULL) {
	    fclose(in);
	    (void) rpmsqlCmd(sql, "finalize", db,
			sqlite3_finalize(pStmt));
assert(azCol);
	}
	sqlite3_exec(db, "BEGIN", 0, 0, 0);
	zCommit = "COMMIT";
	while ((zLine = local_getline(0, in)) != 0) {
	    char *z;
	    i = 0;
	    lineno++;
	    azCol[0] = zLine;
	    for (i = 0, z = zLine; *z && *z != '\n' && *z != '\r'; z++) {
		if (*z == sql->separator[0]
		    && strncmp(z, sql->separator, nSep) == 0) {
		    *z = 0;
		    i++;
		    if (i < nCol) {
			azCol[i] = &z[nSep];
			z += nSep - 1;
		    }
		}
	    }			/* end for */
	    *z = 0;
	    if (i + 1 != nCol) {
		fprintf(stderr,
			"Error: %s line %d: expected %d columns of data but found %d\n",
			zFile, lineno, nCol, i + 1);
		zCommit = "ROLLBACK";
		free(zLine);
		rc = 1;
		break;		/* from while */
	    }
	    for (i = 0; i < nCol; i++) {
		sqlite3_bind_text(pStmt, i + 1, azCol[i], -1,
				  SQLITE_STATIC);
	    }
	    sqlite3_step(pStmt);
	    rc = rpmsqlCmd(sql, "reset", db,
		sqlite3_reset(pStmt));
	    zLine = _free(zLine);
	    if (rc) {
#ifdef	DYING
		sqlite3 * db = (sqlite3 *)sql->I;
		fprintf(stderr, "Error: %s\n", sqlite3_errmsg(db));
#endif
		zCommit = "ROLLBACK";
		rc = 1;
		break;		/* from while */
	    }
	}			/* end while */
	free(azCol);
	fclose(in);
	(void) rpmsqlCmd(sql, "finalize", db,
		sqlite3_finalize(pStmt));
	(void) rpmsqlCmd(sql, "exec", db,
		sqlite3_exec(db, zCommit, 0, 0, 0));
    } else
     if (c == 'i' && strncmp(azArg[0], "indices", n) == 0 && nArg < 3) {
	/* XXX recursion b0rkage lies here. */
	uint32_t _flags = sql->flags;
	uint32_t _mode = sql->mode;
	char * t;
	char *zErrMsg = NULL;
	_rpmsqlOpenDB(sql);
	sql->flags &= ~RPMSQL_FLAGS_SHOWHDR;
	sql->mode = RPMSQL_MODE_LIST;
	if (nArg == 1) {
	    t = rpmExpand("SELECT name FROM sqlite_master"
			  " WHERE type='index' AND name NOT LIKE 'sqlite_%'"
			  " UNION ALL "
			  "SELECT name FROM sqlite_temp_master"
			  " WHERE type='index'"
			  " ORDER BY 1", NULL);
	    rc = rpmsqlCmd(sql, "exec", db,
			sqlite3_exec(db, t, callback, sql, &zErrMsg));
	    t = _free(t);
	} else {
	    t = rpmExpand("SELECT name FROM sqlite_master"
			  " WHERE type='index' AND tbl_name LIKE '", azArg[1], "'",
			  " UNION ALL "
			  "SELECT name FROM sqlite_temp_master"
			  " WHERE type='index' AND tbl_name LIKE '", azArg[1], "'",
			  " ORDER BY 1", NULL);
	    rc = rpmsqlCmd(sql, "exec", db,
			sqlite3_exec(db, t, callback, sql, &zErrMsg));
	    t = _free(t);
	}
	if (zErrMsg) {
	    fprintf(stderr, "Error: %s\n", zErrMsg);
	    sqlite3_free(zErrMsg);
	    rc = 1;
	} else if (rc) {
#ifdef	DYING
	    fprintf(stderr,
		    "Error: querying sqlite_master and sqlite_temp_master\n");
#endif
	    rc = 1;
	}
	sql->mode = _mode;
	sql->flags = _flags;
    } else
#ifdef SQLITE_ENABLE_IOTRACE
    if (c == 'i' && strncmp(azArg[0], "iotrace", n) == 0) {
	extern void (*sqlite3IoTrace) (const char *, ...);
	if (sql->iotrace && fileno(sql->iotrace) > STDERR_FILENO)
	    fclose(sql->iotrace);
	sql->iotrace = NULL;
	if (nArg < 2) {
	    sqlite3IoTrace = NULL;
	} else if (strcmp(azArg[1], "-") == 0) {
	    sqlite3IoTrace = iotracePrintf;
	    sql->iotrace = stdout;
	} else {
	    sql->iotrace = fopen(azArg[1], "w");
	    if (sql->iotrace == NULL) {
		fprintf(stderr, "Error: cannot open \"%s\"\n", azArg[1]);
		sqlite3IoTrace = NULL;
		rc = 1;
	    } else {
		sqlite3IoTrace = iotracePrintf;
	    }
	}
    } else
#endif

    if (c == 'l' && strncmp(azArg[0], "load", n) == 0 && nArg >= 2) {
	const char *zFile, *zProc;
	char *zErrMsg = 0;
	zFile = azArg[1];
	zProc = nArg >= 3 ? azArg[2] : 0;
	if (!F_ISSET(sql, NOLOAD)) {
	    _rpmsqlOpenDB(sql);
	    rc = rpmsqlCmd(sql, "load_extension", db,
			sqlite3_load_extension(db, zFile, zProc, &zErrMsg));
	    if (rc) {
		fprintf(stderr, "Error: %s\n", zErrMsg);
		sqlite3_free(zErrMsg);
		rc = 1;
	    }
	}
    } else

    if (c == 'l' && strncmp(azArg[0], "log", n) == 0 && nArg >= 1) {
	const char *zFile = azArg[1];
	if (sql->pLog && fileno(sql->pLog) > STDERR_FILENO)
	    fclose(sql->pLog);
	sql->pLog = NULL;
	if (strcmp(zFile, "stdout") == 0) {
	    sql->pLog = stdout;
	} else if (strcmp(zFile, "stderr") == 0) {
	    sql->pLog = stderr;
	} else if (strcmp(zFile, "off") == 0) {
	    sql->pLog = NULL;
	} else {
	    sql->pLog = fopen(zFile, "w");
	    if (sql->pLog == NULL) {
		fprintf(stderr, "Error: cannot open \"%s\"\n", zFile);
	    }
	}

    } else
     if (c == 'm' && strncmp(azArg[0], "mode", n) == 0 && nArg == 2) {
	int n2 = strlen30(azArg[1]);
	if ((n2 == 4 && strncmp(azArg[1], "line", n2) == 0)
	    || (n2 == 5 && strncmp(azArg[1], "lines", n2) == 0)) {
	    sql->mode = RPMSQL_MODE_LINE;
	} else if ((n2 == 6 && strncmp(azArg[1], "column", n2) == 0)
		   || (n2 == 7 && strncmp(azArg[1], "columns", n2) == 0)) {
	    sql->mode = RPMSQL_MODE_COLUMN;
	} else if (n2 == 4 && strncmp(azArg[1], "list", n2) == 0) {
	    sql->mode = RPMSQL_MODE_LIST;
	} else if (n2 == 4 && strncmp(azArg[1], "html", n2) == 0) {
	    sql->mode = RPMSQL_MODE_HTML;
	} else if (n2 == 3 && strncmp(azArg[1], "tcl", n2) == 0) {
	    sql->mode = RPMSQL_MODE_TCL;
	} else if (n2 == 3 && strncmp(azArg[1], "csv", n2) == 0) {
	    sql->mode = RPMSQL_MODE_CSV;
	    sqlite3_snprintf(sizeof(sql->separator), sql->separator, ",");
	} else if (n2 == 4 && strncmp(azArg[1], "tabs", n2) == 0) {
	    sql->mode = RPMSQL_MODE_LIST;
	    sqlite3_snprintf(sizeof(sql->separator), sql->separator, "\t");
	} else if (n2 == 6 && strncmp(azArg[1], "insert", n2) == 0) {
	    sql->mode = RPMSQL_MODE_INSERT;
	    set_table_name(sql, "table");
	} else {
	    fprintf(stderr, "Error: mode should be one of: "
		    "column csv html insert line list tabs tcl\n");
	    rc = 1;
	}
    } else
     if (c == 'm' && strncmp(azArg[0], "mode", n) == 0 && nArg == 3) {
	int n2 = strlen30(azArg[1]);
	if (n2 == 6 && strncmp(azArg[1], "insert", n2) == 0) {
	    sql->mode = RPMSQL_MODE_INSERT;
	    set_table_name(sql, azArg[2]);
	} else {
	    fprintf(stderr, "Error: invalid arguments: "
		    " \"%s\". Enter \".help\" for help\n", azArg[2]);
	    rc = 1;
	}
    } else
     if (c == 'n' && strncmp(azArg[0], "nullvalue", n) == 0 && nArg == 2) {
	sqlite3_snprintf(sizeof(sql->nullvalue), sql->nullvalue,
			 "%.*s", (int) ArraySize(sql->nullvalue) - 1,
			 azArg[1]);
    } else
     if (c == 'o' && strncmp(azArg[0], "output", n) == 0 && nArg == 2) {
	if (fileno(sql->out) > STDERR_FILENO)
	    fclose(sql->out);
	sql->out = NULL;
	if (strcmp(azArg[1], "stdout") == 0) {
	    sql->out = stdout;
	    sqlite3_snprintf(sizeof(sql->outfile), sql->outfile, "stdout");
	} else {
	    sql->out = fopen(azArg[1], "wb");
	    if (sql->out == NULL) {
		fprintf(stderr, "Error: cannot write to \"%s\"\n",
			azArg[1]);
		sql->out = stdout;
		rc = 1;
	    } else {
		sqlite3_snprintf(sizeof(sql->outfile), sql->outfile, "%s",
				 azArg[1]);
	    }
	}
    } else
     if (c == 'p' && strncmp(azArg[0], "prompt", n) == 0
	     && (nArg == 2 || nArg == 3)) {
	if (nArg >= 2) {
	    sql->zPrompt = _free(sql->zPrompt);
	    sql->zPrompt = xstrdup(azArg[1]);
	}
	if (nArg >= 3) {
	    sql->zContinue = _free(sql->zContinue);
	    sql->zContinue = xstrdup(azArg[2]);
	}
    } else
     if (c == 'q' && strncmp(azArg[0], "quit", n) == 0 && nArg == 1) {
	rc = 2;
    } else
     if (c == 'r' && n >= 3 && strncmp(azArg[0], "read", n) == 0
	     && nArg == 2) {
	FILE *alt = fopen(azArg[1], "rb");
	if (alt == 0) {
	    fprintf(stderr, "Error: cannot open \"%s\"\n", azArg[1]);
	    rc = 1;
	} else {
	    rc = rpmsqlInput(sql, alt);
	    fclose(alt);
	}
    } else
     if (c == 'r' && n >= 3 && strncmp(azArg[0], "restore", n) == 0
	     && nArg > 1 && nArg < 4) {
	const char *zSrcFile;
	const char *zDb;
	sqlite3 * pSrc;
	sqlite3_backup *pBackup;
	int nTimeout = 0;

	if (nArg == 2) {
	    zSrcFile = azArg[1];
	    zDb = "main";
	} else {
	    zSrcFile = azArg[2];
	    zDb = azArg[1];
	}
	rc = rpmsqlCmd(sql, "open", pSrc,	/* XXX watchout: arg order */
		sqlite3_open(zSrcFile, &pSrc));
	if (rc) {
#ifdef	DYING
	    fprintf(stderr, "Error: cannot open \"%s\"\n", zSrcFile);
#endif
	    (void) rpmsqlCmd(sql, "close", pSrc,
		sqlite3_close(pSrc));
	    return 1;
	}
	_rpmsqlOpenDB(sql);
	pBackup = sqlite3_backup_init(db, zDb, pSrc, "main");
	if (pBackup == 0) {
	    fprintf(stderr, "Error: %s\n", sqlite3_errmsg(db));
	    (void) rpmsqlCmd(sql, "close", db,
			sqlite3_close(pSrc));
	    return 1;
	}
	while ((rc = sqlite3_backup_step(pBackup, 100)) == SQLITE_OK
	       || rc == SQLITE_BUSY)
	{
	    if (rc == SQLITE_BUSY) {
		if (nTimeout++ >= 3)
		    break;
		sqlite3_sleep(100);
	    }
	}
	sqlite3_backup_finish(pBackup);
	switch (rc) {
	case SQLITE_DONE:
	    rc = 0;
	    break;
	case SQLITE_BUSY:
	case SQLITE_LOCKED:
	    fprintf(stderr, "Error: source database is busy\n");
	    rc = 1;
	    break;
	default:
	    fprintf(stderr, "Error: %s\n", sqlite3_errmsg(db));
	    rc = 1;
	    break;
	}
	(void) rpmsqlCmd(sql, "close", pSrc,
		sqlite3_close(pSrc));
    } else
     if (c == 's' && strncmp(azArg[0], "schema", n) == 0 && nArg < 3) {
	/* XXX recursion b0rkage lies here. */
	uint32_t _flags = sql->flags;
	uint32_t _mode = sql->mode;
	char *zErrMsg = 0;
	_rpmsqlOpenDB(sql);
	sql->flags &= ~RPMSQL_FLAGS_SHOWHDR;
	sql->mode = RPMSQL_MODE_SEMI;
	if (nArg > 1) {
	    int i;
	    for (i = 0; azArg[1][i]; i++)
		azArg[1][i] = (char) tolower(azArg[1][i]);
	    if (strcmp(azArg[1], "sqlite_master") == 0) {
		char *new_argv[2], *new_colv[2];
		new_argv[0] = "CREATE TABLE sqlite_master (\n"
		    "  type text,\n"
		    "  name text,\n"
		    "  tbl_name text,\n"
		    "  rootpage integer,\n" "  sql text\n" ")";
		new_argv[1] = 0;
		new_colv[0] = "sql";
		new_colv[1] = 0;
		callback(sql, 1, new_argv, new_colv);
		rc = SQLITE_OK;
	    } else if (strcmp(azArg[1], "sqlite_temp_master") == 0) {
		char *new_argv[2], *new_colv[2];
		new_argv[0] = "CREATE TEMP TABLE sqlite_temp_master (\n"
		    "  type text,\n"
		    "  name text,\n"
		    "  tbl_name text,\n"
		    "  rootpage integer,\n" "  sql text\n" ")";
		new_argv[1] = 0;
		new_colv[0] = "sql";
		new_colv[1] = 0;
		callback(sql, 1, new_argv, new_colv);
		rc = SQLITE_OK;
	    } else {
		char * t;
		t = rpmExpand(	"SELECT sql FROM "
				"  (SELECT sql sql, type type, tbl_name tbl_name, name name"
				"     FROM sqlite_master UNION ALL"
				"   SELECT sql, type, tbl_name, name FROM sqlite_temp_master)"
				" WHERE tbl_name LIKE '", azArg[1], "'"
				" AND type!='meta' AND sql NOTNULL "
				"ORDER BY substr(type,2,1), name", NULL);
		rc = rpmsqlCmd(sql, "exec", db,
			sqlite3_exec(db, t, callback, sql, &zErrMsg));
		t = _free(t);
	    }
	    sql->mode = _mode;
	    sql->flags = _flags;
	} else {
	    rc = sqlite3_exec(db,
			      "SELECT sql FROM "
			      "  (SELECT sql sql, type type, tbl_name tbl_name, name name"
			      "     FROM sqlite_master UNION ALL"
			      "   SELECT sql, type, tbl_name, name FROM sqlite_temp_master) "
			      "WHERE type!='meta' AND sql NOTNULL AND name NOT LIKE 'sqlite_%'"
			      "ORDER BY substr(type,2,1), name",
			      callback, sql, &zErrMsg);
	}
	if (zErrMsg) {
	    fprintf(stderr, "Error: %s\n", zErrMsg);
	    sqlite3_free(zErrMsg);
	    rc = 1;
	} else if (rc != SQLITE_OK) {
	    fprintf(stderr, "Error: querying schema information\n");
	    rc = 1;
	} else {
	    rc = 0;
	}
    } else
     if (c == 's' && strncmp(azArg[0], "separator", n) == 0 && nArg == 2) {
	sqlite3_snprintf(sizeof(sql->separator), sql->separator,
			 "%.*s", (int) sizeof(sql->separator) - 1, azArg[1]);
    } else
     if (c == 's' && strncmp(azArg[0], "show", n) == 0 && nArg == 1) {
	int i;
	rpmsqlFprintf(sql, "%9.9s: %s\n", "echo", F_ISSET(sql, ECHO) ? "on" : "off");
	rpmsqlFprintf(sql, "%9.9s: %s\n", "explain",
		sql->explainPrev.valid ? "on" : "off");
	rpmsqlFprintf(sql, "%9.9s: %s\n", "headers",
		F_ISSET(sql, SHOWHDR) ? "on" : "off");
	rpmsqlFprintf(sql, "%9.9s: %s\n", "mode", modeDescr[sql->mode]);
	rpmsqlFprintf(sql, "%9.9s: ", "nullvalue");
	output_c_string(sql, sql->nullvalue);
	rpmsqlFprintf(sql, "\n");
	rpmsqlFprintf(sql, "%9.9s: %s\n", "output",
		strlen30(sql->outfile) ? sql->outfile : "stdout");
	rpmsqlFprintf(sql, "%9.9s: ", "separator");
	output_c_string(sql, sql->separator);
	rpmsqlFprintf(sql, "\n");
	rpmsqlFprintf(sql, "%9.9s: ", "width");
	for (i = 0;
	     i < (int) ArraySize(sql->colWidth) && sql->colWidth[i] != 0;
	     i++) {
	    rpmsqlFprintf(sql, "%d ", sql->colWidth[i]);
	}
	rpmsqlFprintf(sql, "\n");
    } else
     if (c == 't' && n > 1 && strncmp(azArg[0], "tables", n) == 0
	     && nArg < 3) {
	char **azResult;
	int nRow;
	char *zErrMsg;
	_rpmsqlOpenDB(sql);
	if (nArg == 1) {
	    rc = rpmsqlCmd(sql, "get_table", db,
			sqlite3_get_table(db,
				   "SELECT name FROM sqlite_master "
				   "WHERE type IN ('table','view') AND name NOT LIKE 'sqlite_%' "
				   "UNION ALL "
				   "SELECT name FROM sqlite_temp_master "
				   "WHERE type IN ('table','view') "
				   "ORDER BY 1",
				   &azResult, &nRow, 0, &zErrMsg));
	} else {
	    char * t;
	    t = rpmExpand("SELECT name FROM sqlite_master "
			  " WHERE type IN ('table','view') AND name LIKE '", azArg[1], "'"
			  " UNION ALL "
			  "SELECT name FROM sqlite_temp_master"
			  " WHERE type IN ('table','view') AND name LIKE '", azArg[1], "'"
				   "ORDER BY 1", NULL);
	    rc = rpmsqlCmd(sql, "get_table", db,
			sqlite3_get_table(db, t, &azResult, &nRow, 0,&zErrMsg));
	    t = _free(t);
	}
	if (zErrMsg) {
	    fprintf(stderr, "Error: %s\n", zErrMsg);
	    sqlite3_free(zErrMsg);
	    rc = 1;
	} else if (rc) {
	    fprintf(stderr,
		    "Error: querying sqlite_master and sqlite_temp_master\n");
	    rc = 1;
	} else {
	    int len, maxlen = 0;
	    int i, j;
	    int nPrintCol, nPrintRow;
	    for (i = 1; i <= nRow; i++) {
		if (azResult[i] == 0)
		    continue;
		len = strlen30(azResult[i]);
		if (len > maxlen)
		    maxlen = len;
	    }
	    nPrintCol = 80 / (maxlen + 2);
	    if (nPrintCol < 1)
		nPrintCol = 1;
	    nPrintRow = (nRow + nPrintCol - 1) / nPrintCol;
	    for (i = 0; i < nPrintRow; i++) {
		for (j = i + 1; j <= nRow; j += nPrintRow) {
		    char *zSp = j <= nPrintRow ? "" : "  ";
		    printf("%s%-*s", zSp, maxlen,
			   azResult[j] ? azResult[j] : "");
		}
		printf("\n");
	    }
	}
	sqlite3_free_table(azResult);
    } else
     if (c == 't' && n > 4 && strncmp(azArg[0], "timeout", n) == 0
	     && nArg == 2) {
	_rpmsqlOpenDB(sql);
	(void) rpmsqlCmd(sql, "busy_timeout", db,
		sqlite3_busy_timeout(db, atoi(azArg[1])));
    } else
     if (HAS_TIMER && c == 't' && n >= 5
	     && strncmp(azArg[0], "timer", n) == 0 && nArg == 2) {
	sql->enableTimer = booleanValue(azArg[1]);
    } else
     if (c == 'w' && strncmp(azArg[0], "width", n) == 0 && nArg > 1) {
	int j;
	assert(nArg <= ArraySize(azArg));
	for (j = 1; j < nArg && j < ArraySize(sql->colWidth); j++) {
	    sql->colWidth[j - 1] = atoi(azArg[j]);
	}
    } else
    {
	fprintf(stderr, "Error: unknown command or invalid arguments: "
		" \"%s\". Enter \".help\" for help\n", azArg[0]);
	rc = 1;
    }

    return rc;
}

#endif	/* defined(WITH_SQLITE) */

/*==============================================================*/

#if defined(WITH_SQLITE)

/*
** Return TRUE if a semicolon occurs anywhere in the first N characters
** of string z[].
*/
static int _contains_semicolon(const char *z, int N)
{
    int i;
SQLDBG((stderr, "--> %s(%s,%d)\n", __FUNCTION__, z, N));
    for (i = 0; i < N; i++) {
	if (z[i] == ';')
	    return 1;
    }
    return 0;
}

/*
** Test to see if a line consists entirely of whitespace.
*/
static int _all_whitespace(const char *z)
{
SQLDBG((stderr, "--> %s(%s)\n", __FUNCTION__, z));
    for (; *z; z++) {
	if (isspace(*(unsigned char *) z))
	    continue;
	if (*z == '/' && z[1] == '*') {
	    z += 2;
	    while (*z && (*z != '*' || z[1] != '/'))
		z++;
	    if (*z == 0)
		return 0;
	    z++;
	    continue;
	}
	if (*z == '-' && z[1] == '-') {
	    z += 2;
	    while (*z && *z != '\n')
		z++;
	    if (*z == 0)
		return 1;
	    continue;
	}
	return 0;
    }
    return 1;
}

/*
** Return TRUE if the line typed in is an SQL command terminator other
** than a semi-colon.  The SQL Server style "go" command is understood
** as is the Oracle "/".
*/
static int _is_command_terminator(const char *zLine)
{
SQLDBG((stderr, "--> %s(%s)\n", __FUNCTION__, zLine));
    while (isspace(*(unsigned char *) zLine))
	zLine++;
    if (zLine[0] == '/' && _all_whitespace(&zLine[1]))
	return 1;		/* Oracle */
    if (tolower(zLine[0]) == 'g' && tolower(zLine[1]) == 'o'
     && _all_whitespace(&zLine[2]))
	return 1;		/* SQL Server */
    return 0;
}

/*
** Return true if zSql is a complete SQL statement.  Return false if it
** ends in the middle of a string literal or C-style comment.
*/
static int _is_complete(char *zSql, int nSql)
{
    int rc;
SQLDBG((stderr, "--> %s(%s,%d)\n", __FUNCTION__, zSql, nSql));
    if (zSql == 0)
	return 1;
    zSql[nSql] = ';';
    zSql[nSql + 1] = 0;
    rc = sqlite3_complete(zSql);
    zSql[nSql] = 0;
    return rc;
}

/*
** Read input from *in and process it.  If *in==0 then input
** is interactive - the user is typing it it.  Otherwise, input
** is coming from a file or device.  A prompt is issued and history
** is saved only if input is interactive.  An interrupt signal will
** cause this routine to exit immediately, unless input is interactive.
**
** Return the number of errors.
*/
static int rpmsqlInput(rpmsql sql, void * _in)
{
    FILE * in = (FILE *) _in;
    FILE * out = sql->out;
    sqlite3 * db = (sqlite3 *) sql->I;
    char *zLine = 0;
    char *zSql = 0;
    int nSql = 0;
    int nSqlPrior = 0;
    char *zErrMsg;
    int rc;
    int errCnt = 0;
    int lineno = 0;
    int startline = 0;

SQLDBG((stderr, "--> %s(%p,%p)\n", __FUNCTION__, sql, in));
    while (errCnt == 0 || !F_ISSET(sql, BAIL)
	   || (in == 0 && F_ISSET(sql, INTERACTIVE))) {
	fflush(out);
	zLine = _free(zLine);
	zLine = rpmsqlInputOneLine(sql, zSql, in);
	if (zLine == 0) {
	    break;		/* We have reached EOF */
	}
	if (_rpmsqlSeenInterrupt) {
	    if (in != 0)
		break;
	    _rpmsqlSeenInterrupt = 0;
	}
	lineno++;
	if ((zSql == 0 || zSql[0] == 0) && _all_whitespace(zLine))
	    continue;
	if (zLine && zLine[0] == '.' && nSql == 0) {
	    if (F_ISSET(sql, ECHO))
		printf("%s\n", zLine);
	    rc = rpmsqlMetaCommand(sql, zLine);
	    if (rc == 2) {	/* exit requested */
		break;
	    } else if (rc) {
		errCnt++;
	    }
	    continue;
	}
	if (_is_command_terminator(zLine) && _is_complete(zSql, nSql)) {
	    memcpy(zLine, ";", 2);
	}
	nSqlPrior = nSql;
	if (zSql == 0) {
	    int i;
	    for (i = 0; zLine[i] && isspace((unsigned char) zLine[i]); i++) {
	    }
	    if (zLine[i] != 0) {
		nSql = strlen30(zLine);
		zSql = xmalloc(nSql + 3);
		memcpy(zSql, zLine, nSql + 1);
		startline = lineno;
	    }
	} else {
	    int len = strlen30(zLine);
	    zSql = xrealloc(zSql, nSql + len + 4);
	    zSql[nSql++] = '\n';
	    memcpy(&zSql[nSql], zLine, len + 1);
	    nSql += len;
	}
	if (zSql && _contains_semicolon(&zSql[nSqlPrior], nSql - nSqlPrior)
	    && sqlite3_complete(zSql)) {
	    sql->cnt = 0;
	    _rpmsqlOpenDB(sql);
	    BEGIN_TIMER(sql);
	    rc = _rpmsqlShellExec(sql, zSql, _rpmsqlShellCallback, &zErrMsg);
	    END_TIMER(sql);
	    if (rc || zErrMsg) {
		char zPrefix[100];
		if (in != 0 || !F_ISSET(sql, INTERACTIVE)) {
		    sqlite3_snprintf(sizeof(zPrefix), zPrefix,
				     "Error: near line %d:", startline);
		} else {
		    sqlite3_snprintf(sizeof(zPrefix), zPrefix, "Error:");
		}
		fprintf(stderr, "%s %s\n", zPrefix,
			zErrMsg ? zErrMsg : sqlite3_errmsg(db));
		zErrMsg = _free(zErrMsg);
		errCnt++;
	    }
	    zSql = _free(zSql);
	    nSql = 0;
	}
    }
    if (zSql) {
	if (!_all_whitespace(zSql))
	    fprintf(stderr, "Error: incomplete SQL: %s\n", zSql);
	zSql = _free(zSql);
    }
    zLine = _free(zLine);
    return errCnt;
}

/*
** Read input from the sqliterc file parameter. 
** If sqliterc is NULL, take input from ~/.sqliterc
**
** Returns the number of errors.
*/
static int rpmsqlInitRC(rpmsql sql, const char *sqliterc)
{
    FILE *in = NULL;
    int rc = 0;

    if (sqliterc == NULL) 
	sqliterc = sql->zInitrc;
    if (sqliterc == NULL)
	return rc;
    in = fopen(sqliterc, "rb");
    if (in && !ferror(in)) {
	if (F_ISSET(sql, INTERACTIVE))
	    fprintf(stderr, "-- Loading resources from %s\n", sqliterc);
	rc = rpmsqlInput(sql, in);
    }
    if (in)
	fclose(in);
    return rc;
}

#endif	/* defined(WITH_SQLITE) */

/*==============================================================*/

#if defined(WITH_SQLITE)
static void rpmsqlArgCallback(poptContext con,
			      /*@unused@ */ enum poptCallbackReason reason,
			      const struct poptOption *opt,
			      const char *arg,
			      /*@unused@ */ void *_data)
	/*@ */
{
    rpmsql sql = &_sql;

    /* XXX avoid accidental collisions with POPT_BIT_SET for flags */
    if (opt->arg == NULL)
    switch (opt->val) {
    case 'S':				/*    -separator x */
assert(arg != NULL);
	sqlite3_snprintf(sizeof(sql->separator), sql->separator,
			     "%.*s", (int) sizeof(sql->separator) - 1, arg);
	break;
    case 'N':				/*    -nullvalue text */
assert(arg != NULL);
	sqlite3_snprintf(sizeof(sql->nullvalue), sql->nullvalue,
			     "%.*s", (int) sizeof(sql->nullvalue) - 1, arg);
	break;
    case 'V':				/*    -version */
	printf("%s\n", sqlite3_libversion());
	/*@-exitarg@ */ exit(0); /*@=exitarg@ */
	/*@notreached@ */ break;
    default:
	fprintf(stderr, _("%s: Unknown callback(0x%x)\n"),
		    __FUNCTION__, (unsigned) opt->val);
	poptPrintUsage(con, stderr, 0);
	/*@-exitarg@ */ exit(2); /*@=exitarg@ */
	/*@notreached@ */ break;
    }
}

static struct poptOption _rpmsqlOptions[] = {
    /*@-type@*//* FIX: cast? */
    {NULL, '\0',
     POPT_ARG_CALLBACK | POPT_CBFLAG_INC_DATA | POPT_CBFLAG_CONTINUE,
     rpmsqlArgCallback, 0, NULL, NULL},
/*@=type@*/

 { "debug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_ONEDASH|POPT_ARGFLAG_DOC_HIDDEN, &_rpmsql_debug, -1,
	N_("Debug embedded SQL interpreter"), NULL},

 { "init", '\0', POPT_ARG_STRING|POPT_ARGFLAG_ONEDASH,	&_sql.zInitFile, 0,
	N_("read/process named FILE"), N_("FILE") },
 { "echo", '\0', POPT_BIT_SET|POPT_ARGFLAG_ONEDASH,	&_sql.flags, RPMSQL_FLAGS_ECHO,
	N_("print commands before execution"), NULL },

 { "load", '\0', POPT_BIT_SET|POPT_ARGFLAG_TOGGLE|POPT_ARGFLAG_ONEDASH, &_sql.flags, RPMSQL_FLAGS_NOLOAD,
	N_("disable extnsion loading (normally enabled)"), NULL },
 { "header", '\0', POPT_BIT_SET|POPT_ARGFLAG_TOGGLE|POPT_ARGFLAG_ONEDASH, &_sql.flags, RPMSQL_FLAGS_SHOWHDR,
	N_("turn headers on or off"), NULL },

 { "bail", '\0', POPT_BIT_SET|POPT_ARGFLAG_ONEDASH,	&_sql.flags, RPMSQL_FLAGS_BAIL,
	N_("stop after hitting an error"), NULL },

 { "interactive", '\0', POPT_BIT_SET|POPT_ARGFLAG_TOGGLE|POPT_ARGFLAG_ONEDASH,	&_sql.flags, RPMSQL_FLAGS_INTERACTIVE,
	N_("force interactive I/O"), NULL },
 { "batch", '\0', POPT_BIT_CLR|POPT_ARGFLAG_TOGGLE|POPT_ARGFLAG_ONEDASH, &_sql.flags, RPMSQL_FLAGS_INTERACTIVE,
	N_("force batch I/O"), NULL },

 { "column", '\0', POPT_ARG_VAL|POPT_ARGFLAG_ONEDASH,	&_sql.mode, RPMSQL_MODE_COLUMN,
	N_("set output mode to 'column'"), NULL },
 { "csv", '\0', POPT_ARG_VAL|POPT_ARGFLAG_ONEDASH,	&_sql.mode, RPMSQL_MODE_CSV,
	N_("set output mode to 'csv'"), NULL },
 { "html", '\0', POPT_ARG_VAL|POPT_ARGFLAG_ONEDASH,	&_sql.mode, RPMSQL_MODE_HTML,
	N_("set output mode to HTML"), NULL },
 { "line", '\0', POPT_ARG_VAL|POPT_ARGFLAG_ONEDASH,	&_sql.mode, RPMSQL_MODE_LINE,
	N_("set output mode to 'line'"), NULL },
 { "list", '\0', POPT_ARG_VAL|POPT_ARGFLAG_ONEDASH,	&_sql.mode, RPMSQL_MODE_LIST,
	N_("set output mode to 'list'"), NULL },
 { "separator", '\0', POPT_ARG_STRING|POPT_ARGFLAG_ONEDASH,	0, 'S',
	N_("set output field separator (|)"), N_("CHAR") },
 { "nullvalue", '\0', POPT_ARG_STRING|POPT_ARGFLAG_ONEDASH,	0, 'N',
	N_("set text string for NULL values"), N_("TEXT") },

 { "version", '\0', POPT_ARG_NONE|POPT_ARGFLAG_ONEDASH,	0, 'V',
	N_("show SQLite version"), NULL},

#ifdef	NOTYET
 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmioAllPoptTable, 0,
	N_("Common options for all rpmio executables:"), NULL},
#endif

    POPT_AUTOHELP {NULL, (char) -1, POPT_ARG_INCLUDE_TABLE, NULL, 0,
	N_("\
Usage: dbsql [OPTIONS] FILENAME [SQL]\n\
FILENAME is the name of an SQLite database. A new database is created\n\
if the file does not previously exist.\n\
\n\
OPTIONS include:\n\
   -help                show this message\n\
   -init filename       read/process named file\n\
   -echo                print commands before execution\n\
   -[no]header          turn headers on or off\n\
   -bail                stop after hitting an error\n\
   -interactive         force interactive I/O\n\
   -batch               force batch I/O\n\
   -column              set output mode to 'column'\n\
   -csv                 set output mode to 'csv'\n\
   -html                set output mode to HTML\n\
   -line                set output mode to 'line'\n\
   -list                set output mode to 'list'\n\
   -separator 'x'       set output field separator (|)\n\
   -nullvalue 'text'    set text string for NULL values\n\
   -version             show SQLite version\n\
"), NULL},

    POPT_TABLEEND
};

#endif	/* defined(WITH_SQLITE) */

/*==============================================================*/

static void rpmsqlFini(void * _sql)
	/*@globals fileSystem @*/
	/*@modifies *_sql, fileSystem @*/
{
    rpmsql sql = _sql;

SQLDBG((stderr, "==> %s(%p)\n", __FUNCTION__, sql));

    sql->zDestTable = _free(sql->zDestTable);

    if (sql->out && fileno(sql->out) > STDERR_FILENO)
	fclose(sql->out);
    sql->out = NULL;
    if (sql->pLog && fileno(sql->pLog) > STDERR_FILENO)
	fclose(sql->pLog);
    sql->pLog = NULL;
    if (sql->iotrace && fileno(sql->iotrace) > STDERR_FILENO)
	fclose(sql->iotrace);
    sql->iotrace = NULL;

    /* XXX INTERACTIVE cruft. */
    sql->zHome = _free(sql->zHome);
    sql->zInitrc = _free(sql->zInitrc);
    sql->zHistory = _free(sql->zHistory);
    sql->zPrompt = _free(sql->zPrompt);
    sql->zContinue = _free(sql->zContinue);

    sql->zDbFilename = _free(sql->zDbFilename);
    sql->zInitFile = _free(sql->zInitFile);
    sql->av = argvFree(sql->av);
#if defined(WITH_SQLITE)
    if (sql->I) {
	sqlite3 * db = (sqlite3 *)sql->I;
	(void) rpmsqlCmd(sql, "close", db,
		sqlite3_close(db));
    }
#endif
    sql->I = NULL;
    (void) rpmiobFree(sql->iob);
    sql->iob = NULL;
}

/*@unchecked@*/ /*@only@*/ /*@null@*/
rpmioPool _rpmsqlPool;

static rpmsql rpmsqlGetPool(/*@null@*/ rpmioPool pool)
	/*@globals _rpmsqlPool, fileSystem @*/
	/*@modifies pool, _rpmsqlPool, fileSystem @*/
{
    rpmsql sql;

    if (_rpmsqlPool == NULL) {
	_rpmsqlPool = rpmioNewPool("sql", sizeof(*sql), -1, _rpmsql_debug,
			NULL, NULL, rpmsqlFini);
	pool = _rpmsqlPool;
    }
    sql = (rpmsql) rpmioGetPool(pool, sizeof(*sql));
    memset(((char *)sql)+sizeof(sql->_item), 0, sizeof(*sql)-sizeof(sql->_item));
    return sql;
}

const char ** rpmsqlArgv(rpmsql sql, int * argcp)
{
    const char ** av = sql->av;

    if (argcp)
	*argcp = argvCount(av);
    return av;
}

#if defined(WITH_SQLITE)
static void rpmsqlInitPopt(rpmsql sql, int ac, char ** av, poptOption tbl)
	/*@modifies sql @*/
{
    void *use =  sql->_item.use;
    void *pool = sql->_item.pool;
    rpmiob iob = sql->iob;
    poptContext con;
    int rc;

    if (av == NULL || av[0] == NULL || av[1] == NULL)
	goto exit;

    con = poptGetContext(av[0], ac, (const char **)av, tbl, 0);

    /* Process all options into _sql, whine if unknown options. */
    while ((rc = poptGetNextOpt(con)) > 0) {
	const char * arg = poptGetOptArg(con);
	arg = _free(arg);
	switch (rc) {
	default:
		fprintf(stderr, _("%s: option table misconfigured (%d)\n"),
			__FUNCTION__, rc);
		break;
	}
    }
    /* XXX FIXME: arrange error return iff rc < -1. */
if (rc)
SQLDBG((stderr, "%s: poptGetNextOpt rc(%d): %s\n", __FUNCTION__, rc, poptStrerror(rc)));

    *sql = _sql;	/* structure assignment */

    sql->iob = iob;
    sql->_item.use = use;
    sql->_item.pool = pool;
    rc = argvAppend(&sql->av, poptGetArgs(con));

    con = poptFreeContext(con);

exit:
SQLDBG((stderr, "<== %s(%p, %p[%u], %p)\n", __FUNCTION__, sql, av, (unsigned)ac, tbl));
}
#endif /* defined(WITH_SQLITE) */

rpmsql rpmsqlNew(char ** av, uint32_t flags)
{
    rpmsql sql =
	(flags & 0x80000000) ? rpmsqlI() :
	rpmsqlGetPool(_rpmsqlPool);
    int ac = argvCount((ARGV_t)av);

SQLDBG((stderr, "==> %s(%p[%u], 0x%x)\n", __FUNCTION__, av, (unsigned)ac, flags));

    sql->flags = flags;		/* XXX useful? */

#if defined(WITH_SQLITE)
    if (av && av[1]) {
	static int _oneshot;
	sqlite3 * db = NULL;
	int xx;

	if (!_oneshot) {
#if defined(SQLITE_CONFIG_LOG)
	    sqlite3_config(SQLITE_CONFIG_LOG, shellLog, sql);
#endif
	    sqlite3_config(SQLITE_CONFIG_SINGLETHREAD);
	    _oneshot++;
	}

	/* Initialize defaults for popt parsing. */
	memset(&_sql, 0, sizeof(_sql));
	_sql.flags = flags;	/* XXX INTERACTIVE defaulted here. */
	_sql.mode = RPMSQL_MODE_LIST;
	memcpy(_sql.separator, "|", 2);
	rpmsqlInitPopt(sql, ac, av, (poptOption) _rpmsqlOptions);

	/* The 1st argument is the database to open (or :memory: default). */
	if (sql->av && sql->av[0]) {
	    sql->zDbFilename = xstrdup(sql->av[0]);	/* XXX strdup? */
	    /* If database alread exists, open immediately. */
	    if (Access(sql->zDbFilename, R_OK)) {
		xx = rpmsqlCmd(sql, "open", db,	/* XXX watchout: arg order */
			sqlite3_open(sql->zDbFilename, &db));
		sql->I = (void *) db;
	    }
	} else
	    sql->zDbFilename = xstrdup(":memory:");

	/* Read ~/.sqliterc (if specified), then reparse options. */
	if (sql->zInitFile) {
	    db = (sqlite3 *)sql->I;
	    /* XXX memory leaks are here */
	    xx = rpmsqlInitRC(sql, sql->zInitFile);
	    sql->I = NULL;	/* XXX avoid the db close */
	    rpmsqlFini(sql);
	    rpmsqlInitPopt(sql, ac, av, (poptOption) _rpmsqlOptions);
	    sql->I = (void *)db;
	}

    }

    { /* XXX INTERACTIVE cruft. */
	static const char _zInitrc[]	= "/.sqliterc";
	static const char _zHistory[]	= "/.sqlite_history";
	/* XXX getpwuid? */
	sql->zHome = xstrdup(getenv("HOME"));
	sql->zInitrc = rpmGetPath(sql->zHome, _zInitrc, NULL);
	sql->zHistory = rpmGetPath(sql->zHome, _zHistory, NULL);
	/*
	 ** Prompt strings. Initialized in main. Settable with
	 **   .prompt main continue
	 */
	/* Initialize the prompt from basename(argv[0]). */
	if (sql->zPrompt == NULL) {
	    char * t = xstrdup((av && av[0] ? av[0] : "sql"));
	    char * bn = basename(t);
	    sql->zPrompt = rpmExpand(bn, "> ", NULL);
	    t = _free(t);
	    sql->zContinue = t = xstrdup(sql->zPrompt);
	    while (*t && *t != '>')
		*t++ = '-';
	}

	/* Determine whether stdio or rpmiob should be used for output */
	sql->out = (F_ISSET(sql, INTERACTIVE) ? stdout : NULL);
    }
#else	/* WITH_SQLITE */
    if (av)
	(void) argvAppend(&sql->av, (ARGV_t) av);	/* XXX useful? */
#endif	/* WITH_SQLITE */

    if (!F_ISSET(sql, INTERACTIVE) && sql->iob == NULL)
	sql->iob = rpmiobNew(0);

    return rpmsqlLink(sql);
}

rpmRC rpmsqlRun(rpmsql sql, const char * str, const char ** resultp)
{
    rpmRC rc = RPMRC_FAIL;

SQLDBG((stderr, "==> %s(%p,%p[%u]) \"%s\"\n", __FUNCTION__, sql, str, (unsigned)(str ? strlen(str) : 0), str));

    if (sql == NULL) sql = rpmsqlI();

#if defined(WITH_SQLITE)
    if (str != NULL) {
	const char * s = str;
	FILE * fp = NULL;

	/* Ignore leading whitespace. */
	while (*s && xisspace((int)*s))
	    s++;

	/* Perform the SQL operation(s). */
	if (*s == '\0') {				/* INTERACTIVE */
	    static int oneshot;
	    uint32_t _flags = sql->flags;
	    FILE * _out = sql->out;
	    sql->flags |= RPMSQL_FLAGS_INTERACTIVE;
	    sql->out = stdout;
	    if (!oneshot) {
#ifdef	REFERENCE
		extern char *db_full_version(int *, int *, int *, int *, int *);
		fprintf(sql->out, "%s\n"
			"Enter \".help\" for instructions\n"
			"Enter SQL statements terminated with a \";\"\n",
			db_full_version(NULL, NULL, NULL, NULL, NULL));
#endif
		fprintf(sql->out, "SQLite version %s\n\t(%s)\n"
			"Enter \".help\" for instructions\n"
			"Enter SQL statements terminated with a \";\"\n",
			sqlite3_libversion(), sqlite3_sourceid());
#if defined(HAVE_READLINE) && HAVE_READLINE==1
		if (sql->zHistory)
		    read_history(sql->zHistory);
#endif
		oneshot++;
	    }
	    rc = rpmsqlInput(sql, NULL);
            if (sql->zHistory) {
                stifle_history(100);
                write_history(sql->zHistory);
            }
	    if (rc != 0) rc = RPMRC_FAIL;
	    sql->out = _out;
	    sql->flags = _flags;
	} else
	if (*s == '-' || !strcmp(s, "stdin")) {		/* STDIN */
	    rc = rpmsqlInput(sql, stdin);
	    if (rc != 0) rc = RPMRC_FAIL;
	} else
	if (*s == '/') {				/* FILE */
	    fp = fopen(s, "rb");
	    if (fp && !ferror(fp))
		rc = rpmsqlInput(sql, fp);
	    if (rc != 0) rc = RPMRC_FAIL;
	} else {					/* STRING */
	    if (*s == '.') {
		char * t = xstrdup(s);
		rc = rpmsqlMetaCommand(sql, t);
		t = _free(t);
	    } else {
		char * zErrMsg = NULL;
		_rpmsqlOpenDB(sql);
		rc = _rpmsqlShellExec(sql, s, _rpmsqlShellCallback, &zErrMsg);
		if (zErrMsg) {
		    fprintf(stderr, "Error: %s\n", zErrMsg);
		    zErrMsg = _free(zErrMsg);
		    if (rc == 0) rc = RPMRC_FAIL;
		} else if (rc != 0) {
		    fprintf(stderr, "Error: unable to process SQL \"%s\"\n", s);
		    rc = RPMRC_FAIL;
		}
	    }
	}
	    
	if (fp && fileno(fp) > STDERR_FILENO) (void) fclose(fp);

	/* Return the SQL output. */
	if (sql->iob) {
	    (void) rpmiobRTrim(sql->iob);
SQLDBG((stderr, "==========\n%s\n==========\n", rpmiobStr(sql->iob)));
	    if (resultp)
		*resultp = rpmiobStr(sql->iob);		/* XXX strdup? */
	}

    }
#endif	/* WITH_SQLITE */

SQLDBG((stderr, "<== %s(%p,%p[%u]) rc %d\n", __FUNCTION__, sql, str, (unsigned)(str ? strlen(str) : 0), rc));

    return rc;
}
