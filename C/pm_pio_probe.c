#include <stdio.h>
#include <picomite/pio.h>
int main(void)
{
    for (int i = 0; i < 3; ++i)
        printf("PIO%d: %s\n", i, pm_pio_available(i) ? "available" : "reserved");
    return 0;
}
