/* Author: Farhan Muhammad
 * Date:   01/23/2019
 *
 * This program uses two pipes to demonstrate bidirectional communication between a parent process and a
 * child process. The parent process accepts characters from stdin 1 byte at a type and pipes the data
 * to the child process. The child process then converts the characters to upper case and pipes the data
 * back to the parent process, which then prints the uppercase text to stdout.
 */
 
#include <sys/wait.h>  /* wait()                           */
#include <unistd.h>    /* fork(), pipe(), write(), close() */
#include <stdlib.h>    /* EXIT_FAILURE, EXIT_SUCCESS       */
#include <stdio.h>     /* stderr, printf(), fprintf()      */
#include <string.h>    /* strlen()                         */

#define BUFFER_SIZE 1
#define READ_SIDE 0
#define WRITE_SIDE 1

int main(void)
{
        char parent_write_data[BUFFER_SIZE], child_write_data[BUFFER_SIZE];
        char parent_read_data[BUFFER_SIZE], child_read_data[BUFFER_SIZE];
        int parent_read_ret, child_read_ret;
        int fd1[2], fd2[2];

        pid_t pid;

        /* create the pipes */
        if (pipe(fd1) == -1 || pipe(fd2) == -1) {
                fprintf(stderr, "error -- failed to create pipe");
                return EXIT_FAILURE;
        }

        pid = fork();
        if (pid < 0) {
                fprintf(stderr, "error -- failed to fork()");
                return EXIT_FAILURE;
        }

        if (pid > 0) { /* parent process */
                int child_ret;
				
				close(fd1[READ_SIDE]);
                close(fd2[WRITE_SIDE]);

                parent_read_ret = read(STDIN_FILENO, parent_read_data, BUFFER_SIZE);
                while (parent_read_ret != 0) {
                        write(fd1[WRITE_SIDE], parent_read_data, BUFFER_SIZE);
                        parent_read_ret = read(fd2[READ_SIDE], parent_write_data, BUFFER_SIZE);
                        printf("%c", parent_write_data[0]);
                        parent_read_ret = read(STDIN_FILENO, parent_read_data, BUFFER_SIZE);
                }
                close(fd1[WRITE_SIDE]);
                close(fd2[READ_SIDE]);

                wait(&child_ret);
                printf("\nparent -- child exited with code %i\n", child_ret);

        } else {       /* child process */
                close(fd1[WRITE_SIDE]);
                close(fd2[READ_SIDE]);

                child_read_ret = read(fd1[READ_SIDE], child_read_data, BUFFER_SIZE);
                while (child_read_ret != 0) {
                        child_write_data[0] = toupper(child_read_data[0]);
                        write(fd2[WRITE_SIDE], child_write_data, BUFFER_SIZE);
                        child_read_ret = read(fd1[READ_SIDE], child_read_data, BUFFER_SIZE);
                }
                close(fd1[READ_SIDE]);
                close(fd2[WRITE_SIDE]);
		}
		return EXIT_SUCCESS;
}
