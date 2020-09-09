#ifndef TEMP_SENS
#define TEMP_SENS

#define INVALID_TEMPERATURE -500.0

#define TEMP_MIN 0.0
#define TEMP_MAX 120.0

void init_temp_sensor();
float read_temperature();
char* temp_sensor_name();

#endif