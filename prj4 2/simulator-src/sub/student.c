/*
 * student.c
 * Multithreaded OS Simulation for CS 2200, Project 4
 * Fall 2014
 *
 * This file contains the CPU scheduler for the simulation.
 * Name:  Yang Yang
 * GTID: 902986862
 */

#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "os-sim.h"


/*
 * current[] is an array of pointers to the currently running processes.
 * There is one array element corresponding to each CPU in the simulation.
 *
 * current[] should be updated by schedule() each time a process is scheduled
 * on a CPU.  Since the current[] array is accessed by multiple threads, you
 * will need to use a mutex to protect it.  current_mutex has been provided
 * for your use.
 */
static pcb_t **current;
static pthread_mutex_t current_mutex;
static pthread_mutex_t ready_mutex;
static pthread_cond_t notIdle;
static int timeslice;
static pcb_t *head;
static int cpu_count;
int rr;
int p;
static void addReady(pcb_t *pcb) {
    pcb_t *cur;
    pthread_mutex_lock(&ready_mutex);
    cur = head;
    if (cur == NULL) {
        head = pcb;
    } else {
        while(cur->next != NULL) {
            cur = cur->next;
        }
        cur->next = pcb;
    }
    pcb->next = NULL;
    pthread_cond_broadcast(&notIdle);
    pthread_mutex_unlock(&ready_mutex);
}
static pcb_t* popReady(void) {
    pcb_t *cur;
    pthread_mutex_lock(&ready_mutex);
    cur = head;
    if (cur != NULL) {
        head = head->next;      
    }
    pthread_mutex_unlock(&ready_mutex);
    return cur;
}
static pcb_t* popProirity(void) {
    pcb_t* cur;
    pcb_t* prev;
    pcb_t* toRe;
    pthread_mutex_lock(&ready_mutex);

    if (head == NULL) {
        toRe = NULL;
    } else {
        cur = head;
        prev = head;
        toRe = head;
        while(cur -> next != NULL) {
            if (cur->next->static_priority > toRe->static_priority) {
                prev = cur;
                toRe = cur->next;
            }

            cur = cur->next;
        }
        if (toRe != NULL) {
            if (toRe == head) {
                head = head->next;
            } else {
                prev->next = toRe->next;
            }
        }
    }
    pthread_mutex_unlock(&ready_mutex);
    return toRe;
}

/*
 * schedule() is your CPU scheduler.  It should perform the following tasks:
 *
 *   1. Select and remove a runnable process from your ready queue which 
 *	you will have to implement with a linked list or something of the sort.
 *
 *   2. Set the process state to RUNNING
 *
 *   3. Call context_switch(), to tell the simulator which process to execute
 *      next on the CPU.  If no process is runnable, call context_switch()
 *      with a pointer to NULL to select the idle process.
 *	The current array (see above) is how you access the currently running process indexed by the cpu id. 
 *	See above for full description.
 *	context_switch() is prototyped in os-sim.h. Look there for more information 
 *	about it and its parameters.
 */
static void schedule(unsigned int cpu_id)
{
    pcb_t *pcb;
    if (!p) {
        pcb = popReady();
    } else {
        pcb = popProirity();
    }
    if (pcb != NULL) {
        pcb->state = PROCESS_RUNNING;
    }
    pthread_mutex_lock(&current_mutex);
    current[cpu_id] = pcb;
    pthread_mutex_unlock(&current_mutex);
    context_switch(cpu_id, pcb,timeslice);
}


/*
 * idle() is your idle process.  It is called by the simulator when the idle
 * process is scheduled.
 *
 * This function should block until a process is added to your ready queue.
 * It should then call schedule() to select the process to run on the CPU.
 */
extern void idle(unsigned int cpu_id)
{
    pthread_mutex_lock(&ready_mutex);
    while (head == NULL) {
        pthread_cond_wait(&notIdle, &ready_mutex);
    }
    pthread_mutex_unlock(&ready_mutex);
    schedule(cpu_id);

    /*
     * REMOVE THE LINE BELOW AFTER IMPLEMENTING IDLE()
     *
     * idle() must block when the ready queue is empty, or else the CPU threads
     * will spin in a loop.  Until a ready queue is implemented, we'll put the
     * thread to sleep to keep it from consuming 100% of the CPU time.  Once
     * you implement a proper idle() function using a condition variable,
     * remove the call to mt_safe_usleep() below.
     */
}


/*
 * preempt() is the handler called by the simulator when a process is
 * preempted due to its timeslice expiring.
 *
 * This function should place the currently running process back in the
 * ready queue, and call schedule() to select a new runnable process.
 */
extern void preempt(unsigned int cpu_id)
{
    pcb_t* cur;
    pthread_mutex_lock(&current_mutex);
    cur = current[cpu_id];
    cur->state = PROCESS_READY;
    pthread_mutex_unlock(&current_mutex);
    addReady(cur);
    schedule(cpu_id);
}


/*
 * yield() is the handler called by the simulator when a process yields the
 * CPU to perform an I/O request.
 *
 * It should mark the process as WAITING, then call schedule() to select
 * a new process for the CPU.
 */
extern void yield(unsigned int cpu_id)
{
    pcb_t* cur;
    pthread_mutex_lock(&current_mutex);
    cur = current[cpu_id];
    cur->state = PROCESS_WAITING;
    pthread_mutex_unlock(&current_mutex);
    schedule(cpu_id);
}


/*
 * terminate() is the handler called by the simulator when a process completes.
 * It should mark the process as terminated, then call schedule() to select
 * a new process for the CPU.
 */
extern void terminate(unsigned int cpu_id)
{
    pcb_t* cur;
    pthread_mutex_lock(&current_mutex);
    cur = current[cpu_id];
    cur->state = PROCESS_TERMINATED;
    pthread_mutex_unlock(&current_mutex);
    schedule(cpu_id); /* FIX ME */
}


/*
 * wake_up() is the handler called by the simulator when a process's I/O
 * request completes.  It should perform the following tasks:
 *
 *   1. Mark the process as READY, and insert it into the ready queue.
 *
 *   2. If the scheduling algorithm is static priority, wake_up() may need
 *      to preempt the CPU with the lowest priority process to allow it to
 *      execute the process which just woke up.  However, if any CPU is
 *      currently running idle, or all of the CPUs are running processes
 *      with a higher priority than the one which just woke up, wake_up()
 *      should not preempt any CPUs.
 *	To preempt a process, use force_preempt(). Look in os-sim.h for 
 * 	its prototype and the parameters it takes in.
 */
extern void wake_up(pcb_t *process)
{
    int i, lowest, low_id;
    process->state = PROCESS_READY;
    addReady(process);
    if (p) {
        lowest = process->static_priority;
        low_id = -1;
        pthread_mutex_lock(&current_mutex);
        for (i = 0; i < cpu_count; i++) {
            if (current[i] != NULL) {
                if(current[i] -> static_priority < lowest) {
                    lowest = current[i]->static_priority;
                    low_id = i;
                }
            }
        } 
        pthread_mutex_unlock(&current_mutex);
        if (low_id != -1) {
            force_preempt(low_id);
        }
    }
}
static int errorme() {
            fprintf(stderr, "CS 2200 Project 4 -- Multithreaded OS Simulator\n"
            "Usage: ./os-sim <# CPUs> [ -r <time slice> | -p ]\n"
            "    Default : FIFO Scheduler\n"
            "         -r : Round-Robin Scheduler\n"
            "         -p : Static Priority Scheduler\n\n");
        return -1;
}

/*
 * main() simply parses command line arguments, then calls start_simulator().
 * You will need to modify it to support the -r and -p command-line parameters.
 */
int main(int argc, char *argv[])
{

    timeslice = -1;
    rr = 0;
    p = 0;


    /* Parse command-line arguments */
    if (argc < 2 || argc > 5)
    {
        errorme();
        exit(1);
    }
    cpu_count = atoi(argv[1]);

    /* FIX ME - Add support for -r and -p parameters*/
    int i;
    for (i = 0; i < argc; i++) {
        if (strcmp(argv[i], "-r") == 0) {
            rr = 1;
            if (i + 1 >= argc) {
                errorme();
                exit(1);
            }else {
                timeslice = atoi(argv[i + 1]);
            }
        } else if (strcmp(argv[i], "-p") == 0) {
            p = 1;
        }
    }
    /* Allocate the current[] array and its mutex */
    current = malloc(sizeof(pcb_t*) * cpu_count);
    assert(current != NULL);
    pthread_mutex_init(&current_mutex, NULL);
    head = NULL;
    pthread_mutex_init(&ready_mutex,NULL);
    pthread_cond_init(&notIdle, NULL);
    /* Start the simulator in the library */
    start_simulator(cpu_count);

    return 0;
}



