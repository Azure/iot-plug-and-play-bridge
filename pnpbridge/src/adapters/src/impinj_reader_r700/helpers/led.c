#include "led.h"
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

void writeLed(LED_TARGET ledTarget, char value) {
    
    int fd;

    // fprintf(stdout, "\nValue: %c, LED Path: %s", value, R700_LED_LIST[ledTarget].sysfsPath);

    fd = open(R700_LED_LIST[ledTarget].sysfsPath, O_WRONLY);

    write(fd, &value, 1);

    close(fd);
}