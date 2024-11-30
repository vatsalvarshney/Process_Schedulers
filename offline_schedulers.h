#pragma once

#include "utils.h"


/*
First Come First Serve Scheduling
Input:
    Process p[]: array of processes
    int n: number of processes
Output: None
*/
void FCFS(Process p[], int n){
    uint64_t scheduler_start_time = ms_time(0);
    FILE *file = fopen("result_offline_FCFS.csv", "w");
    fprintf(file, "Command,Finished,Error,Burst Time,Turnaround Time,Waiting Time,Response Time\n");
    fflush(file);
    for (int i=0; i<n; i++){
        run_process_completely(&p[i], scheduler_start_time, file);
    }
    fclose(file);
};


/*
Round Robin Scheduling: Each process is assigned a fixed time slice in cyclic order
Input:
    Process p[]: array of processes
    int n: number of processes
    int quantum: time slice in milliseconds
Output: None
*/
void RoundRobin(Process p[], int n, int quantum){
    int i = -1;
    int num_finished = 0;
    int status;
    int res;
    uint64_t scheduler_start_time = ms_time(0);

    FILE *file = fopen("result_offline_RR.csv", "w");
    fprintf(file, "Command,Finished,Error,Burst Time,Turnaround Time,Waiting Time,Response Time\n");
    fflush(file);

    while (num_finished < n){
        i = (i+1)%n;
        res = run_process_for_quantum(&p[i], quantum, scheduler_start_time, file);
        if (res == 0){
            num_finished++;
        }
    }

    fclose(file);
};


/*
Multi-Level Feedback Queue Scheduling with 3 queues
Input:
    Process p[]: array of processes
    int n: number of processes
    int quantum0: time slice for queue 0
    int quantum1: time slice for queue 1
    int quantum2: time slice for queue 2
    int boostTime: time after which all processes are boosted to the highest priority queue
Output: None
*/
void MultiLevelFeedbackQueue(Process p[], int n, int quantum0, int quantum1, int quantum2, int boostTime){

    // queue[i] has list of indices of processes in queue i
    int *queue[3] = {malloc(n * sizeof(int)), malloc(n * sizeof(int)), malloc(n * sizeof(int))};
    int queue_size[3] = {0, 0, 0};
    int quantum[3] = {quantum0, quantum1, quantum2};

    // add all processes to the highest priority queue
    for (int i=0; i<n; i++){
        queue[0][queue_size[0]++] = i;
    }

    uint64_t scheduler_start_time = ms_time(0);
    uint64_t last_boost_time = 0;
    int queue_to_run;    // queue to run the process from
    int proc_to_run[3] = {0, 0, 0};    // process to run from each queue
    int status;
    int res;

    FILE *file = fopen("result_offline_MLFQ.csv", "w");
    fprintf(file, "Command,Finished,Error,Burst Time,Turnaround Time,Waiting Time,Response Time\n");
    fflush(file);

    while (queue_size[0]+queue_size[1]+queue_size[2] > 0){

        // boost all processes to the highest priority queue
        if (ms_time(scheduler_start_time) - last_boost_time >= boostTime){
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
        for (int i=0; i<3; i++){
            if (queue_size[i] > 0){
                queue_to_run = i;
                break;
            }
        }

        Process *cp = &p[queue[queue_to_run][proc_to_run[queue_to_run]]];
        res = run_process_for_quantum(cp, quantum[queue_to_run], scheduler_start_time, file);
        if (res <= 0){
            // if process is finished or errored, remove it from the queue
            for (int i = proc_to_run[queue_to_run]; i < queue_size[queue_to_run]-1; i++){
                queue[queue_to_run][i] = queue[queue_to_run][i+1];
            }
            queue_size[queue_to_run]--;
            if (queue_size[queue_to_run]>=proc_to_run[queue_to_run]){
                proc_to_run[queue_to_run] = 0;
            }
        } else {
            // process used up the entire time slice
            if (queue_to_run < 2){
                // move the process to the next lower priority queue if present
                queue[queue_to_run+1][queue_size[queue_to_run+1]++] = queue[queue_to_run][0];
                for (int i = 0; i < queue_size[queue_to_run]-1; i++){
                    queue[queue_to_run][i] = queue[queue_to_run][i+1];
                }
                queue_size[queue_to_run]--;
            } else {
                // if the process is already in the lowest priority queue, move to the next process
                proc_to_run[2] = (proc_to_run[2]+1) % queue_size[queue_to_run];
            }
        }
    }

    fclose(file);
    free(queue[0]);
    free(queue[1]);
    free(queue[2]);
}

