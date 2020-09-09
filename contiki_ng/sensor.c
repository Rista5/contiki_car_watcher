#include "sensor.h"
#include "stdbool.h"
#include "stddef.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void add_sensor(sensor_array* sens_arr, void (*init)(void),
                float (*value)(void), char* (*sensor_name)(void))
{
    if (sens_arr->count >= MAX_SENSOR_COUNT)
    {
        printf("Sensor array is full!\nCant add sensor %s\n", sensor_name());
        return;
    }

    sens_arr->arr[sens_arr->count].init = *init;
    sens_arr->arr[sens_arr->count].value = *value;
    sens_arr->arr[sens_arr->count].sensor_name = *sensor_name;
    sens_arr->arr[sens_arr->count].constraint = NULL;
    sens_arr->count++;
}

void remove_sensor(sensor_array* sens_arr, int index)
{
    int i;
    if (sens_arr->count <= 0 && sens_arr->count <= index)
    {
        printf("Sensor array is empty or index out of bounds\n");
        return;
    }

    for(i=index; i<sens_arr->count-1; i++)
    {
        sens_arr->arr[i] = sens_arr->arr[i+1];
    }
    sens_arr->count--;
}

float read_sensor_with_constraint_check(isensor sens)
{
    float value;
    char* sensor_name;

    value = sens.value();
    if (sens.constraint != NULL &&
        (sens.constraint->min_val > value || sens.constraint->max_val < value))
    {
        sensor_name = sens.sensor_name();
        if (sens.constraint->callback != NULL && sens.constraint->status != ONCE)
        {
            sens.constraint->callback(sensor_name, value, 
                sens.constraint->min_val, sens.constraint->max_val);
        }
        sens.constraint->status = sens.constraint->policy;
    }
    else if (sens.constraint != NULL)
    {
        sens.constraint->status = CONSTRAINT_MET;
    }

    return value;
}

int find_sensor_index_in_array(sensor_array* sens_arr, char* sensor_name)
{
    int i = 0;
    bool found = false;

    while(!found && i < sens_arr->count)
    {
        if (strcmp(sens_arr->arr[i].sensor_name(), sensor_name) == 0)
        {
            found = true;
        }
        i++;
    }

    if(found)
        return i - 1;
    else
        return -1;
}

void add_constraint(sensor_array* sens_arr, char* sensor_name, float min_val,
                    float max_val, void (*callback)(char*, float, float, float),
                    char policy)
{
    int index;
    sens_constr* new;

    index = find_sensor_index_in_array(sens_arr, sensor_name);
    
    if(index < 0)
    {
        printf("Could not add restriction, sensor %s not found\n", sensor_name);
        return;
    }

    new = malloc(sizeof(sens_constr));

    new->policy = policy;
    new->min_val = min_val;
    new->max_val = max_val;
    new->callback = callback;

    sens_arr->arr[index].constraint = new;
}

void remove_constraint(sensor_array* sens_arr, char* sensor_name)
{
    int index;

    index = find_sensor_index_in_array(sens_arr, sensor_name);
    if(index < 0)
    {
        printf("Could not remove restriction, sensor %s not found\n", sensor_name);
        return;
    }

    free(sens_arr->arr[index].constraint);
    sens_arr->arr[index].constraint = NULL;
}