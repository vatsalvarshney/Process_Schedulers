# Process Scheduling Algorithms in C

This project implements various CPU scheduling algorithms in C, covering both offline and online scheduling scenarios, as an assignment for the course on Operating Systems (MTL458) by Ashutosh Rai at IIT Delhi

## Implemented Algorithms
### Offline Scheduling
These schedulers take the array of commands as input beforehand
1. **First-Come, First-Served (FCFS)**: Execute each process until completion before executing any other process.
2. **Round Robin (RR)**: Execute each of the remaining processes for a small time slice in a loop.
3. **Multi-level Feedback Queue (MLFQ)**: Maintains three priority queues (High, Medium, Low), executes the processes in the highest non-empty queue in a Round-Roubin fashion, demoting each process if it uses the entire time slice. Boosts all processes to High priority after regular intervals of time.

### Online Scheduling
These schedulers read the input from the terminal in real time and schedule each process according to its past behavior.
1. **Shortest Job First (SJF)**: Executes the process with historically least burst time.
2. **Multi-level Feedback Queue (MLFQ)**: Assign priorities based on historical burst times.

## Execution Details
1. **Input**: Process commands as per the scheduling type.
2. **Output**: Detailed log of context switches and process metrics (CSV files named as `result_<type>_<scheduler>.csv`).
3. **Metrics Tracked**:
   - Completion Time
   - Turnaround Time
   - Waiting Time
   - Response Time
   - Error Handling

## Files Included
- `offline_schedulers.h`: Contains offline scheduling logic.
- `online_schedulers.h`: Contains online scheduling logic.
- `utils.h`: Contains util functions common to both files.

---

**Author**: Vatsal Varshney

**License**: MIT
