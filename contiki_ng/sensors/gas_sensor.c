#include "demo_sensor.h"
#include <stdlib.h>
#include "gas_sensor.h"

struct gas_sens
{
    char initialized;
    int max_val;
};

static struct gas_sens gas_sens =
{
    .initialized = SENSOR_NOT_INITIALIZED
};

void init_gas_sensor()
{
    gas_sens.initialized = SENSOR_INITALIZED;
    gas_sens.max_val = GAS_MAX;
}

float read_gas()
{
    if (gas_sens.initialized == SENSOR_INITALIZED)
    {
        return (float)(rand()%gas_sens.max_val);
    }
    else 
        return INVALID_GAS;
}

char* gas_sensor_name()
{
    return "gas";
}