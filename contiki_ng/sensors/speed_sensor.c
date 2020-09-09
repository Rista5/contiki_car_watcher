#include "demo_sensor.h"
#include <stdlib.h>
#include "speed_sensor.h"

struct speed_sens
{
    char initialized;
    float min_val;
    float max_val;
};

static struct speed_sens speed_sens =
{
    .initialized = SENSOR_NOT_INITIALIZED
};

void init_speed_sensor()
{
    speed_sens.initialized = SENSOR_INITALIZED;
    speed_sens.min_val = SPEED_MIN;
    speed_sens.max_val = SPEED_MAX;
}

float read_speed()
{
    if (speed_sens.initialized == SENSOR_INITALIZED)
    {
        int diff = (int) (speed_sens.max_val - speed_sens.min_val);
        return (float)(rand()%diff) + speed_sens.min_val;
    }
    else 
        return INVALID_SPEED;
}

char* speed_sensor_name()
{
    return "speed";
}