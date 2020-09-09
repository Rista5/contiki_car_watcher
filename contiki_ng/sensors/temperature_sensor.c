#include "temperature_sensor.h"
#include "demo_sensor.h"
#include <stdlib.h>


struct temp_sens
{
    char initialized;
    float min_val;
    float max_val;
};

static struct temp_sens temp_sens =
{
    .initialized = SENSOR_NOT_INITIALIZED
};

void init_temp_sensor()
{
    temp_sens.initialized = SENSOR_INITALIZED;
    temp_sens.min_val = TEMP_MIN;
    temp_sens.max_val = TEMP_MAX;
}

float read_temperature()
{
    if (temp_sens.initialized == SENSOR_INITALIZED)
    {
        int diff =(int) (temp_sens.max_val - temp_sens.min_val);
        return (float)(rand()%diff) + temp_sens.min_val;
    }
    else 
        return INVALID_TEMPERATURE;
}

char* temp_sensor_name()
{
    return "temperature";
}