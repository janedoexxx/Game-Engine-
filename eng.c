#include "eng.h"
#include <stdio.h>

// Engine state
static u8 rn = 0;

void ini(void)
{
    printf("Engine init\n");
    rn = 1;
}

void run(void)
{
    if (!rn) return;
    printf("Engine running\n");
}

void fin(void)
{
    printf("Engine shutdown\n");
    rn = 0;
}
