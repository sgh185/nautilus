/* 
 * This file is part of the Nautilus AeroKernel developed
 * by the Hobbes and V3VEE Projects with funding from the 
 * United States National  Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  The Hobbes Project is a collaboration
 * led by Sandia National Laboratories that includes several national 
 * laboratories and universities. You can find out more at:
 * http://www.v3vee.org  and
 * http://xstack.sandia.gov/hobbes
 *
 * Copyright (c) 2015, Kyle C. Hale <kh@u.northwestern.edu>
 * Copyright (c) 2015, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Author: Kyle C. Hale <kh@u.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

/* 
 *
 * This file is intended to support libc functions
 * 
 *
 */


#define LIBCCOMPAT 1

#include <nautilus/nautilus.h>
#include <nautilus/libccompat.h>
#include <nautilus/thread.h>
#include <nautilus/errno.h>
#include <nautilus/random.h>
#include <dev/hpet.h>
#include <nautilus/log_data.h>


int errno=0;

#define GEN_DEF(x) \
    int x (void) { \
        UNDEF_FUN_ERR(); \
        return 0; \
    } 


// Structs needed for LUA 



static int maxExponent = 511;	/* Largest possible base 10 exponent.  Any
				 * exponent larger than this will already
				 * produce underflow or overflow, so there's
				 * no need to worry about additional digits.
				 */
static double powersOf10[] = {	/* Table giving binary powers of 10.  Entry */
    10.,			/* is 10^2^i.  Used to convert decimal */
    100.,			/* exponents into floating-point numbers. */
    1.0e4,
    1.0e8,
    1.0e16,
    1.0e32,
    1.0e64,
    1.0e128,
    1.0e256
};



struct lconv {
    char *decimal_point;      //"."          LC_NUMERIC
    char *grouping;           //""           LC_NUMERIC
    char *thousands_sep;      //""           LC_NUMERIC

    char *mon_decimal_point;  //""           LC_MONETARY
    char *mon_grouping;       //""           LC_MONETARY
    char *mon_thousands_sep;  //""           LC_MONETARY

    char *negative_sign;      //""           LC_MONETARY
    char *positive_sign;      //""           LC_MONETARY
    char *currency_symbol;    //""           LC_MONETARY
    char frac_digits;         //CHAR_MAX     LC_MONETARY
    char n_cs_precedes;       //CHAR_MAX     LC_MONETARY
    char n_sep_by_space;      //CHAR_MAX     LC_MONETARY
    char n_sign_posn;         //CHAR_MAX     LC_MONETARY
    char p_cs_precedes;       //CHAR_MAX     LC_MONETARY
    char p_sep_by_space;      //CHAR_MAX     LC_MONETARY
    char p_sign_posn;         //CHAR_MAX     LC_MONETARY

    char *int_curr_symbol;
    char int_frac_digits;
    char int_n_cs_precedes;
    char int_n_sep_by_space;
    char int_n_sign_posn;
    char int_p_cs_precedes;
    char int_p_sep_by_space;
    char int_p_sign_posn;
};





//=========================================================
static uint64_t dummy_mono_clock = 0;

time_t 
time (time_t * timer)
{
    struct timespec tp;
    clock_gettime(CLOCK_MONOTONIC, &tp);

    if (timer) {
        *timer = tp.tv_nsec;
    }
    return tp.tv_nsec;
}


void 
abort(void) 
{
    printk("Thread called abort\n");
    nk_thread_exit(NULL);
}


int 
__popcountdi2 (long long a)
{
    unsigned long long x2 = (unsigned long long)a;
    x2 = x2 - ((x2 >> 1) & 0x5555555555555555uLL);
    /* Every 2 bits holds the sum of every pair of bits (32) */
    x2 = ((x2 >> 2) & 0x3333333333333333uLL) + (x2 & 0x3333333333333333uLL);
    /* Every 4 bits holds the sum of every 4-set of bits (3 significant bits) (16) */
    x2 = (x2 + (x2 >> 4)) & 0x0F0F0F0F0F0F0F0FuLL;
    /* Every 8 bits holds the sum of every 8-set of bits (4 significant bits) (8) */
    unsigned x = (unsigned)(x2 + (x2 >> 32));
    /* The lower 32 bits hold four 16 bit sums (5 significant bits). */
    /*   Upper 32 bits are garbage */
    x = x + (x >> 16);
    /* The lower 16 bits hold two 32 bit sums (6 significant bits). */
    /*   Upper 16 bits are garbage */
    return (x + (x >> 8)) & 0x0000007F;  /* (7 significant bits) */
}

void 
exit(int status)
{
    printk("Thread called exit (status=%d)\n", status);
    nk_thread_exit((void*)(long)status);
}

int 
clock_gettime (clockid_t clk_id, struct timespec * tp)
{

    if (clk_id != CLOCK_MONOTONIC) {
        printk("NAUTILUS WARNING: using invalid clock type\n");
        return -EINVAL;
    }

    if (!tp) {
        printk("NAUTILUS WARNING: using invalid timespec\n");
        return -EINVAL;
    }

#ifdef NAUT_CONFIG_HPET
    //uint64_t freq = nk_hpet_get_freq();
    uint64_t cnt  = nk_hpet_get_cntr();
    //uint64_t nsec = (1000000000/freq) * cnt;
    uint64_t nsec = cnt * nk_hpet_nanos_per_tick();
    tp->tv_sec    = nsec / 1000000000;
    tp->tv_nsec   = nsec % 1000000000;
#else
    /* runs at "10kHz" */
    tp->tv_nsec = dummy_mono_clock*100000;
    tp->tv_sec  = dummy_mono_clock/10000;
    ++dummy_mono_clock;
#endif

    return 0;
}


void 
__assert_fail (const char * assertion, const char * file, unsigned line, const char * function)
{
    panic("Failed assertion in %s: %s at %s, line %d, RA=%lx\n",
            function,
            assertion,
            file,
            line,
            __builtin_return_address(0));
}


int 
vfprintf (FILE * stream, const char * format, va_list arg)
{
    UNDEF_FUN_ERR();
    return -1;
}


int 
rand (void) {
    int r;

    nk_get_rand_bytes((uint8_t*)&r, 4);

    r = r<0 ? -r : r;

    return r % RAND_MAX;
}

void 
srand (unsigned int seed)
{
    uint64_t s = ((uint64_t)seed) << 32;
    nk_rand_seed(s | 0x330e);
}
    

/* NOTE: these are likely not in any way compliant
 */
void
srand48 (long int seedval)
{
    uint64_t tmp = (((uint64_t) seedval) & 0xffffffffull) << 32;
    nk_rand_seed(tmp | 0x330eULL);
}


// extract this functionality so that we can implemnt erand, nrand, mrand, jrand, lcong later
static inline uint64_t _pump_rand(uint64_t xi, uint64_t a, uint64_t c)
{
    uint64_t m      = 0x1ULL << 48;
    uint64_t xi_new = (a*xi + c) % m;

    return xi_new;
}    

// advance the random number generator and return the new
// x_i value (48 bit value)
static inline uint64_t pump_rand()
{
    struct nk_rand_info * rand = per_cpu_get(rand);

    uint64_t xi_new = _pump_rand(rand->xi, 0x5deece66dULL, 0xbULL);
    
    nk_rand_set_xi(xi_new);

    return xi_new;
}

   
long int
lrand48 (void)
{
    uint64_t val = pump_rand();

    // return top 31 bits
    return (val >> 17) & 0x8fffffffULL;
}


union ieee754dbl {
    double d;
    uint64_t ui;
    struct {
        uint32_t mantissa1 : 32;
        uint32_t mantissa0 : 20;
        uint32_t exponent  : 11;
        uint32_t sign      :1;
    } __packed;
} __packed;

double
drand48(void) 
{
    uint64_t val = pump_rand();
    
    // now have 48 bits of randomness, but we need 52 bits for a double
    // we will copy the 48 bits into the high order bits of the mantissa

    uint16_t *xic   = (uint16_t*)&val;

    union ieee754dbl ret;

    ret.sign     = 0;
    ret.exponent = 0x3ff;
    // 16 bits from xic[2], 4 bits from xic[1]
    ret.mantissa0 = (((uint32_t)xic[2]) << 4) | (((uint32_t)xic[1]) >> 12);
    // 12 bits from xic[1], 16 bits from xic[0], remaining 4 bits are zeros
    ret.mantissa1 = ((((uint32_t)xic[1]) & 0xfff) << 20) | (xic[0] << 4);

    return ret.d - 1.0;
}


char *
strerror (int errnum)
{
    UNDEF_FUN_ERR();
    return NULL;
}
FILE *tmpfile(void)
{

    UNDEF_FUN_ERR();
    return NULL;

}
int 
ferror (FILE * f)
{
    UNDEF_FUN_ERR();
    return -1;
}
FILE *freopen(const char *fname, const char *mode,FILE *stream)
{

    UNDEF_FUN_ERR();
    return NULL;
}
int 
fclose (FILE * f)
{
    UNDEF_FUN_ERR();
    return -1;
}


FILE * 
fopen (const char * path, FILE * f)
{
    UNDEF_FUN_ERR();
    return NULL;
}


FILE * 
fopen64 (const char * path, FILE * f)
{
    UNDEF_FUN_ERR();
    return NULL;
}


FILE * 
fdopen (int fd, const char * mode)
{
    UNDEF_FUN_ERR();
    return NULL;
}

char *getenv(const char *name)
{

    UNDEF_FUN_ERR();
    return NULL;
}
//For LUA Support
clock_t clock()
{

    UNDEF_FUN_ERR();
    return -1;
}
//For LUA Support
char *tmpnam(char *s)
{

    UNDEF_FUN_ERR();
    return NULL;
}

//For LUA Support
int remove(const char *path)
{

    UNDEF_FUN_ERR();
    return -1;
}
//For LUA Support
int rename(const char *old, const char *new)
{

    UNDEF_FUN_ERR();
    return -1;
}
//For LUA Support
int system(const char *command)
{

    UNDEF_FUN_ERR();
    return -1;
}
//For LUA Support

int fflush (FILE * f)
{
    return 0;
}

void (*signal(int sig, void (*func)(int)))(int ){
    //    printk("\nSIGNAL Function:");
    return 0;
}

//For LUA Support

int 
fprintf (FILE * f, const char * s, ...)
{
#if 0
    UNDEF_FUN_ERR();
    return -1;
#else
    va_list arg;
    va_start(arg,s);
    vprintk(s, arg);
    va_end(arg);
    return 0;
#endif
}

//For LUA
int setvbuf(FILE *restrict stream, char *restrict buf, int type,
       size_t size)
{

    UNDEF_FUN_ERR();
    return -1;
}


//For LUA

int fscanf(FILE *restrict stream, const char *restrict format, ... )
{

    UNDEF_FUN_ERR();
    return -1;

}
//For LUA
void clearerr(FILE *stream)
{

    UNDEF_FUN_ERR();
   // return NULL;
}

int printf (const char * s, ...)
{
#if 0
    UNDEF_FUN_ERR();
    return -1;
#else
    va_list arg;
    va_start(arg,s);
    vprintk(s, arg);
    va_end(arg);
    return 0;
#endif
}

int 
fputc (int c, FILE * f) 
{
    printk("%c");
    return c;
}


int 
fputs (const char * s, FILE * f)
{
    printk("%s\n", s);
    return 0;
}
size_t fwrite (const void *ptr, size_t size, size_t nmemb, FILE *stream)
{

   printk("\n %s",ptr); 
   //UNDEF_FUN_ERR();
   return (int)size;
}

size_t 
fread (void * ptr, size_t size, size_t count, FILE * stream)
{
    UNDEF_FUN_ERR();
    return -1;
}

int
getwc (FILE * stream)
{
    UNDEF_FUN_ERR();
    return -1;
}



size_t 
__ctype_get_mb_cur_max (void)
{
    UNDEF_FUN_ERR();
    return 0;
}
//For LUA Support
int fseek(FILE *stream, long offset, int whence)
{

    UNDEF_FUN_ERR();
    return 1;
}

int 
fseeko64 (FILE *fp, uint64_t offset, int whence)
{
    UNDEF_FUN_ERR();
    return -1;
}


int 
ungetc (int character, FILE * stream)
{
    UNDEF_FUN_ERR();
    return -1;
}
 

uint64_t 
lseek64 (int fd, uint64_t offset, int whence)
{
    UNDEF_FUN_ERR();
    return 0;
}

uint64_t 
ftello64 (FILE *stream)
{
    UNDEF_FUN_ERR();
    return 0;
}
//For LUA Support
long ftell(FILE *x)
{
    UNDEF_FUN_ERR();
    return -1;
}
int poll (struct pollfd *fds, nfds_t nfds, int timeout)
{
    UNDEF_FUN_ERR();
    return 1;
}

int 
ioctl (int d, unsigned long request, ...)
{
    UNDEF_FUN_ERR();
    return -1;
}

int 
syscall (int number, ...)
{
    UNDEF_FUN_ERR();
    return -1;
}



char * 
setlocale (int category, const char *locale)
{
    UNDEF_FUN_ERR();
    return NULL;
}

locale_t 
__duplocale (locale_t locobj)
{
    UNDEF_FUN_ERR();
    return NULL;
}

char * 
bindtextdomain (const char * domainname, const char * dirname)
{
    UNDEF_FUN_ERR();
    return NULL;
}

char * 
textdomain (const char * domainname)
{
    UNDEF_FUN_ERR();
    return NULL;
}

locale_t 
__newlocale (int category_mask, const char *locale, locale_t base)
{
    return (locale_t)((ulong_t)base | (ulong_t)category_mask);
}

char *
__nl_langinfo_l (nl_item item, locale_t locale)
{
    UNDEF_FUN_ERR();
    return NULL;
}

char *
gettext (const char * msgid)
{
    char * ret = (char*)msgid;
    UNDEF_FUN_ERR();
    return ret;
}

int getc(FILE* arg)
{

    UNDEF_FUN_ERR();
    return -1;

}

int fileno(FILE* f)
{
    UNDEF_FUN_ERR();
    return 0;
}

int isatty(int fd)
{
    UNDEF_FUN_ERR();
    return 0;
}

//LUA SPECIFIC....................
size_t strftime(char *str, size_t maxsize, const char *format, const struct tm *timeptr)
{
    UNDEF_FUN_ERR();
    return 0;
}
int feof(FILE * x)
{
    UNDEF_FUN_ERR();
    return 0;
}

char *fgets(char *str, int n, FILE *stream)
{
    UNDEF_FUN_ERR();
    return NULL;
}
void *memchr(const void *str, int c, size_t n)
{
    UNDEF_FUN_ERR();
    return NULL;
}
/*void longjmp(int *x, int __y)
{
    UNDEF_FUN_ERR();
}

int setjmp(int *x)
{
    return 0;
} */
double fabs(double __x){
    UNDEF_FUN_ERR();
    return abs(__x);
}
double atan(double __x){
    UNDEF_FUN_ERR();
    return 45.000;
}
double atan2(double y, double x){
    UNDEF_FUN_ERR();
    return 135.00;
}
double fmod(double y, double x){
    // this is replacement to actual fmod() (/nautilus/libccompat)
    // defining own fmod similar to the one defined in (/gcc/libc)
    printk("\n in fmod y %d,x %d",(int)y,(int)x); 
//	return y -x*(y/x);
      return (int)y% (int)x;
}
 
double fmodnew(int y, int x){
    // this is replacement to actual fmod() (/nautilus/libccompat)
    // defining own fmod similar to the one defined in (/gcc/libc)
  //  printk("\n in fmod y %d,x %d",y,x); 
	return y -x*(y/x);
}
double modf(double y, double *x){
  *x = 0;
//	printk("\n in modf");
    UNDEF_FUN_ERR();
  return 0.000;
}
double frexp(double x, int *e){
  UNDEF_FUN_ERR();
  *e = 0;
  return 0.5;
}
double ldexp(double x, int exp){
  UNDEF_FUN_ERR();
  return x;
}


int ischar(unsigned char *str)
{
    UNDEF_FUN_ERR();
	return 1;
}

// strtod is implemented in dtoa.c

/*----------*/
double abs(double x)
{
UNDEF_FUN_ERR();
//should return absolute value of x
if (x<0)
	return -1*x;
else
	return x;
}
double sin(double x)
{
UNDEF_FUN_ERR();
return x;
}
double sinh(double x)
{
UNDEF_FUN_ERR();
return x;
}
double cos(double x)
{
UNDEF_FUN_ERR();
return x;
}
double cosh(double x)
{
UNDEF_FUN_ERR();
return x;
}
time_t mktime(struct tm *timeptr)
{
    UNDEF_FUN_ERR();
    return 0;
}
struct tm *localtime(const time_t *timer)
{
    UNDEF_FUN_ERR();
    return NULL;
}
struct tm *gmtime(const time_t *timer)
{
    UNDEF_FUN_ERR();
    return NULL;
}
int strcoll(const char *str1, const char *str2)
{
    UNDEF_FUN_ERR();
    return 0;
}
double tan(double x)
{
UNDEF_FUN_ERR();
return x;
}
double tanh(double x)
{
UNDEF_FUN_ERR();
return x;
}
double asin(double x)
{
UNDEF_FUN_ERR();
return x;
}
double acos(double x)
{
UNDEF_FUN_ERR();
return x;
}
double ceil(double x)
{
UNDEF_FUN_ERR();
return x;
}
double floor(double x)
{
UNDEF_FUN_ERR();
return x;
}
double difftime(time_t time1, time_t time2)
{
    UNDEF_FUN_ERR();
    return 0;
}
double sqrt(double x)
{
UNDEF_FUN_ERR();
return x;
}
double pow(double x, double y)
{
UNDEF_FUN_ERR();
return x;
}


/*
 * Double-precision log(x) function.
 *
 * Copyright (c) 2018, Arm Limited.
 * SPDX-License-Identifier: MIT
 */

#define __MUSL_LOG_T __log_data.tab
#define __MUSL_LOG_T2 __log_data.tab2
#define __MUSL_LOG_B __log_data.poly1
#define __MUSL_LOG_A __log_data.poly
#define __MUSL_LOG_Ln2hi __log_data.ln2hi
#define __MUSL_LOG_Ln2lo __log_data.ln2lo
#define __MUSL_LOG_N (1 << __MUSL_LOG_TABLE_BITS)
#define __MUSL_LOG_OFF 0x3fe6000000000000
#define __MUSL_asuint64(f) ((union{double _f; uint64_t _i;}){f})._i
#define __MUSL_asdouble(i) ((union{uint64_t _i; double _f;}){i})._f
#define __MUSL_predict_true(x) __builtin_expect(!!(x), 1)
#define __MUSL_predict_false(x) __builtin_expect(x, 0)
#define WANT_ROUNDING 1
#define INFINITY __builtin_inff()
#ifndef double_t
#define double_t double
#endif
static inline double __musl_fp_barrier(double x)
{
	volatile double y = x;
	return y;
}

static double __musl_math_divzero(uint32_t sign)
{
	return __musl_fp_barrier(sign ? -1.0 : 1.0) / 0.0;
}

static double __musl_math_invalid(double x)
{
    return (x - x) / (x - x);
}

static inline double __musl_eval_as_double(double x)
{
	double y = x;
	return y;
}

/* Top 16 bits of a double.  */
static inline uint32_t top16(double x)
{
	return __MUSL_asuint64(x) >> 48;
}

double log(double x)
{
	double_t w, z, r, r2, r3, y, invc, logc, kd, hi, lo;
	uint64_t ix, iz, tmp;
	uint32_t top;
	int k, i;

	ix = __MUSL_asuint64(x);
	top = top16(x);
#define __MUSL_LOG_LO __MUSL_asuint64(1.0 - 0x1p-4)
#define __MUSL_LOG_HI __MUSL_asuint64(1.0 + 0x1.09p-4)
	if (__MUSL_predict_false(ix - __MUSL_LOG_LO < __MUSL_LOG_HI - __MUSL_LOG_LO)) {
		/* Handle close to 1.0 inputs separately.  */
		/* Fix sign of zero with downward rounding when x==1.  */
		if (WANT_ROUNDING && __MUSL_predict_false(ix == __MUSL_asuint64(1.0)))
			return 0;
		r = x - 1.0;
		r2 = r * r;
		r3 = r * r2;
		y = r3 *
		    (__MUSL_LOG_B[1] + r * __MUSL_LOG_B[2] + r2 * __MUSL_LOG_B[3] +
		     r3 * (__MUSL_LOG_B[4] + r * __MUSL_LOG_B[5] + r2 * __MUSL_LOG_B[6] +
			   r3 * (__MUSL_LOG_B[7] + r * __MUSL_LOG_B[8] + r2 * __MUSL_LOG_B[9] + r3 * __MUSL_LOG_B[10])));
		/* Worst-case error is around 0.507 ULP.  */
		w = r * 0x1p27;
		double_t rhi = r + w - w;
		double_t rlo = r - rhi;
		w = rhi * rhi * __MUSL_LOG_B[0]; /* __MUSL_LOG_B[0] == -0.5.  */
		hi = r + w;
		lo = r - hi + w;
		lo += __MUSL_LOG_B[0] * rlo * (rhi + r);
		y += lo;
		y += hi;
		return __musl_eval_as_double(y);
	}
	if (__MUSL_predict_false(top - 0x0010 >= 0x7ff0 - 0x0010)) {
		/* x < 0x1p-1022 or inf or nan.  */
		if (ix * 2 == 0)
			return __musl_math_divzero(1);
		if (ix == __MUSL_asuint64(INFINITY)) /* log(inf) == inf.  */
			return x;
		if ((top & 0x8000) || (top & 0x7ff0) == 0x7ff0)
			return __musl_math_invalid(x);
		/* x is subnormal, normalize it.  */
		ix = __MUSL_asuint64(x * 0x1p52);
		ix -= 52ULL << 52;
	}

	/* x = 2^k z; where z is in range [__MUSL_LOG_OFF,2*__MUSL_LOG_OFF) and exact.
	   The range is split into N subintervals.
	   The ith subinterval contains z and c is near its center.  */
	tmp = ix - __MUSL_LOG_OFF;
	i = (tmp >> (52 - __MUSL_LOG_TABLE_BITS)) % __MUSL_LOG_N;
	//k = (int64_t)tmp >> 52; /* arithmetic shift */
	k = (long)tmp >> 52; /* arithmetic shift */
	iz = ix - (tmp & 0xfffULL << 52);
	invc = __MUSL_LOG_T[i].invc;
	logc = __MUSL_LOG_T[i].logc;
	z = __MUSL_asdouble(iz);

	/* log(x) = log1p(z/c-1) + log(c) + k*Ln2.  */
	/* r ~= z/c - 1, |r| < 1/(2*__MUSL_LOG_N).  */
#if __FP_FAST_FMA
	/* rounding error: 0x1p-55/__MUSL_LOG_N.  */
	r = __builtin_fma(z, invc, -1.0);
#else
	/* rounding error: 0x1p-55/__MUSL_LOG_N + 0x1p-66.  */
	r = (z - __MUSL_LOG_T2[i].chi - __MUSL_LOG_T2[i].clo) * invc;
#endif
	kd = (double_t)k;

	/* hi + lo = r + log(c) + k*Ln2.  */
	w = kd * __MUSL_LOG_Ln2hi + logc;
	hi = w + r;
	lo = w - hi + r + kd * __MUSL_LOG_Ln2lo;

	/* log(x) = lo + (log1p(r) - r) + hi.  */
	r2 = r * r; /* rounding error: 0x1p-54/__MUSL_LOG_N^2.  */
	/* Worst case error if |y| > 0x1p-5:
	   0.5 + 4.13/__MUSL_LOG_N + abs-poly-error*2^57 ULP (+ 0.002 ULP without fma)
	   Worst case error if |y| > 0x1p-4:
	   0.5 + 2.06/__MUSL_LOG_N + abs-poly-error*2^56 ULP (+ 0.001 ULP without fma).  */
	y = lo + r2 * __MUSL_LOG_A[0] +
	    r * r2 * (__MUSL_LOG_A[1] + r * __MUSL_LOG_A[2] + r2 * (__MUSL_LOG_A[3] + r * __MUSL_LOG_A[4])) + hi;
	return __musl_eval_as_double(y);
}

double log10(double x)
{
UNDEF_FUN_ERR();
return x;
}
double exp(double x)
{
UNDEF_FUN_ERR();
return x;
}

/* became lazy... */
GEN_DEF(writev)
GEN_DEF(ungetwc)
GEN_DEF(__errno_location)
GEN_DEF(write)
GEN_DEF(wcrtomb)
GEN_DEF(mbrtowc)
//GEN_DEF(getc)
GEN_DEF(__iswctype_l)
GEN_DEF(wcslen)
GEN_DEF(__strtof_l)
//GEN_DEF(stderr)
GEN_DEF(wmemset)
//GEN_DEF(stdin)
//GEN_DEF(fileno)
GEN_DEF(__fxstat64)
GEN_DEF(putc)
GEN_DEF(__wcscoll_l)
GEN_DEF(__towlower_l)
GEN_DEF(wctob)
GEN_DEF(mbsrtowcs)
GEN_DEF(read)
GEN_DEF(wmemmove)
GEN_DEF(__strxfrm_l)
GEN_DEF(wmemchr)
GEN_DEF(__freelocale)
GEN_DEF(__wcsftime_l)
GEN_DEF(wmemcpy)
GEN_DEF(putwc)
GEN_DEF(__stack_chk_fail)
GEN_DEF(__wcsxfrm_l)
GEN_DEF(wcscmp)
GEN_DEF(wcsnrtombs)
GEN_DEF(__strcoll_l)
//GEN_DEF(stdout)
GEN_DEF(btowc)
//GEN_DEF(memchr)
GEN_DEF(strtold_l)
GEN_DEF(wmemcmp)
GEN_DEF(__strtod_l)
//GEN_DEF(setvbuf)
GEN_DEF(__wctype_l)
GEN_DEF(__towupper_l)
GEN_DEF(__uselocale)
GEN_DEF(__strftime_l)
GEN_DEF(mbsnrtowcs)
GEN_DEF(pthread_mutex_init)
GEN_DEF(pthread_mutex_lock)
GEN_DEF(pthread_mutex_unlock)
GEN_DEF(wcscoll)
//GEN_DEF(strcoll)
GEN_DEF(towupper)
GEN_DEF(towlower)
GEN_DEF(iswctype)
//GEN_DEF(strftime)
GEN_DEF(wcsftime)
GEN_DEF(wctype)
GEN_DEF(strtold)
//GEN_DEF(strtod)
GEN_DEF(strtof)
GEN_DEF(__ctype_b_loc)
GEN_DEF(__ctype_toupper_loc)
GEN_DEF(__ctype_tolower_loc)
GEN_DEF(strxfrm)
GEN_DEF(wcsxfrm)
GEN_DEF(__kernel_standard);
GEN_DEF(__get_cpu_features);
