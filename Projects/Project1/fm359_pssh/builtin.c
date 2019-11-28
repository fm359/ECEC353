#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "builtin.h"
#include "parse.h"

static char* builtin[] = {
    "exit",   /* exits the shell */
    "which",  /* displays full path to command */
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


void builtin_execute (Task T)
{
    if(!strcmp (T.cmd, "exit"))
    {
        exit (EXIT_SUCCESS);
    }
    else if(!strcmp (T.cmd, "which"))
    {
        /* Check if any arguments are provided */
        if(T.argv[1] != NULL)
        {
            if(!strcmp(T.argv[1], "which"))
            {
                printf("which: shell built-in command\n");
            }
            else if(!strcmp (T.argv[1], "exit"))
            {
                printf("exit: shell built-in command\n");
            }
            else
            {
       	        pid_t pid;

                pid = vfork();

	        if(pid < 0)
                {
                    fprintf(stderr, "error -> failed to vfork()");
                    exit(EXIT_FAILURE);
                }

                if(pid > 0)
                {
                    /* Parent process */

                    int child_ret;

                    waitpid(pid, &child_ret, 0);
                }
                else
                {
                    /* Child process */
                    execvp(T.cmd, T.argv);

                    printf("child -> error: failed to execlp 'ls'\n");
                    exit(EXIT_FAILURE);
                }
            }
        }
    }
    else
    {
        printf ("pssh: builtin command: %s (not implemented!)\n", T.cmd);
    }
}
