#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <stdbool.h>
#include <fcntl.h>
#include <string.h>
#include <limits.h>


typedef struct {
    char *command;
    int process_id;
    bool started;
    bool finished;
    bool error;
    uint64_t arrival_time;
    uint64_t response_time;
    uint64_t start_time;
    uint64_t burst_time;
    uint64_t waiting_time;
    uint64_t completion_time;
    uint64_t turnaround_time;
    int history_idx;
} Process;


/* Get the current time in milliseconds relative to the start_time */
uint64_t ms_time(uint64_t start_time){
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000 - start_time;
}


/*
Get array of args from the given command character array
Input:
    char *command: command character array
Output:
    char **: array of arguments
*/
char **get_args_from_command(char *command){
    int args_size = 4;
    char **args = malloc(args_size * sizeof(char *));
    int j = 0;
    char *token = strtok(strdup(command), " ");
    while (token != NULL){
        if (j >= args_size){
            args_size *= 2;
            args = realloc(args, args_size * sizeof(char *));
        }
        args[j++] = token;
        token = strtok(NULL, " ");
    }
    args[j] = NULL;
    return args;
}


/*
Create a child process to execute the given command
Input:
    Process *p: pointer to the process
    uint64_t start_time: start time of the scheduler
*/
void start_process(Process *p, uint64_t start_time){
    p->started = true;
    p->start_time = ms_time(start_time);
    p->response_time = p->start_time - p->arrival_time;
    int pid = fork();
    if (pid == 0) {
        char **args = get_args_from_command(p->command);
        execvp(args[0], args);
        exit(1);
    } else if (pid < 0) {
        p->error = true;
        p->completion_time = ms_time(start_time);
    } else {
        p->process_id = pid;
    }
}

/*
Print the output after every context switch
Input:
    Process *p: pointer to the process
    uint64_t start_time: start time of the context
    uint64_t end_time: end time of the context
*/
void context_switch_output(Process *p, uint64_t start_time, uint64_t end_time){
    printf("%s|%lu|%lu\n", p->command, start_time, end_time);
}


/*
Run the given process completely
Input:
    Process *p: pointer to the process
    uint64_t start_time: start time of the scheduler
*/
void run_process_completely(Process *p, uint64_t start_time, FILE *file){
    uint64_t context_start_time = ms_time(start_time);
    start_process(p, start_time);
    int status;
    waitpid(p->process_id, &status, 0);
    if (WIFEXITED(status) && WEXITSTATUS(status) == 1){
        p->error = true;
    } else {
        p->finished = true;
    }
    p->completion_time = ms_time(start_time);
    p->burst_time = p->completion_time - p->start_time;
    p->turnaround_time = p->completion_time - p->arrival_time;
    p->waiting_time = p->turnaround_time - p->burst_time;

    fprintf(
        file, "%s,%s,%s,%lu,%lu,%lu,%lu\n",
        p->command,
        p->finished ? "Yes" : "No",
        p->error ? "Yes" : "No",
        p->burst_time,
        p->turnaround_time,
        p->waiting_time,
        p->response_time
    );
    fflush(file);

    context_switch_output(p, context_start_time, p->completion_time);
};


/* 
Run the given process for the given time slice
Input:
    Process *p: pointer to the process
    int quantum: time slice in milliseconds
    uint64_t start_time: start time of the scheduler
Output:
    -1 if process has already finished or errored
    0 if process finished or errored during the time slice
    1 if process used up the entire time slice
*/
int run_process_for_quantum(Process *p, int quantum, uint64_t start_time, FILE *file){
    uint64_t context_start_time = ms_time(start_time);
    if (p->error || p->finished) {
        return -1;
    } else if (!p->started) {
        start_process(p, start_time);
    } else {
        kill(p->process_id, SIGCONT);
    }

    usleep(quantum*1000);
    p->burst_time += quantum;
    int status;
    int res = waitpid(p->process_id, &status, WNOHANG);
    int ret = 1;
    if (res > 0){
        if (WIFEXITED(status) && WEXITSTATUS(status) == 1){
            p->error = true;
        } else {
            p->finished = true;
        }
        p->completion_time = ms_time(start_time);
        p->turnaround_time = p->completion_time - p->arrival_time;
        p->waiting_time = p->turnaround_time - p->burst_time;
        fprintf(
            file, "%s,%s,%s,%lu,%lu,%lu,%lu\n",
            p->command,
            p->finished ? "Yes" : "No",
            p->error ? "Yes" : "No",
            p->burst_time,
            p->turnaround_time,
            p->waiting_time,
            p->response_time
        );
        fflush(file);
        ret = 0;
    } else if (res == 0){
        kill(p->process_id, SIGSTOP);
    }

    context_switch_output(p, context_start_time, ms_time(start_time));

    return ret;
};
