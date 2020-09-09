#include "demo_sensor.h"
#include <stdlib.h>
#include "rpm_sensor.h"

struct rpm_sens
{
    char initialized;
    int max_val;
};

static struct rpm_sens rpm_sens =
{
    .initialized = SENSOR_NOT_INITIALIZED
};

void init_rpm_sensor()
{
    rpm_sens.initialized = SENSOR_INITALIZED;
    rpm_sens.max_val = RPM_MAX;
}

float read_rpm()
{
    if (rpm_sens.initialized == SENSOR_INITALIZED)
    {
        return (float)(rand()%rpm_sens.max_val);
    }
    else 
        return INVALID_RPM;
}

char* rpm_sensor_name()
{
    return "rpm";
}