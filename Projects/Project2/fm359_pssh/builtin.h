#ifndef _builtin_h_
#define _builtin_h_

#include "parse.h"
#include "job_struct.h"

int is_builtin (char* cmd);
void set_fg_pgid (pid_t pgid);
void fg_bg (Task T, Job* current_jobs, int fg_or_bg);
void disp_jobs (Job* current_jobs);
void kill_cmd (Task T, Job* current_jobs);
void builtin_which (Task T, char* outfile);

#endif /* _builtin_h_ */
