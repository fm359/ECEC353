/* Author: Farhan Muhammad
 * Date:   02/22/2019
 *
 * This program simulates the linux kill command. It takes process IDs as user input and sends
 * a signal to them as specified by the user. The user has the option to select which signal is
 * sent as an input argument along with the -s flag. The -l displays all the available signals.
 * And the program sends the SIGTERM signal by default, when no flag is specified.
 */

#include <signal.h>    /* signal(), SIGTERM                */
#include <stdlib.h>    /* EXIT_FAILURE, atoi(), abs()      */
#include <stdio.h>     /* stderr, printf(), fprintf()      */
#include <string.h>    /* strcmp()                         */
#include <errno.h>     /* errno 			   */

extern int errno;

int main(int argc, const char** argv)
{
	/* Display usage instructions if no arguments are provided */
	if(argc == 1) {
		printf("\nUsage: ./fm359_signal [options] <pid>\n\n");
		printf("Options:\n");
		printf("    -s <signal> Sends <signal> to <pid>\n");
		printf("    -l Lists all signal numbers with their names\n\n");
		return 0;
	}

/*---------------------Check input argument(s) validity----------------------------*/
	int i, j;
	/* No PIDs entered with -s flag */
	if(!strcmp(argv[1], "-s")) {
		if(argc == 2) {
			fprintf(stderr, "Error: No PIDs specified\n");
			exit(EXIT_FAILURE);
		}
		i = 2;
	/* PIDs entered with -l flag */
	} else if(!strcmp(argv[1], "-l")) {
		if(argc > 2) {
                        fprintf(stderr, "Error: Too many arguments\n");
                        exit(EXIT_FAILURE);
                }
	/* No flags specified */
	} else if(abs(strcmp(argv[1], "-s")) && abs(strcmp(argv[1], "-l"))) {
		i = 1;
	}
	/* Check if PIDs entered contain only numbers */
	for(i; i < argc; i++) {
		for(j = 0; argv[i][j] != '\0'; j++) {
			if(isdigit(argv[i][j]) == 0) {
				fprintf(stderr, "Error: Invalid input argument.\n");
				exit(EXIT_FAILURE);
			}
		}
	}
 /*--------------------------------------------------------------------------------*/
	/* If -s flag is specified*/
	if(!strcmp(argv[1], "-s")) {
		int signal = atoi(argv[2]); /* Extract the signal number */
		/* Signal 0 specified */
		if(signal == 0) {
			/* Check if PIDs exist and can receive signals using errno */
			for(j = 3; j < argc; j++) {
				kill((pid_t)atoi(argv[j]), signal);
				if(errno == 0) {
					printf("PID %d exists and is able to receive signals\n", atoi(argv[j]));
				} else if(errno == 1) {
					printf("PID %d exists, but we can't send it signals\n", atoi(argv[j]));
				} else if(errno == 3) {
					printf("PID %d does not exist\n", atoi(argv[j]));
				}
            }
		/* Signal 1 - 31*/
		} else if(signal >= 1 && signal <= 31) {
			for(j = 3; j < argc; j++) {
				kill((pid_t)atoi(argv[j]), signal);
			}
		/* Return error for any other number or input given */
		} else {
			fprintf(stderr, "Error: Invalid signal entered.\n");
			exit(EXIT_FAILURE);
		}
	/* If -l flag is specified*/
	} else if(!strcmp(argv[1], "-l")) {
		printf("1) SIGHUP       2) SIGINT       3) SIGQUIT      4) SIGILL\n");
		printf("5) SIGTRAP      6) SIGABRT      7) SIGBUS       8) SIGFPE\n");
		printf("9) SIGKILL     10) SIGUSR1     11) SIGSEGV     12) SIGUSR2\n");
		printf("13) SIGPIPE     14) SIGALRM     15) SIGTERM     16) SIGSTKFLT\n");
		printf("17) SIGCHLD     18) SIGCONT     19) SIGSTOP     20) SIGTSTP\n");
		printf("21) SIGTTIN     22) SIGTTOU     23) SIGURG      24) SIGXCPU\n");
		printf("25) SIGXFSZ     26) SIGVTALRM   27) SIGPROF     28) SIGWINCH\n");
		printf("29) SIGIO       30) SIGPWR      31) SIGSYS\n");
	/* If no flag is specified*/
	} else {
		for(j = 1; j < argc; j++) {
			kill((pid_t)atoi(argv[j]), 15);
        }
	}

	return 0;
}
