/* Author: Farhan Muhammad
 * Date:   03/08/2019
 *
 * This program computes the sum of the squares of the first 20 integers from 0 to 19
 * using multithreading and compares the result with a single threaded loop to calculate
 * the same summation. The program makes use of a control variable to ensure that the 
 * main loop does not terminate before all of the threads have finished executing.
 */

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#define NUM_WORKER_THREADS 20

struct thread_data {
    pthread_t tid;
    unsigned int num;
};

struct workers_state {
    int still_working;
    pthread_mutex_t mutex;
    pthread_cond_t signal;
};

static struct workers_state wstate = {
    .still_working = NUM_WORKER_THREADS,
    .mutex = PTHREAD_MUTEX_INITIALIZER,
    .signal = PTHREAD_COND_INITIALIZER
};

static unsigned int result[NUM_WORKER_THREADS];

void* worker_thread (void* param)
{
    unsigned int thread_num = ((struct thread_data*)param)->num;
   
    pthread_mutex_lock(&wstate.mutex);
    result[thread_num] = thread_num * thread_num;
    pthread_mutex_unlock(&wstate.mutex);

    wstate.still_working--;

    pthread_cond_signal(&wstate.signal);
}

int main (int argc, char** argv)
{
    unsigned int i, C = 0, total = 0;

    struct thread_data* threads = malloc (sizeof (struct thread_data) * NUM_WORKER_THREADS);

    /* Create 20 threads */
    for (i = 0; i < NUM_WORKER_THREADS; i++) {
        threads[i].num = i;
        C += i * i;
        pthread_create (&threads[i].tid, NULL, worker_thread, (void *)&threads[i]);
        pthread_detach (threads[i].tid);
    }

    /* Block using condition variable while there are still working threads */
    pthread_mutex_lock (&wstate.mutex);
    while (wstate.still_working) {
        pthread_cond_wait (&wstate.signal, &wstate.mutex);
    }
    pthread_mutex_unlock (&wstate.mutex);

    free (threads);

    /* Summation of squares */
    for (i = 0; i < NUM_WORKER_THREADS; i++) {
        total += result[i];
    }

    printf ("Sum of squares using multiple threads (total): %u\n", total);
    printf ("Sum of squares using single thread (C):        %u\n", C);

    return 0;
}
