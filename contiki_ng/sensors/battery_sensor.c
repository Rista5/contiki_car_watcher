#include "demo_sensor.h"
#include <stdlib.h>
#include "battery_sensor.h"

struct battery_sens
{
    char initialized;
    float max_val;
    float min_val;
};

static struct battery_sens battery_sens =
{
    .initialized = SENSOR_NOT_INITIALIZED
};

void init_battery_sensor()
{
    battery_sens.initialized = SENSOR_INITALIZED;
    battery_sens.min_val = BATTERY_CHARGE_MIN;
    battery_sens.max_val = BATTERY_CHARGE_MAX;
}

float read_battery_charge()
{
    if (battery_sens.initialized == SENSOR_INITALIZED)
    {
        int diff = (int)(battery_sens.max_val - battery_sens.min_val);
        return (float)(rand()%diff) + battery_sens.min_val;
    }
    else 
        return IVALID_BATTERY_CHARGE;
}

char* battery_sensor_name()
{
    return "battery_charge";
}