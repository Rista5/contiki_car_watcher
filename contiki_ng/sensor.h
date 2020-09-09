#ifndef SENSOR_DEF
#define SENSOR_DEF

#define MAX_SENSOR_COUNT 8
#define CONSTRAINT_MET 0


enum callback_policy
{
    ONCE=1,               // invoke callback at first sensor reading when cnstraint is not met
    AT_EVERY_READING=2    // invoke callback at every reading when constraint is not met
};

typedef struct sensor_constraint
{
    char    policy;     // callback_policy that specifys how many times to invoke callback
    char    status;     // has CONSTRAINT_MET value if constraint is met or value of policy if not
    float   min_val;    // minimal allowed value for reading that mets constraint
    float   max_val;    // maximal allowed value for reading that mets constraint
    void    (*callback)(char* sensor_name, float current_value, float min_val,
        float max_val); // callback to execute when given restriction is not met
} sens_constr;

typedef struct isensor
{
    void    (*init)(void);          // callback for initialization of a sensor
    float   (*value)(void);         // callback for reading value of a sensor
    char*   (*sensor_name)(void);   // callback that reaturns unique name of a sensor in a system
    sens_constr* constraint;        // constraint for values of sensor reading 
} isensor;

typedef struct sensor_array
{
    int count;
    struct isensor arr[MAX_SENSOR_COUNT];
} sensor_array;

void add_sensor(sensor_array* sens_arr, void (*init)(void),
                float (*value)(void), char* (*sensor_name)(void));
void remove_sensor(sensor_array* sens_arr, int index);
float read_sensor_with_constraint_check(isensor sens);

int find_sensor_index_in_array(sensor_array* sens_arr, char* sensor_name);
void add_constraint(sensor_array* sens_arr, char* sensor_name, float min_val,
                    float max_val, void (*callback)(char*, float, float, float), 
                    char policy);
void remove_constraint(sensor_array* sens_arr, char* sensor_name);


#endif