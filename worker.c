// Author: Tu Le
// CS4760 Project 3
// Date: 3/13/2025

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>
#include <sys/msg.h>

//Struct for the clock
typedef struct {
	int seconds;
	int nanoseconds;
} SimulatedClock;

// Message structures
struct oss_message {
	long mtype; // Message type (PID of the worker)
	int command; // Command for the worker
};

struct worker_message {
	long mtype; // Message type (PID of oss)
	int status; // Status from the worker 
};

int main(int argc, char **argv) {
	printf("Worker: Received args: %s %s %s\n", argv[1], argv[2], argv[3]); 
	if (argc != 4) {
		fprintf(stderr, "Usage: %s <max_seconds> <max_nanoseconds> <oss_pid>\n", argv[0]);
		exit(1);
	}

	//Get command-line arguments
	int max_seconds = atoi(argv[1]);
	int max_nanoseconds = atoi(argv[2]);
	pid_t oss_pid = atoi(argv[3]); // Get OSS PID from argument

	printf("Worker: Converted args: max_seconds=%d, max_nanoseconds=%d, oss_pid=%d\n", max_seconds, max_nanoseconds, oss_pid); 

	//Shared memory setup
	key_t key = ftok("oss.c", 1); // use the same key as oss.c
	if (key == -1) {
		perror("ftok");
		exit(1);
	}
	
	int shmid = shmget(key, sizeof(SimulatedClock), IPC_CREAT | 0666);
	if (shmid < 0) {
		perror("shmget");
		exit(1);
	}

	SimulatedClock *simClock = (SimulatedClock *)shmat(shmid, NULL, 0);
	if (simClock == (SimulatedClock *) -1) {
		perror("shmat");
		exit(1);
	}


	//Calculate the termination time
	int termination_seconds = simClock->seconds + max_seconds;
	int termination_nanoseconds = simClock->nanoseconds + max_nanoseconds;
	if (termination_nanoseconds >= 1000000000) {
		termination_seconds += 1;
        	termination_nanoseconds -= 1000000000;
	}

	//print initial message
	printf("WORKER PID: %d PPID: %d SysClockS: %d SysclockNano: %d TermTimeS: %d TermTimeNano: %d\n", getpid(), getppid(), simClock->seconds, simClock->nanoseconds, termination_seconds, termination_nanoseconds);
	printf("--Just Starting\n");
	fflush(stdout);
	int iteration_count = 0;

	// attach to message queue
	int msgid = msgget(key, 0666);
	if (msgid == -1) {
		perror("msgget in worker");
		exit(1);
	}
	printf("Worker %d: msgid = %d\n", getpid(), msgid);//add  print statement

	// main loop
	while (1) {
		// receive message from oss
		struct oss_message oss_msg;
		printf("Worker %d: Expecting mtype = %d\n", getpid(), getpid()); // expecting worker pid.
        	if (msgrcv(msgid, &oss_msg, sizeof(oss_msg) - sizeof(long), getpid(), 0) == -1) {
			perror("msgrcv failed");
			break;
		}
		printf("Worker %d: Received message with mtype = %ld\n", getpid(), oss_msg.mtype); 


		iteration_count++;

		//debugging
		printf("Worker %d: Current time %d:%d, Term time %d:%d\n", getpid(), simClock->seconds, simClock->nanoseconds, termination_seconds, termination_nanoseconds);

		//check termination
		if (simClock->seconds > termination_seconds || (simClock->seconds == termination_seconds && simClock->nanoseconds >= termination_nanoseconds)) {
			printf("WORKER PID: %d PPID: %d SysClockS: %d SysclockNano: %d TermTimeS: %d TermTimeNano: %d\n", getpid(), getppid(), simClock->seconds, simClock->nanoseconds, termination_seconds, termination_nanoseconds);
			printf("--Terminating after sending message back to oss after %d iterations.\n", iteration_count);
			fflush(stdout);

			//send termination message to oss
			struct worker_message worker_msg;
			worker_msg.mtype = getppid(); //OSS's PID
			worker_msg.status = 1; // Done
			printf("Worker %d: Sending termination message to OSS\n", getpid());
			printf("Worker %d: Sending mtype = %ld\n", getpid(), worker_msg.mtype); 

			if (msgsnd(msgid, &worker_msg, sizeof(worker_msg) - sizeof(long), 0) == -1) {
				perror("msgsnd (termination) failed");
            		}

			//clean up and exit
			shmdt(simClock);
			exit(0); // Terminate the worker
            		//break; //exit the loop
        	}

		//periodic output
		printf("WORKER PID: %d PPID: %d SysClockS: %d SysclockNano: %d TermTimeS: %d TermTimeNano: %d\n", getpid(), getppid(), simClock->seconds, simClock->nanoseconds, termination_seconds, termination_nanoseconds);
        	printf("--%d iterations have passed since it started\n", iteration_count);
        	fflush(stdout); //Flush output

		//send continue message to oss
		struct worker_message worker_msg;
		worker_msg.mtype = getppid();
		worker_msg.status = 0; // Continue
		printf("Worker %d: Sending mtype = %ld\n", getpid(), worker_msg.mtype);		
		if (msgsnd(msgid, &worker_msg, sizeof(worker_msg) - sizeof(long), 0) == -1) {
			perror("msgsnd failed");
			break;
		}
	}

	//Detach shared memory
	shmdt(simClock);
	return 0;
}
