#ifndef RPM_SENS
#define RPM_SENS

#define INVALID_RPM -1.0

#define RPM_MAX 3500

void init_rpm_sensor();
float read_rpm();
char* rpm_sensor_name();

#endif