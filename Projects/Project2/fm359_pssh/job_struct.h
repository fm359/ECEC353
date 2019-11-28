#ifndef _job_struct_h_
#define _job_struct_h_

#include <limits.h>

typedef enum {
    STOPPED,
    TERM,
    BG,
    FG,
} JobStatus;

typedef struct {
    char* name;
    pid_t* pids;
    unsigned int npids;
    unsigned int rem_pids;
    int bg_job_done;
    int bg_job_stopped;
    int job_mem_freed;
    pid_t pgid;
    JobStatus status;
} Job;

#endif /* _parse_h_ */
