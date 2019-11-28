/******************************************************************************
 * minigrep - search a directory for files containing a given string
 *            and print the line numbers and filenames where found.
 *
 *  Authors: James A. Shackleford, Farhan Muhammad
 *  Updated: 03/17/2019
 *
 * Compile with:
 *   $ gcc -o minigrep minigrep.c -pthread
 *
 * NOTE: the search algorithm is generally slower with the multithreaded
 *       option due to the serial nature of the process of checking lines.
 *       Each directory and/or file is linearly enqueued and dequeued one
 *       after the other, so utilizing multiple threads does not accelerate
 *       the process since all other threads have to wait for one thread to
 *       finish going through a file or directory before any other thread
 *       can run. Adding more threads increases the probability that the
 *       CPU scheduler will run the code more often, however, the overhead
 *       from creating multiple threads negates that effect. This was the
 *       conclusion after running multiple tests and comparing the timing
 *       results with that of the single threaded option.
 **********************************************/

#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <pthread.h>

/***** HELPER FUCTIONS: WORK QUEUE *******************/
#define QUEUE_INITIALIZER NULL
#define NUM_WORKER_THREADS 10

/***** CUSTOM TYPES **********************************/
struct queue {
    char path[PATH_MAX];
    struct queue *next;
};
typedef struct queue* queue_t;

typedef struct stopwatch {
    struct timeval start;
} stopwatch_t;

/***** REUSED FROM HW5 *******************************/
struct workers_state {
    pthread_mutex_t mutex;
    pthread_cond_t signal;
};

static struct workers_state wstate = {
    .mutex = PTHREAD_MUTEX_INITIALIZER,
    .signal = PTHREAD_COND_INITIALIZER
};
/****************************************************/

/***************************/

/***** GLOBAL VARIABLES ******************************/
static unsigned int num_occurences = 0;
queue_t work_queue = QUEUE_INITIALIZER;

int ret;
struct stat file_stats;
char current_path[PATH_MAX];
char* string;
unsigned int num_rem_threads = NUM_WORKER_THREADS;

/***************************/

/* adds the contents path to the queue */
void enqueue (queue_t* head, char* path)
{
    queue_t next = *head;

    *head = malloc(sizeof(**head));
    (*head)->next = next;
    strcpy((*head)->path, path);
}

/* removes the oldest item from the queue and populates it into path.
 * if the queue is empty, path is populated with NULL */
void dequeue(queue_t* head, char* path)
{
    queue_t  cur = *head;
    queue_t* pp = head;

    while (cur) {
        if (cur->next == NULL) {
            strcpy(path, cur->path);
            *pp = cur->next;
            free(cur);
            return;
        }

        pp = &cur->next;
        cur = cur->next;
    }
}
/***************************/


/***** HELPER FUCTIONS: CODE TIMING ******************/
void stopwatch_start (stopwatch_t* sw)
{
    gettimeofday(&sw->start, NULL);
}

float stopwatch_report (stopwatch_t* sw)
{
    struct timeval stop;

    gettimeofday(&stop, NULL);
    return (float)(stop.tv_sec - sw->start.tv_sec + (stop.tv_usec - sw->start.tv_usec)/1000000.0);
}
/***************************/


/***** HELPER FUCTIONS: PRINT USAGE ******************/
void print_usage (char* prog)
{
    printf("Usage: %s mode path string \n\n", prog);
    printf("    mode    -   either -S for single thread or -P for pthreads\n");
    printf("    path    -   recursively scan all files in this path and report\n");
    printf("                   all occurances of string\n");
    printf("    string  -   scan files for this string\n\n");
}
/***************************/




/******************************************************************************
 *********************  M I N I   G R E P   S T A R T *************************
 ******************************************************************************/

/* Decend into the directory located at "current_path" and add all
 * the files and/or directories it contains to the work_queue */
unsigned int handle_directory (queue_t* work_queue, char* current_path)
{
    DIR *ptr_dir = NULL;
    struct dirent *ptr_result;
    char new_path[PATH_MAX];

    ptr_dir = opendir(current_path);
    if (!ptr_dir)
        return -1;

    /* scan through all files within the directory */
    while (1) {
        /* obtain a pointer to the current directory entry and store
         * it in ptr_entry.  if ptr_result is NULL, we have
         * cycled through all items in the directory */
        ptr_result = readdir(ptr_dir);

        if (ptr_result == NULL)
            break;

        /* Ignore "." (this directory) and ".." (parent directory) */
        if (!strcmp(ptr_result->d_name, ".") || !strcmp(ptr_result->d_name, ".."))
            continue;

        /* add the file or directory to the work queue */
        strcpy(new_path, current_path);
        strcat(new_path, "/");
        strcat(new_path, ptr_result->d_name);
        enqueue(work_queue, new_path);
    }
    closedir(ptr_dir);

    return 0;
}


/* Search the file located at "current_path" for "string" line-by-line.
 * If we find a line that contains the string, we print the name of
 * the file, the line number, and the line itself. */
unsigned int handle_file (char* current_path, char* string)
{
    FILE *fp;
    char *offset;

    size_t len = 0;
    char* line = NULL;
    unsigned int line_number = 0;

    fp = fopen(current_path, "r");
    if (fp == NULL)
        return -1;

    while (getline(&line, &len, fp) != -1 ) {
        line_number++;

        /* get offset of substring "string" within the string "line" */
        offset = strstr(line, string);
        if (offset != NULL) {
            printf("%s:%u: %s", current_path, line_number, line);
            num_occurences++;
        }
    }
    fclose(fp);
    free(line);

    return 0;
}


/* Given a starting path, minigrep_simple using a single thread
 * to recursively search all files and directories within path
 * for the specified string */
void minigrep_simple (char* path, char* string)
{
    /* the path specified on the command line is the first work item */
    enqueue(&work_queue, path);
    /* While there is work in the queue, process it. */
    while(work_queue != NULL){

        /* get the next item from the work queue */
        dequeue(&work_queue, current_path);
        /* and retrieve its file type information */
        lstat(current_path, &file_stats);

        /* if work item is a file, scan it for our string
         * if work item is a directory, add its contents to the work queue */
        if (S_ISDIR(file_stats.st_mode)) {
            /* work item is a directory; descend into it and post work to the queue */
            ret = handle_directory(&work_queue, current_path);
            if (ret < 0) {
                fprintf(stderr, "warning -- unable to decend into %s\n", current_path);
                continue;
            }
        }
        else if (S_ISREG(file_stats.st_mode)) {
            /* work item is a file; scan it for our string */
            ret = handle_file(current_path, string);
            if (ret < 0) {
                fprintf(stderr, "warning -- unable to open %s\n", current_path);
                continue;
            }
        }
        else if (S_ISLNK(file_stats.st_mode)) {
            /* work item is a symbolic link -- do nothing */
        }
        else {
            printf("warning -- skipping file of unknown type %s\n", current_path);
        }
    }

    printf("\n\nFound %u instance(s) of string \"%s\".\n", num_occurences, string);
}

void* worker_thread (void* param)
{
    pthread_mutex_lock(&wstate.mutex);

    if (work_queue != NULL) {
        /* get the next item from the work queue */
        dequeue(&work_queue, current_path);
        /* and retrieve its file type information */
        lstat(current_path, &file_stats);

        /* if work item is a file, scan it for our string
         * if work item is a directory, add its contents to the work queue */
        if (S_ISDIR(file_stats.st_mode)) {
            /* work item is a directory; descend into it and post work to the queue */
            ret = handle_directory(&work_queue, current_path);
            if (ret < 0) {
                fprintf(stderr, "warning -- unable to decend into %s\n", current_path);
            }
        }
        else if (S_ISREG(file_stats.st_mode)) {
            /* work item is a file; scan it for our string */
            ret = handle_file(current_path, string);
            if (ret < 0) {
                fprintf(stderr, "warning -- unable to open %s\n", current_path);
            }
        }
        else if (S_ISLNK(file_stats.st_mode)) {
            /* work item is a symbolic link -- do nothing */
        }
        else {
            printf("warning -- skipping file of unknown type %s\n", current_path);
        }
        num_rem_threads--;
    }
    else {
        num_rem_threads = 0;
    }

    pthread_mutex_unlock(&wstate.mutex);

    pthread_cond_signal(&wstate.signal);
}

/* Given a starting path, minigrep_pthreads uses multiple threads
 * to recursively search all files and directories within path
 * for the specified string */
void minigrep_pthreads(char* path, char* string)
{
    unsigned int i;

    /* the path specified on the command line is the first work item */
    enqueue(&work_queue, path);

    while(work_queue != NULL){
        num_rem_threads = NUM_WORKER_THREADS;
        /* Create NUM_WORKER_THREADS threads */
        for(i = 0; i < NUM_WORKER_THREADS; i++){
            pthread_t tid;
            pthread_create(&tid, NULL, worker_thread, NULL);
            pthread_detach(tid);
        }

        /* While there is work in the queue, process it. */
        pthread_mutex_lock(&wstate.mutex);
        while(num_rem_threads){
            /* Block using condition variable while there are still working threads */
            pthread_cond_wait(&wstate.signal, &wstate.mutex);
        }
        pthread_mutex_unlock(&wstate.mutex);
    }

    printf("\n\nFound %u instance(s) of string \"%s\".\n", num_occurences, string);
}


int main(int argc, char** argv)
{
    stopwatch_t T;

    if(argc < 4){
        print_usage (argv[0]);
        return EXIT_FAILURE;
    }
    string = argv[3];

    if (!strcmp(argv[1], "-S")) {
        /* Perform a serial search of the file system */
        stopwatch_start(&T);
        minigrep_simple(argv[2], string);
        printf("Single Thread Execution Time: %f\n", stopwatch_report(&T));
    }
    else if (!strcmp(argv[1], "-P")) {
        /* Perform a multi-threaded search of the file system */
        stopwatch_start(&T);
        minigrep_pthreads(argv[2], string);
        printf("pthreads Execution Time: %f\n", stopwatch_report(&T));
    }
    else {
        printf("error -- invalide mode specified\n\n");
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
