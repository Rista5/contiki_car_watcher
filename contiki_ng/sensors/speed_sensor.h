#ifndef SPEED_SENS
#define SPEED_SENS

#define INVALID_SPEED -1.0

#define SPEED_MIN 0.0
#define SPEED_MAX 120.0

void init_speed_sensor();
float read_speed();
char* speed_sensor_name();

#endif
