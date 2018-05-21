#include <stdio.h>

extern "C" void printi(int64_t i) 
{
    printf("%lld\n", i);
}

extern "C" void printf(double i) 
{
    printf("%.1lf\n", i);
}

extern "C" void prints(char *c, int64_t l) 
{
    for (int64_t i = 0; i < l; i++)
        putc(c[i], stdout);
    putc('\n', stdout);
}

