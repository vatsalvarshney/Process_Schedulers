#pragma once

#include "utils.h"

#define DEFAULT_CAPACITY 100

typedef struct {
    char *command;
    uint64_t avg_burst_time;
    uint64_t history_size;
} ProcessHistory;


/* append the given process in the given array of processes */
void append_process(Process *processes, int *size, int *cap, Process *p){
    if (*size >= *cap){
        *cap *= 2;
        processes = realloc(processes, sizeof(Process) * (*cap));
    }
    processes[*size] = *p;
    *size += 1;
}


/* append the given process in the given array of process histories */
void append_process_history(ProcessHistory *process_histories, int *size, int *cap, Process *p){
    if (*size >= *cap){
        *cap *= 2;
        process_histories = realloc(process_histories, sizeof(ProcessHistory) * (*cap));
    }
    process_histories[*size].command = strdup(p->command);
    process_histories[*size].avg_burst_time = 1000;
    process_histories[*size].history_size = 0;
    *size += 1;
}


/*
Check the stdin for new processes and add them to the array of new_processes
Input:
    Process *new_processes: array of new processes
    int *n: number of new processes
    uint64_t scheduler_start_time: start time of the scheduler
*/
void check_for_new_processes(Process *new_processes, int *n, uint64_t scheduler_start_time){
    int BUFFER_SIZE = 1000;
    int capacity = DEFAULT_CAPACITY;
    char command[BUFFER_SIZE];

    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

    while (fgets(command, sizeof(command), stdin) != NULL){
        size_t len = strlen(command);
        if (len > 0 && command[len-1] == '\n'){
            command[len-1] = '\0';
        }

        Process p;
        p.command = strdup(command);
        p.arrival_time = ms_time(scheduler_start_time);
        append_process(new_processes, n, &capacity, &p);

        usleep(10000);
        if (feof(stdin)){
            break;
        }
    }
    fcntl(STDIN_FILENO, F_SETFL, flags);
}


/*
Shortest Job First (SJF) Scheduling
*/
void ShortestJobFirst(){
    // array of ProcessHistory to store the history of each process
    int history_capacity = DEFAULT_CAPACITY;
    ProcessHistory *process_histories = malloc(sizeof(ProcessHistory) * history_capacity);
    int num_process_histories = 0;

    // array of Process to store the pending processes
    int process_capacity = DEFAULT_CAPACITY;
    Process *processes = malloc(sizeof(Process) * process_capacity);
    int num_processes = 0;

    uint64_t scheduler_start_time = ms_time(0);
    FILE *file = fopen("result_online_SJF.csv", "w");
    fprintf(file, "Command,Finished,Error,Burst Time,Turnaround Time,Waiting Time,Response Time\n");
    fflush(file);

    while (1){
        // get new processes
        Process *new_processes = malloc(sizeof(Process) * DEFAULT_CAPACITY);
        int num_new_processes = 0;
        check_for_new_processes(new_processes, &num_new_processes, scheduler_start_time);
        
        // for each new process, check if it is already in the history, if not add it to the history
        for (int i=0; i<num_new_processes; i++){
            int history_idx = -1;
            for (int j=0; j<num_process_histories; j++){
                if (strcmp(process_histories[j].command, new_processes[i].command) == 0){
                    history_idx = j;
                    break;
                }
            }
            if (history_idx == -1){
                history_idx = num_process_histories;
                append_process_history(process_histories, &num_process_histories, &history_capacity, &new_processes[i]);
            }
            new_processes[i].history_idx = history_idx;
        }

        // add new processes to the array of processes
        if (num_new_processes+num_processes >= process_capacity){
            process_capacity = num_new_processes+num_processes;
            processes = realloc(processes, sizeof(Process) * process_capacity);
        }
        for (int i=0; i<num_new_processes; i++){
            processes[num_processes++] = new_processes[i];
        }

        free(new_processes);

        // find the process with the shortest avg burst time
        int shortest_burst_time = INT_MAX;
        int shortest_burst_time_index = -1;
        for (int i=0; i<num_processes; i++){
            ProcessHistory *cph = &process_histories[processes[i].history_idx];
            if (cph->avg_burst_time < shortest_burst_time){
                shortest_burst_time = cph->avg_burst_time;
                shortest_burst_time_index = i;
            }
        }

        // run the process with the shortest avg burst time
        if (shortest_burst_time_index != -1){
            Process *cp = &processes[shortest_burst_time_index];
            run_process_completely(cp, scheduler_start_time, file);
            
            // update the average burst time of the process in the history
            if (!cp->error){
                ProcessHistory *cph = &process_histories[cp->history_idx];
                cph->avg_burst_time = (cph->avg_burst_time * cph->history_size + cp->burst_time) / (cph->history_size + 1);
                cph->history_size++;
            }

            // remove the process from the array of processes
            for (int i=shortest_burst_time_index; i<num_processes-1; i++){
                processes[i] = processes[i+1];
            }
            num_processes--;
        }
    }

    fclose(file);
    free(process_histories);
    free(processes);
}


/*
Multi-Level Feedback Queue Scheduling
Input:
    int quantum0: time slice for queue 0
    int quantum1: time slice for queue 1
    int quantum2: time slice for queue 2
    int boostTime: time after which all processes are boosted to the highest priority queue
*/
void MultiLevelFeedbackQueue(int quantum0, int quantum1, int quantum2, int boostTime){

    // array of ProcessHistory to store the history of each process
    int history_capacity = DEFAULT_CAPACITY;
    ProcessHistory *process_histories = malloc(sizeof(ProcessHistory) * history_capacity);
    int num_process_histories = 0;

    // 3 queues for the pending processes
    int queue_capacity[3] = {DEFAULT_CAPACITY, DEFAULT_CAPACITY, DEFAULT_CAPACITY};
    Process **queue = malloc(sizeof(Process *) * 3);
    for (int i=0; i<3; i++){
        queue[i] = malloc(sizeof(Process) * queue_capacity[i]);
    }
    int queue_size[3] = {0, 0, 0};

    int quantum[3] = {quantum0, quantum1, quantum2};
    int queue_to_run;
    int proc_to_run[3] = {0, 0, 0};

    uint64_t scheduler_start_time = ms_time(0);
    uint64_t last_boost_time = 0;
    FILE *file = fopen("result_online_MLFQ.csv", "w");
    fprintf(file, "Command,Finished,Error,Burst Time,Turnaround Time,Waiting Time,Response Time\n");
    fflush(file);

    while (1){
        // get new processes
        Process *new_processes = malloc(sizeof(Process) * DEFAULT_CAPACITY);
        int num_new_processes = 0;
        check_for_new_processes(new_processes, &num_new_processes, scheduler_start_time);

        // for each new process, check if it is already in the history, if not add it to the history
        for (int i=0; i<num_new_processes; i++){
            int history_idx = -1;
            int queue_idx = 1;
            for (int j=0; j<num_process_histories; j++){
                if (strcmp(process_histories[j].command, new_processes[i].command) == 0){
                    history_idx = j;
                    break;
                }
            }
            if (history_idx == -1){
                new_processes[i].history_idx = num_process_histories;
                append_process_history(process_histories, &num_process_histories, &history_capacity, &new_processes[i]);
            } else {
                new_processes[i].history_idx = history_idx;
                if (process_histories[history_idx].avg_burst_time < quantum[0]){
                    queue_idx = 0;
                } else if (process_histories[history_idx].avg_burst_time < quantum[1]){
                    queue_idx = 1;
                } else {
                    queue_idx = 2;
                }
            }
            append_process(queue[queue_idx], &queue_size[queue_idx], &queue_capacity[queue_idx], &new_processes[i]);
        }
        
        free(new_processes);

        // boost all processes to the highest priority queue
        if (ms_time(scheduler_start_time) - last_boost_time >= boostTime){
            if (queue_size[0]+queue_size[1]+queue_size[2] > queue_capacity[0]){
                queue_capacity[0] = queue_size[0]+queue_size[1]+queue_size[2];
                queue[0] = realloc(queue[0], sizeof(Process) * queue_capacity[0]);
            }
            for (int i=1; i<3; i++){
                for (int j=0; j<queue_size[i]; j++){
                    queue[0][queue_size[0]++] = queue[i][j];
                }
                queue_size[i] = 0;
            }
            last_boost_time = ms_time(scheduler_start_time);
            proc_to_run[2] = 0;
        }

        // find the highest priority queue that is not empty
        queue_to_run = -1;
        for (int i=0; i<3; i++){
            if (queue_size[i] > 0){
                queue_to_run = i;
                break;
            }
        }

        if (queue_to_run != -1){
            Process *cp = &queue[queue_to_run][proc_to_run[queue_to_run]];
            int res = run_process_for_quantum(cp, quantum[queue_to_run], scheduler_start_time, file);

            if (res <= 0){
                // if process is finished or errored, update the average burst time in the history and remove it from the queue
                if (!cp->error){
                    ProcessHistory *cph = &process_histories[cp->history_idx];
                    cph->avg_burst_time = (cph->avg_burst_time * cph->history_size + cp->burst_time) / (cph->history_size + 1);
                    cph->history_size++;
                }

                for (int i=proc_to_run[queue_to_run]; i<queue_size[queue_to_run]-1; i++){
                    queue[queue_to_run][i] = queue[queue_to_run][i+1];
                }
                queue_size[queue_to_run]--;

                if (queue_size[queue_to_run] >= proc_to_run[queue_to_run]){
                    proc_to_run[queue_to_run] = 0;
                }
            } else {
                // process used up the entire time slice
                if (queue_to_run < 2){
                    // move the process to the next lower priority queue if present
                    append_process(queue[queue_to_run+1], &queue_size[queue_to_run+1], &queue_capacity[queue_to_run+1], cp);
                    for (int i=0; i<queue_size[queue_to_run]-1; i++){
                        queue[queue_to_run][i] = queue[queue_to_run][i+1];
                    }
                    queue_size[queue_to_run]--;
                } else {
                    // if the process is already in the lowest priority queue, move to the next process
                    proc_to_run[2] = (proc_to_run[2]+1) % queue_size[queue_to_run];
                }
            }
        }
    }

    fclose(file);
    free(process_histories);
    free(queue[0]);
    free(queue[1]);
    free(queue[2]);
}
