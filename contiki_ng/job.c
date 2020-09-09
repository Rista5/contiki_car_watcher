#include "job.h"
#include <stdlib.h>
#include <stdio.h>

typedef struct job_list_node
{
    struct list* node;
    job_request job; 
} job_list_node;

LIST(job_list);

void init_job_list()
{
    list_init(job_list);
}

void add_job(job_request* job)
{
    job_list_node* node;
    
    node = (job_list_node*) malloc(sizeof(job_list_node));
    node->node = NULL;
    node->job = *job;
    
    list_push(job_list, node);
}

void process_jobs()
{
    job_list_node* ptr;
    
    while (list_head(job_list) != NULL) 
    {
        ptr = (job_list_node*) list_pop(job_list);
        ptr->job.func(ptr->job.argv);
        free(ptr);
    }
}
