#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

static uint64_t data[10000];
static int index = 0;
static uint64_t last = 0;

inline uint64_t __attribute__((always_inline))
rdtsc (void)
{
    uint32_t lo, hi;
    asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return lo | ((uint64_t)(hi) << 32);
}

__attribute__((noinline)) void callback(void)
{
	if (index >= 100) { return; }
	uint64_t temp = rdtsc();
	data[index] = temp - last;
	last = temp;
	index++;

	return;
}

__attribute__((noinline, annotate("nohook"))) int myF(int i)
{
	return i * 42 + 24 - 4;
}

__attribute__((noinline, annotate("nohook"))) int nextF(int i)
{
	return i * 24 + 42 - 2;
}

double operations(double a, double b)
{
	double test = 0;
	int i;
	
	uint64_t interval = rdtsc();
	
	for (i = 0; i < 1000; i++)
	{
		test *= 4.2 + b;
		test += a + b + 33.22;
		int j;

#if 1 
		#pragma nounroll
		for (j = 0; j < 800; j++)
		{
			a += j;
			b *= (j / 10) - 24;
		}
#endif
		a /= 2;
		test *= (a * b);
	}

	interval = rdtsc() - interval;
	printf("Operations Interval: %lu\n", interval);
	printf("Index: %d\n", index);

	return test;
}

uint64_t fib(void)
{
	uint64_t fib_num = 4200;	
	uint64_t memo[fib_num + 2];

	uint64_t interval = rdtsc();

	// Set base cases
	memo[0] = 0;
	memo[1] = 1;

	uint64_t k;
	for (k = 2; k < fib_num; k++) {
		memo[k] = memo[k - 1] + memo[k - 2];
	}

	interval = rdtsc() - interval;
	printf("Fib Interval: %lu\n", interval);

	volatile int j = 0;
	double a = 123.123;
	a *= (123 * (j + 1) * 823.21);
	
	float test = 234.11111111;
	a /= ((double) test * 123.111 - memo[k]);
	uint64_t ret = memo[k] * (uint64_t) a;
	return ret;

	return memo[k];
}

int main()
{
	return 0;
	int i;
	volatile int sum = 0;
	
	
	uint64_t interval = rdtsc();
	
#if 1
	for (i = 0; i < 10000; i++)
		sum += (myF(i) + nextF(i));

	interval = rdtsc() - interval;
	printf("Interval: %lu\n", interval);

#endif

#if 1
	for (i = 0; i < 10000; i++)
	{
		sum += (myF(i) + nextF(i));
		if (sum > 234234234) abort();
		if (sum > 500000) break;
	}
#endif
	sum += (int) operations(1.2, 2.4);

	// sum += (int) fib();

	int *l = (int *) malloc(sizeof(int));
	while (l != NULL)
	{
		*l = i;
		l++;
		i++;
	}

	int j = index;
	printf("Index: %d\n", index);
	for (i = 1; i < j; i++)
		printf("Timing Interval: %lu, %p\n", data[i], l);

	return sum;
}


