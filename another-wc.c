#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <unistd.h>

#define KEY 5
#define SEM_KEY 11
#define DIM_MSG 1

int main(int argc, char const *argv[]) {
    // Inizializzazione degli id per area di memoria condivisa e semaforo
    int shm_id, sem_id;

    // Creazione dell'area di memoria condivisa
    if ((shm_id = shmget(KEY, DIM_MSG, IPC_CREAT | 0600)) == -1) {
        perror("Errore nella creazione della memoria condivisa");
        exit(1);
    }

    // Crea il semaforo per segnalare da disponibilita' di messaggi
    if ((sem_id = semget(IPC_PRIVATE, 1, IPC_CREAT | 0600)) == -1) {
        perror("semget");
        exit(1);
    }

    // Imposta il valore iniziale del semaforo ad 0
    if (semctl(sem_id, 0, SETVAL, 0) == -1) {
        perror("semctl");
        exit(1);
    }


    // Il processo Figlio
    if (fork() == 0) {
        FILE * in;
        int* wc;
        int * shm;

        // Inizializzazione del comando SIGNAL
        struct sembuf SIGNAL[1] = { { 0, +1, 0 } };

        // Map della memoria condivisa nello spazio di indirizzamento del processo Child, sia in lettura che in scrittura
        if ((shm = (int *)shmat(shm_id, NULL, 0)) == (int *)-1) {
            perror("Errore nell'annettere la memoria condivisa");
            exit(1);
        }

        // Apre il file sorgente in sola lettura.
        // Se è stato specificato alcun file, legge dallo standard input
        if (argc > 1) {
            if ((in = fopen(argv[1], "r")) == NULL) {
                perror(argv[1]);
                exit(1);
            }
        } else {
            in = stdin;
        }

        if (in == stdin) printf("Inserire le stringhe da copiare sulla memoria condivisa ('quit' per uscire): \n");
        
        // --------------------------------------------------------
        // Sostituire fgets() con fgetc(), il quale restituisce un solo byte
        // --------------------------------------------------------

        while ((shm[0] = fgetc(in)) != EOF) {
            printf("Messaggio scritto: %c\n", shm[0]);
            // Un messaggio è adesso presente all'interno dell'area di messaggi condivisa, motivo per cui invoco SIGNAL sul semaforo
            semop(sem_id, SIGNAL, 1);  
            sleep(1);
        }

        // Invio notifica di terminazione
        shm[0] = -1;
        printf("Messaggio finale scritto: %c\n", shm[0]);
        // Un messaggio è adesso presente all'interno dell'area di messaggi condivisa, motivo per cui invoco SIGNAL sul semaforo
        semop(sem_id, SIGNAL, 1);  

    } else { // Il processo Padre
        int * shm;
        int n_letters = 0, n_words = 0, n_rows = 0;
        // Inizializzazione del comando SIGNAL
        struct sembuf WAIT[1] = { { 0, -1, 0 } };

        // Map della memoria condivisa nello spazio di indirizzamento del processo Child, in sola lettura
        if ((shm = (int *)shmat(shm_id, NULL, SHM_RDONLY)) == (int *)-1) {
            perror("Errore nell'annettere la memoria condivisa");
            exit(1);
        }

        do {
            // Il processo padre è in attesa di un messaggio all'interno dell'area di memoria condivisa
            semop(sem_id, WAIT, 1);  
            printf("Messaggio letto: %c\n\n", shm[0]);   // legge e stampa il messaggio

            if ((shm[0] >= 'a') && (shm[0] <= 'z') || (shm[0] >= 'A') && (shm[0] <= 'Z')) n_letters++;
            if (isspace(shm[0])) n_words++;
            if (shm[0] == '\n') n_rows++;

        } while (shm[0] != -1);

        printf("%d\t%d\t%d\t", n_letters, n_words + 1, n_rows + 1);
        if (argc > 1) printf("%s\n", argv[1]);
        exit(0);
    }
    
    // Distrugge memoria condivisa e semaforo
    shmctl(shm_id, IPC_RMID, NULL);
    semctl(sem_id, 0, IPC_RMID, 0);
    return 0;
}
