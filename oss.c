// Author: Tu Le
// CS4760 Project 3
// Date:  3/13/2025

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <errno.h>
#include <getopt.h>
#include <time.h>
#include <signal.h>
#include <sys/msg.h> 

// Struct for the clock
typedef struct {
	int seconds;
	int nanoseconds;
} SimulatedClock;

//The PCB struct
typedef struct {
	int occupied; // If empty 0, if occupied 1
	pid_t pid; // Process ID of the child
	int startSeconds; //Time when child launched will be in seconds
	int startNano; 
	int messagesSent; //Number of messages sent
} PCB;

// Message structures
struct oss_message {
	long mtype; //Message type (PID of the worker)
	int command; //Command for the worker
};

struct worker_message {
	long mtype; // Message type (PID of oss)
	int status; // Status from the worker (e.g., 0 for continue, 1 for done)
};

// Global declarations 
PCB processTable[20]; //Array
int shmid;
SimulatedClock *simClock;
int  msgid; //Message queue ID
FILE *logfile; //logfile
int total_processes_launched = 0;
int total_messages_sent = 0;
int workers_completed = 0;
int children_running = 0;
pid_t  oss_pid;

//Signal handler function
void signal_handler(int sig) {
	if (sig == SIGALRM) {
		printf("OSS: 60 seconds elapsed. Terminating all child processes.\n");
	} else if (sig == SIGINT) {
		printf("OSS: Ctrl+C received. Terminating all child processes.\n");
	}

	// Terminate all child processes
	for (int i = 0; i < 20; i++) {
		if (processTable[i].occupied == 1) {
			kill(processTable[i].pid, SIGTERM); // Send termination signal to children
		}
	}
	printf("OSS: Signal handler called, children_running = %d\n", children_running); 
    		if(children_running == 0){

		// Detach shared memory
		shmdt(simClock);
		shmctl(shmid, IPC_RMID, NULL);

		// Remove message queu
		msgctl(msgid, IPC_RMID, NULL);
		if(logfile != NULL) {
			fclose(logfile);
		}
		exit(0); //exit the program
	}
}

int main(int argc, char **argv) {
	int opt;
	int num_children = 5;
	int simul_children = 3;
	int timelimitForChildren = 10;
	int intervalInMsToLaunchChildren = 100;
	int increment = 10000;
	int children_running = 0;
	int last_printed_seconds = -1;
	int last_launch_time = 0; 
	int next_process_index = 0;
	char *logfilename = NULL; //log filename
	// Command line argument parsing
	while ((opt = getopt(argc, argv, "hn:s:t:i:f:")) != -1) {
		switch (opt) {
			case  'h': //this is help option
				printf("Usage: %s [-h] [-n proc] [-s simul] [-t timelimitForChildren] [-i intervalInMsToLaunchChildren]\n", argv[0]);
				exit(0);
			case 'n': // number of child processes
                		num_children = atoi(optarg);
                		break;
            		case 's': // number of simultaneous child allowed
                		simul_children = atoi(optarg);
                		break;
            		case 't': // upper bound for child process time
                		timelimitForChildren = atoi(optarg);
                		break;
            		case 'i': // interval for launching children
                		intervalInMsToLaunchChildren = atoi(optarg);
               			 break;
			case 'f':
				logfilename = optarg;
				break;
            		default:
                		fprintf(stderr, "Usage: %s [-h] [-n proc] [-s simul] [-t timelimitForChildren] [-i intervalInMsToLaunchChildren] [-f logfile]\n", argv[0]);
               			exit(1);
        	}
    	}

	//Open log file
	if (logfilename != NULL) {
		logfile = fopen(logfilename, "w");
		if (logfile == NULL) {
			perror("fopen");
			exit(1);
		}
	}
	// Initialize the process tale below
	for (int i = 0; i < 20; i++) {
		processTable[i].occupied = 0; // mark empty first
		processTable[i].messagesSent = 0; //initialize messages went to 0
	}

	// Shared memory here
    	key_t key = ftok("oss.c", 1); // generate key based on this file
    	if (key == -1) {
        	perror("ftok");
        	exit(1);
    	}

    	//  new  Create shared memory segment
    	shmid = shmget(key, sizeof(SimulatedClock), IPC_CREAT | 0666);
    	if (shmid < 0) {
        	perror("shmget");
        	exit(1);
    	}

    	// new attached shared memory segment
    	simClock = (SimulatedClock *)shmat(shmid, NULL, 0);
    	if (simClock == (SimulatedClock *) -1) {
        	perror("shmat");
        	exit(1);
    	}

    	// New create clock
    	simClock->seconds = 0;
    	simClock->nanoseconds = 0;

    	// Setup signal handlers
    	signal(SIGALRM, signal_handler);
    	signal(SIGINT, signal_handler);

    	// set alarm for 60 seconds
    	alarm(60);
	srand(time(NULL)); //  random number

    	// create message queue
    	msgid = msgget(key, IPC_CREAT | 0666);
    	if (msgid == -1) {
        	perror("msgget in oss");
        	exit(1);
    	}
	fprintf(logfile != NULL ? logfile : stdout, "OSS: msgid = %d\n", msgid);//print statement

	while(1) {
		//Calculate the clock increment
		if (children_running > 0) {
			increment = 250000000;
		} else {
			increment = 250000000; // if no children,  increment by 250ms
		}

		// increment the clock
		simClock->nanoseconds += increment;

		// if overflow
		if (simClock->nanoseconds >= 1000000000) {
			simClock->seconds += 1;
			simClock->nanoseconds -= 1000000000;
		}

		// check for terminate children
		int status;
		pid_t child_pid = waitpid(-1, &status, WNOHANG);

		if (child_pid > 0) {
			//a child process has terinate
			children_running--;

			//update the process table
			for (int i = 0; i < 20; i++) {
				if (processTable[i].pid == child_pid) {
					processTable[i].occupied = 0;
					break;
				}
			}
			if (WIFEXITED(status) || WIFSIGNALED(status)) { //add this if statement.
                		printf("OSS: Child exited, children_running = %d\n", children_running); 
            		}
		}

		// output the process table every half a second
		if (simClock->seconds != last_printed_seconds) {
			last_printed_seconds = simClock->seconds; // update  the last printed secodns
			fprintf(logfile != NULL ? logfile : stdout, "OSS PID: %d SysClockS: %d SysclockNano: %d\n", getpid(), simClock->seconds, simClock->nanoseconds);
            		fprintf(logfile != NULL ? logfile : stdout, "Process Table:\n");
            		fprintf(logfile != NULL ? logfile : stdout, "Entry\tOccupied\tPID\tStartS\tStartN\tMessages Sent\n");
            		for (int i = 0; i < 20; i++) {
                		fprintf(logfile != NULL ? logfile : stdout, "%d\t%d\t\t%d\t%d\t%d\t%d\n", i, processTable[i].occupied, processTable[i].pid, processTable[i].startSeconds, processTable[i].startNano, processTable[i].messagesSent);
            		}
			fprintf(logfile != NULL ? logfile : stdout, "\n");
			fflush(logfile != NULL ? logfile : stdout); //flush output
		}

		// Launch new child if conditions are met
        	if (children_running < simul_children) {
            		int current_time_ms = (simClock->seconds * 1000) + (simClock->nanoseconds / 1000000);
            		if (current_time_ms - last_launch_time >= intervalInMsToLaunchChildren) {
                		last_launch_time = current_time_ms;

                		//Generate random time limties for the worker
                		int max_seconds = rand() % timelimitForChildren + 1;
                		int max_nanoseconds = rand() % 1000000000;

				pid_t pid = fork();
                		if (pid == 0) {
					char max_seconds_str[20];
					char max_nanoseconds_str[20];
					sprintf(max_seconds_str, "%d", max_seconds);
					sprintf(max_nanoseconds_str, "%d", max_nanoseconds);
					execl("./worker", "worker", max_seconds_str, max_nanoseconds_str, NULL);
					perror("execl failed");
					exit(1);
				} else if (pid > 0) {
					total_processes_launched++; // increment total processes launched
					int slot_found = 0;
					for  (int j = 0; j < 20; j++) {
						if (processTable[j].occupied == 0) {
                            				processTable[j].occupied = 1;
                            				processTable[j].pid = pid;
                            				processTable[j].startSeconds = simClock->seconds;
                            				processTable[j].startNano = simClock->nanoseconds;
							processTable[j].messagesSent = 0;
                            				children_running++;
							slot_found = 1;
							break;
                            				//output process table after launch
							//fprintf(logfile != NULL ? logfile : stdout, "OSS PID: %d SysClockS: %d SysclockNano: %d\n", getpid(), simClock->seconds, simClock->nanoseconds);
                            				//fprintf(logfile != NULL ? logfile : stdout, "Process Table:\n");
                            				//fprintf(logfile != NULL ? logfile : stdout, "Entry\tOccupied\tPID\tStartS\tStartN\tMessages Sent\n");
                            				//for (int i = 0; i < 20; i++) {
                                			//	fprintf(logfile != NULL ? logfile : stdout, "%d\t%d\t\t%d\t%d\t%d\t%d\n", i, processTable[i].occupied, processTable[i].pid, processTable[i].startSeconds, processTable[i].startNano, processTable[i].messagesSent);
                            				//}
                            				//fprintf(logfile != NULL ? logfile : stdout, "\n");
                            				//fflush(logfile != NULL ? logfile : stdout);
                            				//break;
						}
					}
					if (!slot_found) {
						//no space in process table
						kill(pid, SIGTERM);
						fprintf(stderr, "No space in process table for new child\n");
					}
				} else { // fork has failed
					perror("fork failed");
					exit(1);
				}
			}
		}
	
		//Send messages to worker processes in round-robin fashion
        	if (children_running > 0) {
            		int process_index = next_process_index;
            		int attempts = 0;

		// find next valid process
		while (attempts < 20) {
			if (processTable[process_index].occupied == 1 && processTable[process_index].pid > 0) {
				//found valid process
				struct oss_message oss_msg;
				oss_msg.mtype = processTable[process_index].pid;
				oss_msg.command = 1;

				fprintf(logfile != NULL ? logfile : stdout, "OSS: Sending message to worker %d PID %d at time %d:%d\n",
					process_index, processTable[process_index].pid, simClock->seconds, simClock->nanoseconds);

				if (msgsnd(msgid, &oss_msg, sizeof(oss_msg) - sizeof(long), 0) == -1) {
					perror("msgsnd failed");
				} else {
					processTable[process_index].messagesSent++;
				} 
				break;
			}
			process_index = (process_index + 1) % 20;
			attempts++;
		}
		next_process_index = (process_index + 1) % 20;

		// Receive message from worker
		struct worker_message worker_msg;
		if (msgrcv(msgid, &worker_msg, sizeof(worker_msg) - sizeof(long), oss_pid, 0) == -1) {
			perror("msgrcv failed");
			// Add detailed error handling using errno (as previously discussed)
    			if (errno == EIDRM) {
        			fprintf(stderr, "OSS: Message queue was removed.\n");
    			} else if (errno == EINVAL) {
        			fprintf(stderr, "OSS: Invalid argument to msgrcv (e.g., invalid msgid or mtype).\n");
    			} else if (errno == ENOMSG) {
        			fprintf(stderr, "OSS: No message of the desired type was available.\n");
    			} else if (errno == EACCES) {
        			fprintf(stderr, "OSS: Permission denied to receive message.\n");
    			} else {
        			fprintf(stderr, "OSS: msgrcv failed with unknown error: %d\n", errno);
    			}
		} else {
			fprintf(logfile != NULL ? logfile : stdout, "OSS: Receiving message from worker %d PID %d at time %d:%d\n", process_index, processTable[process_index].pid, simClock->seconds, simClock->nanoseconds);
			if (worker_msg.status == 1) {
				fprintf(logfile != NULL ? logfile : stdout, "OSS: Worker %d PID %d is terminating.\n", process_index, processTable[process_index].pid);
				workers_completed++;
				printf("OSS: workers_completed = %d\n", workers_completed);
			}
			//add debug print statement
    			fprintf(stderr, "OSS: message recieved, mtype = %ld, status = %d\n", worker_msg.mtype, worker_msg.status);
		}

		//Check termination
		if (children_running == 0 && total_processes_launched >= num_children) {
			break;
		}
	} // end of while loop

	//Ending output here
	fprintf(logfile != NULL ? logfile : stdout, "OSS: Total processes launched: %d\n", total_processes_launched);
    	fprintf(logfile != NULL ? logfile : stdout, "OSS: Total messages sent: %d\n", total_messages_sent);
	
	//Detach shared memory when done
	shmdt(simClock);
	// Clean up the shared memory segment
        //Detach shared memory when done
	shmctl(shmid, IPC_RMID, NULL);
	// Remove Message queue
	if(workers_completed >= total_processes_launched) {
		msgctl(msgid, IPC_RMID, NULL);
	}

	if(logfile != NULL) {
		fclose(logfile);
	}
	return 0;
}			
