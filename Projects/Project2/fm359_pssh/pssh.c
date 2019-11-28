#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <readline/readline.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>

#include "builtin.h"
#include "parse.h"
#include "job_struct.h"

/*******************************************
 * Set to 1 to view the command line parse *
 *******************************************/
#define DEBUG_PARSE 0

#define READ_SIDE 0
#define WRITE_SIDE 1
#define BUFF_SIZE 512
#define MAX_NUM_JOBS 100

Job current_jobs[MAX_NUM_JOBS];
int lowest_job_num = 0;
int highest_job_num = 0;
int job_replaced = 0;/////NOT NEEDED?

void print_banner ()
{
    printf ("                    ________   \n");
    printf ("_________________________  /_  \n");

    printf ("__  /_/ /(__  )_(__  )_  / / / \n");
    printf ("_  .___//____/ /____/ /_/ /_/  \n");
    printf ("/_/ Type 'exit' or ctrl+c to quit\n\n");
}


/* returns a string for building the prompt
 *
 * Note:
 *   If you modify this function to return a string on the heap,
 *   be sure to free() it later when appropriate!  */
static char* build_prompt (char* cdir)
{
    if (getcwd (cdir, sizeof(char) * BUFF_SIZE) == NULL) {
        fprintf (stderr, "Error: Failed to print current working directory\n");
        exit (EXIT_FAILURE);
    }
    else {
	strcat (cdir, "$ ");
    }

    return  cdir;
}


/* return true if command is found, either:
 *   - a valid fully qualified path was supplied to an existing file
 *   - the executable file was found in the system's PATH
 * false is returned otherwise */
static int command_found (const char* cmd)
{
    char* dir;
    char* tmp;
    char* PATH;
    char* state;
    char probe[PATH_MAX];

    int ret = 0;

    if (access (cmd, X_OK) == 0)
        return 1;

    PATH = strdup (getenv("PATH"));

    for (tmp=PATH; ; tmp=NULL) {
        dir = strtok_r (tmp, ":", &state);
        if (!dir)
            break;

        strncpy (probe, dir, PATH_MAX);
        strncat (probe, "/", PATH_MAX);
        strncat (probe, cmd, PATH_MAX);

        if (access (probe, X_OK) == 0) {
            ret = 1;
            break;
        }
    }

    free (PATH);
    return ret;
}

/* Redirects stdin or stdout to a file when an input or output file is specified
 * in the command. This function takes in the name of the file and a number to specify
 * if the file is input or output. Then it opens the file and redirects either the file
 * to stdin or redirects stdout to the file by changing the file descripter table. */
void file_redirect (char* file, int redirect_side)
{
    int fd;
    if (redirect_side == 0) {
        if ((fd = open (file, O_RDONLY)) < 0) {
            fprintf (stderr, "Error: Failed to open input file\n");
            exit (EXIT_FAILURE);
        }   
        if (dup2 (fd, STDIN_FILENO) == -1) {
            fprintf (stderr, "Error: Failed to read input file. Check if file exists\n");
            exit (EXIT_FAILURE);
        } 
    }
    else if (redirect_side == 1) {
        if ((fd = open (file, O_CREAT | O_WRONLY | O_TRUNC, 0664)) < 0) {
            fprintf (stderr, "Error: Failed to open output file\n");
            exit (EXIT_FAILURE);
        }
        if (dup2 (fd, STDOUT_FILENO) == -1) {
            fprintf (stderr, "Error: Failed to redirect stdout to output file\n");
            exit (EXIT_FAILURE);
        }
    }	
    close (fd);
}

int find_lowest_job_num ()
{
    int i;
    for (i = 0; i < highest_job_num; i++) { //////////could give error
        if (current_jobs[i].status == TERM) {
            return i;
        }
    }
    return lowest_job_num;
}

int get_job_num (pid_t pid)
{
    int i, j, job_num = 0, break_flag = 0;
    for (i = 0; i < highest_job_num; i++) {
        for (j = 0; j < current_jobs[i].npids; j++) {
            if (current_jobs[i].pids[j] == pid) {
                job_num = i;
		break_flag = 1;
            }
        }
        if (break_flag) {
            break;
        }
    }
    return job_num;
}

pid_t get_pgid (pid_t pid)
{
    int i, j;
    pid_t pgrp_id = 0;
    for (i = 0; i < highest_job_num; i++) {
        for (j = 0; j < current_jobs[i].npids; j++) {
            if (current_jobs[i].pids[j] == pid) {
                pgrp_id = current_jobs[i].pgid;
            }
        }
        if (pgrp_id) {
            break;
        }
    }
    return pgrp_id;
}

void manage_dead_child (pid_t pid)
{
    int i, job_num = 0;

    job_num = get_job_num (pid);
    for (i = 0; i < current_jobs[job_num].npids; i++) {
        if (current_jobs[job_num].pids[i] == pid) {
            current_jobs[job_num].pids[i] = 0;
            current_jobs[job_num].rem_pids--;
        }
    }
}

void handler_sigchld (int sig)
{
    pid_t child_pid, pgrp_id;
    int status, job_num = 0;

    while ((child_pid = waitpid (-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
        job_num = get_job_num (child_pid);
	pgrp_id = get_pgid (child_pid);
        if (WIFSTOPPED (status)) {
            if (current_jobs[job_num].status == FG) {
                printf ("[%d] + suspended    %s\n", job_num, current_jobs[job_num].name);
            }
            else if (current_jobs[job_num].status == BG) {
                current_jobs[job_num].bg_job_stopped = 1;
            }
            current_jobs[job_num].status = STOPPED;
            set_fg_pgid (getpgrp());
            continue;
        }
        else if (WIFCONTINUED (status)) {
            if (current_jobs[job_num].status == STOPPED) {
                current_jobs[job_num].status = BG;
            }
            if (current_jobs[job_num].status == BG && child_pid == pgrp_id) {
                printf ("[%d] + continued    %s\n", job_num, current_jobs[job_num].name);
            }
            continue;
        }
        else {
            manage_dead_child (child_pid);
            if (current_jobs[job_num].rem_pids == 0) {
                if (current_jobs[job_num].status == BG || current_jobs[job_num].status == STOPPED) {
                    current_jobs[job_num].bg_job_done = 1;
                }
                else if (current_jobs[job_num].status == FG) {
                    set_fg_pgid (getpgrp());
                }
                current_jobs[job_num].status = TERM;
            }
            continue;
        }
    }
}

void handler_sigttin (int sig)
{
    while (tcgetpgrp(STDIN_FILENO) != getpid ())
        pause ();
}

void handler_sigttou (int sig)
{
    while (tcgetpgrp(STDOUT_FILENO) != getpid ())
        pause ();
}

/* Called upon receiving a successful parse.
 * This function is responsible for cycling through the
 * tasks, and forking, executing, etc as necessary to get
 * the job done! */
void execute_tasks (Parse* P)
{
    unsigned int t;
    int fd[2];
    pid_t pid[P->ntasks];
    int builtin_flag = 0, cmd_found = 1;

    /* Used to store the address of the read side of the previous tasks pipe */
    int previous_pipe = 0;

    for (t = 0; t < P->ntasks; t++) {
        /* Create a pipe for every iteration if there are more than one tasks */
        if (P->ntasks > 1) {
            if (pipe (fd) == -1) {
                fprintf (stderr, "error -- failed to create pipe\n");
                exit (EXIT_FAILURE);
            }
        }

        if (is_builtin (P->tasks[t].cmd)) {
            builtin_flag = 1;
            if (!strcmp (P->tasks[t].cmd, "exit")) {
                exit (EXIT_SUCCESS);
            }
	    else if (!strcmp (P->tasks[t].cmd, "fg")) {
                fg_bg (P->tasks[t], current_jobs, 0);
            }
            else if (!strcmp (P->tasks[t].cmd, "bg")) {
                fg_bg (P->tasks[t], current_jobs, 1);
            }
	    else if (!strcmp (P->tasks[t].cmd, "kill")) {
                kill_cmd (P->tasks[t], current_jobs);
            }
            else if (!strcmp (P->tasks[t].cmd, "jobs")) {
                disp_jobs (current_jobs);
            }
            else if (!strcmp (P->tasks[t].cmd, "which")) {
                builtin_which (P->tasks[t], P->outfile);
            }
            else {
                printf ("pssh: builtin command: %s (not implemented!)\n", P->tasks[t].cmd);
            }
        }
	else if (command_found (P->tasks[t].cmd)) {
            pid[t] = fork ();
            if (pid[t] < 0) {
                fprintf (stderr, "error -> failed to fork()\n");
                exit (EXIT_FAILURE);
	    }

            setpgid (pid[t], pid[0]);

            if (pid[t] > 0) {
                /* Parent process */

		////////DOES THIS HAVE TO BE HERE?/////////////    
                //signal (SIGCHLD, handler_sigchld);
                //signal (SIGTTIN, handler_sigttin);
                //signal (SIGTTOU, handler_sigttou);

                if (!P->background) {
                    set_fg_pgid (pid[0]);
                }

                if (P->ntasks > 1 && t != P->ntasks - 1) {
                    /* Close the write side of the process and store the read side
                     * if there are more tasks */
                    close (fd[WRITE_SIDE]);
                    previous_pipe = fd[READ_SIDE];
                }
                else if (P->ntasks > 1 && t == P->ntasks - 1) {
                    /* Close all connections to the pipes if it is the last task */
                    close (fd[WRITE_SIDE]);
                    close (fd[READ_SIDE]);
                    close (previous_pipe);
                }
            }
            else {
                /* Child process */

                /* Check for infile before first task */
                if (t == 0) {
                    if (P->infile) {
                        file_redirect (P->infile, 0);
                    }
                }
                /* Case where there is only one task */
                if (P->ntasks == 1) {
                    if (P->outfile) {
                        file_redirect (P->outfile, 1);
                    }
                }
                /* Case where there are multiple tasks */
                else if (P->ntasks > 1) {
                    /* For first task, read from STDIN or infile and write to pipe */
                    if (t == 0) {
                        close (fd[READ_SIDE]);
                        if (dup2 (fd[WRITE_SIDE], STDOUT_FILENO) == -1) {
                            fprintf (stderr, "error -- dup2() failed for WRITE_SIDE -> STDOUT\n");
                            exit (EXIT_FAILURE);
                        }
                        close (fd[WRITE_SIDE]);
                    /* For tasks in between, read from previous pipe and write to new pipe */
                    }
                    else if (t != 0 && t != P->ntasks - 1) {
                        close (fd[READ_SIDE]);
                        if (dup2 (previous_pipe, STDIN_FILENO) == -1) {
                            fprintf (stderr, "error -- dup2() failed for READ_SIDE -> STDIN\n");
                            exit (EXIT_FAILURE);
                        }
                        close (previous_pipe);
                        if (dup2 (fd[WRITE_SIDE], STDOUT_FILENO) == -1) {
                            fprintf (stderr, "error -- dup2() failed for WRITE_SIDE -> STDOUT\n");
                            exit (EXIT_FAILURE);
                        }
                        close (fd[WRITE_SIDE]);
                    /* For last task, read from previous pipe and write to STDOUT or outfile */
                    }
                    else if (t == P->ntasks - 1) {
                        if (P->outfile) {
                            file_redirect (P->outfile, 1);
                        }
                        close (fd[READ_SIDE]);
                        close (fd[WRITE_SIDE]);
                        if (dup2 (previous_pipe, STDIN_FILENO) == -1) {
                            fprintf (stderr, "error -- dup2() failed for READ_SIDE -> STDIN\n");
                            exit (EXIT_FAILURE);
                        }
                        close (previous_pipe);
                    }
                }
                execvp (P->tasks[t].cmd, P->tasks[t].argv);

                printf ("pssh: found but can't exec: %s\n", P->tasks[t].cmd);
                exit (EXIT_FAILURE);
	    }
        }
        else {
            printf ("pssh: command not found: %s\n", P->tasks[t].cmd);
            current_jobs[lowest_job_num].name = NULL;
            cmd_found = 0;
        }
    }

    if (!builtin_flag && cmd_found) {////PUT EVERYTHING BELOW THIS IN A FUNCTION////
        int id;
        current_jobs[lowest_job_num].npids = P->ntasks;
        current_jobs[lowest_job_num].rem_pids = P->ntasks;
        current_jobs[lowest_job_num].pgid = pid[0];
	current_jobs[lowest_job_num].bg_job_done = 0;
	current_jobs[lowest_job_num].bg_job_stopped = 0;
        current_jobs[lowest_job_num].job_mem_freed = 0;
        current_jobs[lowest_job_num].pids = malloc (sizeof(pid_t) * (P->ntasks));
	for (id = 0; id < P->ntasks; id++) {
            current_jobs[lowest_job_num].pids[id] = pid[id];
        }
        if (P->background) {
            current_jobs[lowest_job_num].status = BG;
            printf ("[%d]", lowest_job_num);
            for (id = 0; id < P->ntasks; id++) {
                printf (" %ld", (long int)current_jobs[lowest_job_num].pids[id]);
            }
            printf ("\n");
        }
        else {
            current_jobs[lowest_job_num].status = FG;
        }

	while (current_jobs[lowest_job_num].pgid && 
              (current_jobs[lowest_job_num].status == STOPPED ||
               current_jobs[lowest_job_num].status == FG      ||
               current_jobs[lowest_job_num].status == BG)) {
            lowest_job_num++;
        }
        highest_job_num = lowest_job_num;
    }
}


int main (int argc, char** argv)
{
    char* cmdline;
    Parse* P;

    size_t i, cmd_len;
    int j;

    print_banner ();

    signal (SIGCHLD, handler_sigchld);
    signal (SIGTTIN, handler_sigttin);
    signal (SIGTTOU, handler_sigttou);

    while (1) {
        char *cdir = malloc (sizeof(char) * BUFF_SIZE);
        cmdline = readline (build_prompt (cdir));
	free (cdir);
        if (!cmdline)       /* EOF (ex: ctrl-d) */
            exit (EXIT_SUCCESS);

        cmd_len = strlen(cmdline);
        char* job_name = malloc (sizeof(char) * (cmd_len + 1));
        for (i = 0; i < cmd_len; i++) {
            job_name[i] = cmdline[i];
        }
        job_name[i] = '\0';

        for (j = 0; j < highest_job_num; j++) {
            if (current_jobs[j].bg_job_stopped) {
                printf ("[%d] + suspended    %s\n", j, current_jobs[j].name);
                current_jobs[j].bg_job_stopped = 0;
            }
            if (!current_jobs[j].rem_pids) {
                if (current_jobs[j].bg_job_done) {
                    printf ("[%d] + done    %s\n", j, current_jobs[j].name);
		    current_jobs[j].bg_job_done = 0;
                }
                if (!current_jobs[j].job_mem_freed) {
                   free (current_jobs[j].name);
                   free (current_jobs[j].pids);
                   current_jobs[j].job_mem_freed = 1;
	        }
            }
        }

	lowest_job_num = find_lowest_job_num ();

        P = parse_cmdline (cmdline);

        if (P && !is_builtin (P->tasks->cmd)) {
            current_jobs[lowest_job_num].name = strdup (job_name);
        }
        free (job_name);

        if (!P) {
            goto next;
        }

        if (P->invalid_syntax) {
            printf ("pssh: invalid syntax\n");
            goto next;
        }

#if DEBUG_PARSE
        parse_debug (P);
#endif
        execute_tasks (P);

    next:
        parse_destroy (&P);
        free(cmdline);
    }
}
