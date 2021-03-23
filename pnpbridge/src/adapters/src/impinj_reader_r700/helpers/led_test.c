#include "led.h"
#include <unistd.h>
#include <stdio.h>

void main()
{
    for (int i; i <= LED_LIST_SIZE; i++)
    {
        writeLed(R700_LED_LIST[i].ledTarget, LED_ON);

        usleep(1000000);

        writeLed(R700_LED_LIST[i].ledTarget, LED_OFF);

        usleep(1000000);
    }

    writeLed(R700_LED_LIST[SYSTEM_BLUE].ledTarget, LED_ON);

    fprintf(stdout, "\n\n");
}