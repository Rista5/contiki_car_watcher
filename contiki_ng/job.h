#ifndef JOB
#define JOB

#include <list.h>
#include <stddef.h>

typedef struct job_request
{
    void (*func)(void *);
    void* argv;
} job_request;

void init_job_list();
void add_job(job_request* job);
void process_jobs();

#endif