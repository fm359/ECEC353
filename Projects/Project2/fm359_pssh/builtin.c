#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <readline/readline.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>

#include "builtin.h"
#include "parse.h"

extern int errno;

static char* builtin[] = {
    "exit",   /* exits the shell */
    "which",  /* displays full path to command */
    "fg",
    "bg",
    "kill",
    "jobs",
    NULL
};

int is_builtin (char* cmd)
{
    int i;

    for (i=0; builtin[i]; i++) {
        if (!strcmp (cmd, builtin[i]))
            return 1;
    }

    return 0;
}

void set_fg_pgid (pid_t pgid)
{
    void (*old)(int);

    old = signal (SIGTTOU, SIG_IGN);
    tcsetpgrp (STDIN_FILENO, pgid);
    tcsetpgrp (STDOUT_FILENO, pgid);
    signal (SIGTTOU, old);
}

void output_redirect (char* outfile)
{
    if (outfile) {
        int fd;
        if ((fd = open (outfile, O_CREAT | O_TRUNC | O_WRONLY, 0644)) < 0) {
            fprintf (stderr, "Error: Failed to open output file\n");
            exit (EXIT_FAILURE);
        }
        if (dup2 (fd, STDOUT_FILENO) == -1) {
            fprintf (stderr, "Error: Failed to redirect stdout to output file\n");
            exit (EXIT_FAILURE);
        }
        close (fd);
    }
}

int job_exists (Job* current_jobs, int i)
{
    if (current_jobs[i].pgid &&
       (current_jobs[i].status == STOPPED ||
        current_jobs[i].status == FG      ||
        current_jobs[i].status == BG)) {
        return 1;
    }
    return 0;
}

void fg_bg (Task T, Job* current_jobs, int fg_or_bg)
{
    int i, valid = 1, job_num = 0;
    pid_t pgrp_id = 0;
    /* Display usage instructions if no arguments are provided */
    if (!T.argv[1]) {
        if (fg_or_bg == 0) {
            printf ("\nUsage: fg %%<job number>\n\n");
        }
        else if (fg_or_bg == 1) {
            printf ("\nUsage: bg %%<job number>\n\n");
        }
    }
    else if (T.argv[2]) {
        fprintf (stderr, "pssh: too many arguments\n");
    }
    else if (T.argv[1][0] != '%') {
        fprintf (stderr, "pssh: invalid syntax\n");
    }
    else {
        for (i = 1; T.argv[1][i] != '\0'; i++) {
            if (isdigit (T.argv[1][i]) == 0) {
                valid = 0;
                fprintf (stderr, "pssh: invalid job number: [%s]\n", T.argv[1]++);
            }
        }

        if (valid) {
            T.argv[1][0] = '0';
            job_num = atoi (T.argv[1]);
            if (job_exists (current_jobs, job_num)) {
                pgrp_id = current_jobs[job_num].pgid;
                if (fg_or_bg == 0) {
                    current_jobs[job_num].status = FG;
                    printf ("%s\n", current_jobs[job_num].name);
                    set_fg_pgid (pgrp_id);
                }
                else if (fg_or_bg == 1) {
                    current_jobs[job_num].status = BG;
                    //printf ("[%d] + continued    %s\n", job_num, current_jobs[job_num].name);
                }
                kill (pgrp_id * (-1), 18);
            }
            else {
                fprintf (stderr, "pssh: invalid job number: [%d]\n", job_num);
            }
        }
    }
}

void disp_jobs (Job* current_jobs)
{
    int i;
    for (i = 0; i < 100; i++) {
        char* job_status;
        if (current_jobs[i].pgid && current_jobs[i].status != TERM) {
            if (current_jobs[i].status == FG || current_jobs[i].status == BG) {
                job_status = "running";
            }
            else {
                job_status = "stopped";
            }
            printf ("[%d] + %s    %s\n", i, job_status, current_jobs[i].name);
        }
    }
}

void kill_cmd (Task T, Job* current_jobs)
{
    int continue_flag, is_job = 0, job_num = 0;
    pid_t id = 0;
    if (!T.argv[1]) { /* Display usage instructions if no arguments are provided */
        printf ("\nUsage: kill [-s <signal>] <pid> | %%<job> ...\n\n");
    }
    else {
        int i, j, start_char;
        if (!strcmp (T.argv[1], "-s")) { /* No PIDs entered with -s flag */
            if (!T.argv[2]) {
                fprintf (stderr, "Error: No PIDs specified\n");
            }
            else {//////////////
                for (j = 0; T.argv[2][j] != '\0'; j++) {
                    if (isdigit (T.argv[2][j]) == 0) {
                        fprintf (stderr, "Error: Invalid signal entered.\n");
                    }
                }
            }
            i = 3;
        }
        else if (abs (strcmp (T.argv[1], "-s"))) { /* No flags specified */
            i = 1;
        }

        while (T.argv[i]) {
            continue_flag = 0;
            if (T.argv[i][0] == '%') {
                is_job = 1;
                start_char = 1;
            }
            else {
                is_job = 0;
                start_char = 0;
            }
            for (j = start_char; T.argv[i][j] != '\0'; j++) {
                if (isdigit (T.argv[i][j]) == 0) {
                    if (is_job) {
                        fprintf (stderr, "pssh: invalid job number: [%s]\n", T.argv[i]++);
                    }
                    else {
                        fprintf (stderr, "pssh: invalid pid: [%s]\n", T.argv[i]);
                        continue_flag = 1;
                    }
                }
            }

            if (continue_flag) {
                i++;
                continue;
            }

            if (is_job) {
                T.argv[i][0] = '0';
                job_num = atoi (T.argv[i]);
                if (!job_exists (current_jobs, job_num)) {
                    fprintf (stderr, "pssh: invalid job number: [%d]\n", job_num);
                    continue_flag = 1;
                }
		else {
                    id = current_jobs[job_num].pgid * (-1);
                }
            }
            else {
                id = (pid_t)atoi (T.argv[i]);
                kill (id, 0);
                if (errno == 3) {
                    fprintf (stderr, "pssh: invalid pid: [%s]\n", T.argv[i]);
                    continue_flag = 1;
                }
            }

            if (continue_flag) {
                i++;
                continue;
            }

            if (!strcmp (T.argv[1], "-s")) { /* If -s flag is specified*/
                int signal = atoi (T.argv[2]);
                if (signal == 0) { /* Signal 0 specified */
                    errno = 0;
		    kill (id, signal);
                    if (is_job) {
                        if (errno == 0) {
                            printf ("PGID %d exists ", id * (-1));
                            printf ("and is able to receive signals\n");
                        }
                        else if (errno == 1) {
                            printf ("PGID %d exists", id * (-1));
                            printf (", but we can't send it signals\n");
                        }
                    }
                    else {
                        if (errno == 0) {
                            printf ("PID %d exists ", id);
                            printf ("and is able to receive signals\n");
                        }
                        else if (errno == 1) {
                            printf ("PID %d exists", id);
                            printf (", but we can't send it signals\n");
                        }
                    }
                }
                else if (signal >= 1 && signal <= 31) { /* Signal 1 - 31*/
                    kill (id, signal);
                }
            }
            else { /* If no flag is specified*/
                kill (id, 15);
            }
            i++;
        }
    }
}

void builtin_which (Task T, char* outfile)
{
    if (!strcmp (T.cmd, "which")) {
        /* Check if any arguments are provided */
        if (T.argv[1]) {
            pid_t pid;
            pid = fork ();

            if (pid < 0) {
                fprintf (stderr, "error -> failed to fork()");
                exit (EXIT_FAILURE);
            }

            if (pid > 0) {
                /* Parent process */
                int child_ret;
                waitpid (pid, &child_ret, 0);
            }
            else {
                /* Child process */
                output_redirect (outfile);
                if (!strcmp (T.argv[1], "which")) {
                    execlp ("echo", "echo", "which: shell built-in command", NULL);
                }
                else if (!strcmp (T.argv[1], "exit")) {
                    execlp ("echo", "echo", "exit: shell built-in command", NULL);
                }
                else if (!strcmp (T.argv[1], "kill")) {
                    execlp ("echo", "echo", "kill: shell built-in command", NULL);
                }
                else if (!strcmp (T.argv[1], "jobs")) {
                    execlp ("echo", "echo", "jobs: shell built-in command", NULL);
                }
                else if (!strcmp (T.argv[1], "fg")) {
                    execlp ("echo", "echo", "fg: shell built-in command", NULL);
                }
                else if (!strcmp (T.argv[1], "bg")) {
                    execlp ("echo", "echo", "bg: shell built-in command", NULL);
                }
                else {
                    execvp (T.cmd, T.argv);
                }

                printf ("child -> error: failed to exec 'which'\n");
                exit (EXIT_FAILURE);
            }
        }
    }
}
