#ifndef BATTERY_SENS
#define BATTERY_SENS

#define IVALID_BATTERY_CHARGE -1.0

#define BATTERY_CHARGE_MIN 10.4
#define BATTERY_CHARGE_MAX 12.6

void init_battery_sensor();
float read_battery_charge();
char* battery_sensor_name();

#endif