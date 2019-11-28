# Author: Farhan Muhammad
# Date:   01/17/2019

# This program demonstrates how to fork a parent process and overwrite the child process
# with an "ls -l -h" command using the execvp() function

#include <sys/types.h> /* pid_t                              */
#include <sys/wait.h>  /* waitpid(), WEXITSTATUS()           */
#include <unistd.h>    /* vfork()                            */
#include <stdlib.h>    /* exit(), EXIT_SUCCESS, EXIT_FAILURE */
#include <stdio.h>     /* printf(), fprintf(), stderr        */

int main(int argc, char** argv)
{
        pid_t pid;
		char *const paramArray[] = {"ls", "-l", "-h", NULL};

		pid = vfork();

        if (pid < 0) {
                fprintf(stderr, "error -> failed to vfork()");
                exit(EXIT_FAILURE);
        }

        if (pid > 0) {
                int child_ret;

                printf("parent -> pid: %i\n", getpid());
                waitpid(pid, &child_ret, 0);
                printf("parent -> child exited with code %i\n", WEXITSTATUS(child_ret));

                exit(EXIT_SUCCESS);
        } else {
                printf("child -> pid: %i\n", getpid());
                execvp("ls", paramArray);

                printf("child -> error: failed to execvp 'ls'\n");
				exit(EXIT_FAILURE);
		}
}