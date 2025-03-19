
# CS4760 - Project 3: Message Passing
**Git Repository:** [https://github.com/ananquay15x9/CS4760-P3](https://github.com/ananquay15x9/CS4760-P3.git)

## Project Description

This project simulates a system with a master process ('oss') and worker processes, utilizing shared memory, message queues, and a simulated clock. The `oss` process manages worker creation, message passing, and time simulation, while worker processes perform simulated tasks and communicate with the `oss`.

## Compilation

To compile this project, navigate to the project directory and run 'make' in the terminal:

```bash
make

## Running

To run the project, execute ./oss with the desired command-line options:

* '-h': Display help information
* '-n <num_workers>': Specify the number of worker processes to create (default: 5)
* '-s <max_seconds)>': Specify the maximum seconds a worker process can run
* '-ns <max_nanoseconds>': Specify the maximum nanoseconds a worker process can run
* '-t <time_to_launch>': Specify the interval in nanoseconds for launching worker processes
* '-f <logfile>': Specify the log file name. If not provided, logs to standard output

Example:
```bash
./oss -n 2 -s 1 -t 3 -i 500 -f test1.log 

## Problems Encountered and Solutions

Several issues were identified and resolved:

1. **Missing Closing Braces**:
	- **Problem**: The code would not compile due to an "expected declaration or statement at end of input" error.
	- **Solution**: Ensure that all opening braces '{' have corresponding closing braces '}'. This was particularly important for the 'main' function and loops.

2. **Unused Variable Warning**:
	- **Problem**: Warnings were generated for variables that were declared but not used ('increment' and 'intervalInMsToLaunchChildren').
	- **Solution**: These warnings can be ignored.

3. **Undeclared Identifiers**:
	- **Problem**: Errors related to undeclared identifiers
	- **Solution**: Ensured all variables are declared before being used

4. **Output  Timing**:
	- **Problem**: Ensuring worker processes correctly calculate their termination times and terminate as expected
	- **Solution**: Thoroughly tested worker process timing calculations and termination logic, verified through verbose logging and process table monitoring

5. **Worker mtype Issues:**:
	- **Problem**: Workers were not receiving messages from the OSS due to incorrect message type (mtype)
	- **Solution**: Corrected the msgrcv call in worker.c to use the worker's PID as the expected mtype

6. **OSS Sending Messages to Terminated Workers**:
	- **Problem**: The oss was attempting to send messages to workers after they had terminated
	- **Solution**: Added a check to the oss that verifies that a worker is still running before sending a message to it


