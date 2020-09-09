#ifndef GAS_SENS
#define GAS_SENS

#define INVALID_GAS -1.0

#define GAS_MAX 40.0

void init_gas_sensor();
float read_gas();
char* gas_sensor_name();


#endif