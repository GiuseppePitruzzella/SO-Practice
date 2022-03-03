/* L'esercizio non è corretto al 100%. Infatti, non rispetta una delle feature richieste dell'esame,
    ovvero quella di analizzare/operare 10 dati alla volta. */

#include <stdio.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

typedef struct {
    int num;
    int type;
} item;

int WAIT(int sem_des, int num_semaforo){
      struct sembuf operazioni[1] = {{num_semaforo,-1,0}};
      return semop(sem_des, operazioni, 1);
}

int SIGNAL(int sem_des, int num_semaforo){
      struct sembuf operazioni[1] = {{num_semaforo,+1,0}};
      return semop(sem_des, operazioni, 1);
}

int main(int argc, char const *argv[]) {
    int semid, shmid;
    // Controllo dei parametri
    if (argc < 3) {
        printf("Numero di parametri insufficiente");
        exit(1);
    }
    // Inizializzo il semaforo
    if ((semid = semget(IPC_PRIVATE, 2, IPC_CREAT | 0600)) == -1) {
        perror("Errore nella creazione del semaforo");
        exit(1);
    }
    // Set dei valori per il semaforo
    semctl(semid, 0, SETVAL, 0);
    semctl(semid, 1, SETVAL, 0);
    

    // Inizializzo la memoria condivisa
    if ((shmid = shmget(IPC_PRIVATE, 10, IPC_CREAT | 0600)) == -1) {
        perror("Errore nella creazione dell'area di memoria condivisa");
        exit(1);
    }

    // ------------
    // Processo Mod
    // ------------
    if (fork() == 0) {
        item * shm;
        int m = atoi(argv[2]);
        // Attacco la memoria condivisa
        if ((shm = (item *)shmat(shmid, NULL, 0)) == (item *)-1) {
            perror("Errore nell'attaccare la memoria condivisa al processo");
            exit(1);
        }
        // Il processo figlio aspetta che vi siano dieci elementi
        WAIT(semid, 0);
        for (int i = 0; shm[i].type != -1; i++) {
            shm[i].num = shm[i].num % m;
            shm[i].type = 1;
        }
        SIGNAL(semid, 1);
        exit(0);
    }

    // ------------
    // Processo Out
    // ------------
    if (fork() == 0) {
        item * shm;
        // Attacco la memoria condivisa
        if ((shm = (item *)shmat(shmid, NULL, 0)) == (item *)-1) {
            perror("Errore nell'attaccare la memoria condivisa al processo");
            exit(1);
        }
        WAIT(semid, 1);
        for (int i = 0; shm[i].type != -1; i++)
            printf("(%d, %d)\t", shm[i].num, shm[i].type);
        exit(0);
    }
    // ------------
    // Processo P
    // ------------

    // Attacco la memoria condivisa
    int file_desc;
    item * shm;
    struct stat statbuf;
    if ((shm = (item *)shmat(shmid, NULL, 0)) == (item *)-1) {
        perror("Errore nell'attaccare la memoria condivisa al processo");
        exit(1);
    }
    // Apro il file
    if ((file_desc = open(argv[1], O_RDONLY)) == -1) {
        perror("Errore nell'apertura del file");
        exit(1);
    }
    fstat(file_desc, &statbuf);
    if (!S_ISREG(statbuf.st_mode)) {
        perror("Il file non è regolare");
        exit(1);
    }
    FILE * in;
    if ((in = fdopen(file_desc, "r")) == NULL) {
        perror("Errore nell'apertura dello stream");
        exit(1);
    }
    int i = 0;
    while (fscanf(in, "%d", &shm[i].num) != EOF) {
        shm[i++].type = 0;
        // printf("SHM[%d]: (%d, %d)\n", i, shm[i].num, shm[i].type);
    }
    shm[i].type = -1; // Num terminatore
    SIGNAL(semid, 0); // Effettuo una SIGNAL verso Mod



    return 0;
}
