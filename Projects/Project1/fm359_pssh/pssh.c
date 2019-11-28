#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <readline/readline.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

#include "builtin.h"
#include "parse.h"

/*******************************************
 * Set to 1 to view the command line parse *
 *******************************************/
#define DEBUG_PARSE 0

#define READ_SIDE 0
#define WRITE_SIDE 1

void print_banner ()
{
    printf ("                    ________   \n");
    printf ("_________________________  /_  \n");
    printf ("___  __ \\_  ___/_  ___/_  __ \\ \n");
    printf ("__  /_/ /(__  )_(__  )_  / / / \n");
    printf ("_  .___//____/ /____/ /_/ /_/  \n");
    printf ("/_/ Type 'exit' or ctrl+c to quit\n\n");
}


/* returns a string for building the prompt
 *
 * Note:
 *   If you modify this function to return a string on the heap,
 *   be sure to free() it later when appropirate!  */
static char* build_prompt ()
{
     char cwd[1024];
     if (getcwd(cwd, sizeof(cwd)) != NULL)
         fprintf(stdout, "%s", cwd);
     else
         perror("Error printing cwd");
    return  "$ ";
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
void file_redirect(char* file, int redirect_side)
{
    int fd;
    if (redirect_side == 0)
    {
        if((fd = open(file, O_RDONLY)) < 0)
	{
            fprintf(stderr, "Error: Failed to open input file\n");
            exit(EXIT_FAILURE);
        }   
        if(dup2(fd, STDIN_FILENO) == -1)
	{
            fprintf(stderr, "Error: Failed to read input file. Check if file exists\n");
            exit(EXIT_FAILURE);
        } 
    }	
    else if (redirect_side == 1)
    {
        if((fd = open(file, O_CREAT | O_TRUNC | O_WRONLY, 0644)) < 0)
	{
            fprintf(stderr, "Error: Failed to open output file\n");
            exit(0);
        }
        if(dup2(fd, STDOUT_FILENO) == -1)
	{
            fprintf(stderr, "Error: Failed to redirect stdout to output file\n");
            exit(0);
        }
    }	
    close(fd);
}

/* Called upon receiving a successful parse.
 * This function is responsible for cycling through the
 * tasks, and forking, executing, etc as necessary to get
 * the job done! */
void execute_tasks (Parse* P)
{
    unsigned int t;
    int fd[2];
    pid_t pid;

    /* Used to store the address of the read side of the previous tasks pipe */
    int previous_pipe = 0;

    for (t = 0; t < P->ntasks; t++)
    {
	/* This condition creates a pipe for every iteration
	 * only when there are more than one tasks */
	if (P->ntasks > 1)
	{
	    if(pipe(fd) == -1)
	    {
		fprintf(stderr, "error -- failed to create pipe\n");
		exit(EXIT_FAILURE);
	    }
	}

        if(is_builtin (P->tasks[t].cmd))
	{
            builtin_execute (P->tasks[t]);
        }
        else if(command_found (P->tasks[t].cmd))
	{
	    pid = vfork();

	    if(pid < 0)
	    {
                fprintf(stderr, "error -> failed to vfork()\n");
                exit(EXIT_FAILURE);
	    }

	    if(pid > 0)
	    {
		/* Parent process */
                int child_ret;

                waitpid(pid, &child_ret, 0);

		if(P->ntasks > 1 && t != P->ntasks - 1)
		{
		    /* Close the write side of the process and store the read side
		     * if there are more tasks */
		    close(fd[WRITE_SIDE]);
		    previous_pipe = fd[READ_SIDE];
		}
		else if(P->ntasks > 1 && t == P->ntasks - 1)
		{
		    /* Close all connections to the pipes if it is the last task */
		    close(fd[WRITE_SIDE]);
		    close(fd[READ_SIDE]);
		    close(previous_pipe);
		}
	    }
	    else
	    {
		/* Child process */

		/* Case where there is a single command...
		 * Perform file redirection using file_redirect() as appropriate */
		if(t == 0 && P->ntasks == 1)
		{
	            if(P->infile == NULL && P->outfile == NULL)
	            {
                        execvp(P->tasks[t].cmd, P->tasks[t].argv);
                    }
                    else if(P->infile != NULL && P->outfile == NULL)
	            {    
                        file_redirect(P->infile, 0);
                        execvp(P->tasks[t].cmd, P->tasks[t].argv);
                    }
                    else if(P->outfile != NULL && P->infile == NULL)
	            {
                        file_redirect(P->outfile, 1);
                        execvp(P->tasks[t].cmd, P->tasks[t].argv);
                    }
                    else if(P->outfile != NULL && P->infile != NULL)
	            {
                        file_redirect(P->outfile, 1);
                        file_redirect(P->infile, 0);
                        execvp(P->tasks[t].cmd, P->tasks[t].argv);
                    }		
				
	            printf ("pssh: found but can't exec: %s\n", P->tasks[t].cmd);
	            exit(EXIT_FAILURE);
	        }
		/* Case where it is the first command amongst multiple commands...
		 * -redirect input from a file if specified
		 * -redirect output to the pipe
		 * -close all connections to the pipe
		 * -execute task */
		else if(t == 0 && P->ntasks > 1)
		{
		    if(P->infile != NULL)
		    {
			file_redirect(P->infile, 0);
		    }

		    close(fd[READ_SIDE]);

		    if (dup2(fd[WRITE_SIDE], STDOUT_FILENO) == -1)
		    {
	                fprintf(stderr, "error -- dup2() failed for WRITE_SIDE -> STDOUT\n");
		        exit(EXIT_FAILURE);
		    }
		    close(fd[WRITE_SIDE]);

		    execvp(P->tasks[t].cmd, P->tasks[t].argv);
 
		    printf ("pssh: found but can't exec: %s\n", P->tasks[t].cmd);
                    exit(EXIT_FAILURE);
		}
		/* Case where it is the last command amongst multiple commands...
		 * -redirect input from the previous pipe
		 * -close all connections to the current and previous pipes
		 * -execute task and output to stdout */
		else if(t != 0 && t == P->ntasks - 1)
		{
		    if(P->outfile != NULL)
	            {
		        file_redirect(P->outfile, 1);
		    }

		    close(fd[READ_SIDE]);
		    close(fd[WRITE_SIDE]);

		    if(dup2(previous_pipe, STDIN_FILENO) == -1)
		    {
			fprintf(stderr, "error -- dup2() failed for READ_SIDE -> STDIN\n");
			exit(EXIT_FAILURE);
		    }
		    close(previous_pipe);

		    execvp(P->tasks[t].cmd, P->tasks[t].argv);

		    printf ("pssh: found but can't exec: %s\n", P->tasks[t].cmd);
		    exit(EXIT_FAILURE);
		}
		/* Case where it is not the first or last command amongst multiple commands...
		 * -redirect input from the previous pipe
		 * -redirect output to the current pipe
		 * -close all connections to both pipes
		 * -execute task */	
		else
		{
		    close(fd[READ_SIDE]);

		    if(dup2(previous_pipe, STDIN_FILENO) == -1)
		    {
			fprintf(stderr, "error -- dup2() failed for READ_SIDE -> STDIN\n");
			exit(EXIT_FAILURE);
		    }

		    close(previous_pipe);

		    if (dup2(fd[WRITE_SIDE], STDOUT_FILENO) == -1)
	            {
			fprintf(stderr, "error -- dup2() failed for WRITE_SIDE -> STDOUT\n");
			exit(EXIT_FAILURE);
		    }
		    close(fd[WRITE_SIDE]);

		    execvp(P->tasks[t].cmd, P->tasks[t].argv);

		    printf ("pssh: found but can't exec: %s\n", P->tasks[t].cmd);
		    exit(EXIT_FAILURE);
		}
	    }
	}
	else
        {
            printf ("pssh: command not found: %s\n", P->tasks[t].cmd);
            break;
        }
    }
}


int main (int argc, char** argv)
{
    char* cmdline;
    Parse* P;

    print_banner ();

    while (1) {
        cmdline = readline (build_prompt());
        if (!cmdline)       /* EOF (ex: ctrl-d) */
            exit (EXIT_SUCCESS);

        P = parse_cmdline (cmdline);
        if (!P)
            goto next;

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
