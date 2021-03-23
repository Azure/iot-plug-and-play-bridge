#include <string.h>

#define LED_ON '1'
#define LED_OFF '0'

typedef enum _LED_TARGET
{
    SYSTEM_BLUE,
    SYSTEM_RED,
    SYSTEM_GREEN,
    INVENTORY, 
    UPGRADE,
    UNSUPPORTED
} LED_TARGET;

typedef struct _IMPINJ_R700_LED
{
    LED_TARGET ledTarget;
    char sysfsPath[255];
} IMPINJ_R700_LED, *PIMPINJ_R700_LED;

#define LED_LIST_SIZE 5

static IMPINJ_R700_LED R700_LED_LIST[] =
{
    {SYSTEM_BLUE,   "/sys/devices/platform/leds/leds/status-blue/brightness"},
    {SYSTEM_RED,    "/sys/devices/platform/leds/leds/status-red/brightness"},
    {SYSTEM_GREEN,  "/sys/devices/platform/leds/leds/status-green/brightness"},
    {INVENTORY,     "/sys/devices/platform/leds/leds/inventory-status/brightness"},
    {UPGRADE,       "/sys/devices/platform/leds/leds/upgrade-status/brightness"},
};

void writeLed(LED_TARGET ledTarget, char value);
