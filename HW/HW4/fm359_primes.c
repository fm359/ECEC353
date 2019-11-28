/* Author: Farhan Muhammad
 * Date:   03/01/2019
 *
 * This program computes the first 10,000,000 prime numbers and stores them in
 * an array. Using SIGALRM the program prints the last five prime numbers found
 * and displays them to the terminal as well as the number of prime numbers
 * computed at the time. It also accepts SIGUSR1 to display the last 5 primes
 * upon receiving the signal at any time from the user. Finally, it accepts
 * SIGTERM and displays the current status before exiting the program.
 */

#include <signal.h>  /* signal(), SIGINT     */
#include <stdio.h>   /* printf()             */
#include <stdlib.h>  /* exit(), EXIT_SUCCESS */
#include <unistd.h>  /* alarm()		     */
#include <stdbool.h> /* true, false 	     */

#define FALSE 0
#define TRUE !FALSE
#define MAXNUM 10000000

/* Global variables declared and initialized */
unsigned int num_found = 0;
unsigned int primes[MAXNUM];
volatile sig_atomic_t sigalrm_flag = false;
volatile sig_atomic_t sigusr1_flag = false;
volatile sig_atomic_t sigterm_flag = false;
int i = 1;

/* Custom handler for the three signals accepted in the program */
void handler(int sig)
{
	if (sig == SIGALRM) {
		sigalrm_flag = true;
	} else if (sig == SIGUSR1) {
		sigusr1_flag = true;
	} else if (sig == SIGTERM) {
		sigterm_flag = true;
	}
}

/* Function to check if a number is prime or not */
int is_prime(unsigned int num)
{
	int c;
	 
	for (c = 2 ; c <= num - 1 ; c++) { 
		if (num % c == 0)
			return FALSE;
	}
	
  	return TRUE;
}

/* Function to print the last n computed prime numbers */
void print_primes(int num)
{
	printf("Found %d primes.\n", num_found);
	printf("Last %d primes found:\n   ", num);
	for (i; i <= num; i++) {
		printf(" %d", primes[num_found-i]);
	}
	printf("\n");
	i = 1;
}

int main (int argc, char* argv[])
{
	unsigned int current_num = 2;

/********************Setting signal masks********************/
	sigset_t block_quit;
	sigset_t block_all;

	sigemptyset(&block_quit);
	sigemptyset(&block_all);

	sigaddset(&block_quit, SIGQUIT);
	sigaddset(&block_all, SIGALRM);
	sigaddset(&block_all, SIGUSR1);
	sigaddset(&block_all, SIGTERM);

	sigprocmask(SIG_BLOCK, &block_quit, NULL);
	
/*******************Setting custom handler*******************/
	void (*old_sigalrm_handler)(int sig);
	void (*old_sigusr1_handler)(int sig);
	void (*old_sigterm_handler)(int sig);

	old_sigalrm_handler = signal(SIGALRM, handler);
	old_sigusr1_handler = signal(SIGUSR1, handler);
	old_sigterm_handler = signal(SIGTERM, handler);
	
/************************************************************/
	
	alarm(10);

	for (current_num; num_found < MAXNUM; current_num++) {
	
		/* Mask signals while computing prime numbers */
		sigprocmask(SIG_BLOCK, &block_all, NULL);
		if (is_prime(current_num)) {
			primes[num_found] = current_num;
			num_found++;
		}
		sigprocmask(SIG_UNBLOCK, &block_all, NULL);

		if (sigterm_flag) {
			print_primes(5);
			printf("Goodbye!\n");
			exit(EXIT_SUCCESS);
		}

		if (sigalrm_flag) {
			print_primes(5);
			sigalrm_flag = false;
			alarm(10);
		} else if (sigusr1_flag) {
			print_primes(5);
			sigusr1_flag = false;
		}
	}

	print_primes(5);

	signal(SIGALRM, old_sigalrm_handler);
	signal(SIGUSR1, old_sigusr1_handler);
	signal(SIGTERM, old_sigterm_handler);

	return 0;

}
