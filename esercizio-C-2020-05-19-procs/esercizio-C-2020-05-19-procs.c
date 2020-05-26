
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/mman.h>

#define N 10

int * countdown;
int * process_counter;
int * shutdown;

//mutex per regolare accesso concorrente alle variabili

sem_t * count_mutex;

void child_process();

int main() {

	int res;

	countdown = mmap(NULL, sizeof(int)*(N+2) + sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);

	if (countdown == MAP_FAILED) {
		perror("mmap()");
		exit(EXIT_FAILURE);
	}

	process_counter = &countdown[1];

	shutdown = &process_counter[N];

	count_mutex = (sem_t *)(&shutdown[1]);

	*countdown = 0;
	*shutdown = 0;

	res = sem_init(count_mutex,
			1, // 1 => il semaforo è condiviso tra processi,
			   // 0 => il semaforo è condiviso tra threads del processo
			1 // valore iniziale del semaforo (se mettiamo 0 che succede?)
		  );

	if(res==-1){
		perror("sem_init");
		exit(EXIT_FAILURE);
	}


	for(int i =0; i<N; i++){

		switch(fork()){
		case 0:
			child_process(i);
			break;
		case -1:
			perror("fork");
			exit(EXIT_FAILURE);
			break;
		default:
			;
		}

	}
	printf("ora dormo 1 sec\n");
	//dopo avere avviato i processi figli, il processo padre dorme 1 secondo
	//e poi imposta il valore di countdown al valore 100000.
	sleep(1);


	printf("setto countdown\n");
	if (sem_wait(count_mutex) == -1) {
			perror("sem_wait");
			exit(EXIT_FAILURE);
		}
	*countdown = 100000;

	if (sem_post(count_mutex) == -1) {
			perror("sem_post");
			exit(EXIT_FAILURE);
		}

	int countdown_copy=-1;

	while(countdown_copy != 0){

		if (sem_wait(count_mutex) == -1) {
				perror("sem_wait");
				exit(EXIT_FAILURE);
			}
		countdown_copy = *countdown;

		if(countdown_copy == 0){
			*shutdown=1;
		}

		if (sem_post(count_mutex) == -1) {
				perror("sem_post");
				exit(EXIT_FAILURE);
			}
	}

	for(int j=0; j<N; j++){
		if(wait(NULL) == -1){
			perror("wait()");
			exit(EXIT_FAILURE);
		}
	}

	for(int k=0; k<N; k++){
		printf("process_counter[%d] = %d\n", k, process_counter[k]);
	}


	return 0;
}

void child_process(int i){

	int shut_copy=0;
	while(shut_copy == 0){

		if (sem_wait(count_mutex) == -1) {
						perror("sem_wait");
						exit(EXIT_FAILURE);
					}

		if(*countdown > 0){

			(*countdown)--;
			//printf("countdown : %d\n", *countdown);
			process_counter[i]++;
		}

		shut_copy = *shutdown;

		if (sem_post(count_mutex) == -1) {
			perror("sem_post");
			exit(EXIT_FAILURE);
		}

	}
	exit(EXIT_SUCCESS);
}

