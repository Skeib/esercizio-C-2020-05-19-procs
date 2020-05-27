#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

#include <errno.h>
#include <semaphore.h>

/*
N = 10

un processo padre crea N processi figli ( https://repl.it/@MarcoTessarotto/crea-n-processi-figli )

shared variables: countdown, process_counter[N], shutdown

usare mutex per regolare l'accesso concorrente a countdown

dopo avere avviato i processi figli, il processo padre dorme 1 secondo e
poi imposta il valore di countdown al valore 100000.

quando countdown == 0, il processo padre imposta shutdown a 1.

aspetta che terminino tutti i processi figli e poi stampa su stdout process_counter[].

i processi figli "monitorano" continuamente countdown:

    processo i-mo: se countdown > 0, allora decrementa countdown ed incrementa process_counter[i]

    se shutdown != 0, processo i-mo termina



è un problema del tipo "produttore-consumatore"
Little Book of Semaphores, pag. 55-59
 */

#define N 10

#define INIT_VALUE 100000

typedef struct {
	sem_t sem; // semaforo per implementare il meccanismo di mutual exclusion (mutex)
	int val; // variabile condivisa
} shared_int;


shared_int * countdown;


int * process_counter; // int [N]

int * shutdown;


#define CHECK_ERR(a,msg) {if ((a) == -1) { perror((msg)); exit(EXIT_FAILURE); } }

#define CHECK_ERR_MMAP(a,msg) {if ((a) == MAP_FAILED) { perror((msg)); exit(EXIT_FAILURE); } }

void child_process(int i);

int main() {

	int res;

	countdown = mmap(NULL, // NULL: è il kernel a scegliere l'indirizzo
			sizeof(shared_int) + sizeof(int) * (N+1), // dimensione della memory map
			PROT_READ | PROT_WRITE, // memory map leggibile e scrivibile
			MAP_SHARED | MAP_ANONYMOUS, // memory map condivisibile con altri processi e senza file di appoggio
			-1, // nessun file di appoggio alla memory map
			0); // offset nel file
	CHECK_ERR_MMAP(countdown,"mmap")


	process_counter = (int *) (&countdown[1]);

	shutdown = &process_counter[N];

	printf("sizeof(shared_int) = %ld\n", sizeof(shared_int));
	printf("countdown = %p\n", countdown);
	printf("process_counter = %p\n", process_counter);
	printf("shutdown = %p\n", shutdown);
	printf("\n");

	printf("[parent] countdown->val initial value: %d\n", countdown->val);

	res = sem_init(&countdown->sem,
					1, // 1 => il semaforo è condiviso tra processi, 0 => il semaforo è condiviso tra threads del processo
					1 // valore iniziale del semaforo (se mettiamo 0 che succede?)
				  );

	CHECK_ERR(res,"sem_init")

	// voglio creare N processi figli

	for (int i = 0; i < N; i++) {
		switch (fork()) {
		  case 0: // child process
			child_process(i);
		  case -1:
			perror("fork");
			exit(EXIT_FAILURE);
		  default:
			;
		}
	}

	printf("[parent] before sleep 1 second\n");
	sleep(1);

	printf("[parent] before set countdown->val=%d\n", INIT_VALUE);
	// 3.4.2 Mutual exclusion solution, pag. 19
	if (sem_wait(&countdown->sem) == -1) {
		perror("sem_wait");
		exit(EXIT_FAILURE);
	}

	countdown->val = INIT_VALUE;

	if (sem_post(&countdown->sem) == -1) {
		perror("sem_post");
		exit(EXIT_FAILURE);
	}

	int countdown_val_copy = -1;
	while (countdown_val_copy != 0) {
		if (sem_wait(&countdown->sem) == -1) {
			perror("sem_wait");
			exit(EXIT_FAILURE);
		}

		countdown_val_copy = countdown->val;

		if (countdown_val_copy == 0) {
			*shutdown = 1; // comunichiamo a tutti i processi figli di terminare
		}

		if (sem_post(&countdown->sem) == -1) {
			perror("sem_post");
			exit(EXIT_FAILURE);
		}
	}

	printf("[parent] after while, countdown->val=%d\n", countdown->val);

	while (wait(NULL) != -1) ; // aspettiamo la terminazione di tutt i processi figli

	res = sem_destroy(&countdown->sem); // ok per distruggere il semaforo
	CHECK_ERR(res,"sem_destroy")

	long sum = 0;
	for (int i = 0; i < N; i++) {
		printf("[parent] process_counter[%d] = %d\n", i, process_counter[i]);

		sum += process_counter[i];
	}

	printf("[parent] sum of process_counter[] = %ld\n", sum);

	printf("[parent] bye\n");

	return 0;
}

void child_process(int proc_id) {
/*
i processi figli "monitorano" continuamente countdown:

    processo i-mo: se countdown > 0, allora decrementa countdown ed incrementa process_counter[i]

    se shutdown != 0, processo i-mo termina
 */

	//printf("proc_id %d started\n", proc_id);

	int shutdown_copy = 0;

	while (shutdown_copy == 0) {
		if (sem_wait(&countdown->sem) == -1) {
			perror("sem_wait");
			exit(EXIT_FAILURE);
		}

		if (countdown->val > 0) {
			countdown->val--;
			process_counter[proc_id]++;
		}

		shutdown_copy = *shutdown;

		if (sem_post(&countdown->sem) == -1) {
			perror("sem_post");
			exit(EXIT_FAILURE);
		}
	}

	//printf("proc_id %d terminating\n", proc_id);

	exit(EXIT_SUCCESS);
}

