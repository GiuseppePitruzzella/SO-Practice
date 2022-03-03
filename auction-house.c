#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <ctype.h>
#include <string.h>
#include <time.h>

#define SIZE 1024

typedef struct {
    int auction_index;
    char * name;
    int max, min;
    int max_offered;
    int winner_bidder_index, valid_offers, total_offers;
} auction;


int WAIT(int sem_des, int num_semaforo){
      struct sembuf operazioni[1] = {{num_semaforo,-1,0}};
      return semop(sem_des, operazioni, 1);
}
int SIGNAL(int sem_des, int num_semaforo){
      struct sembuf operazioni[1] = {{num_semaforo,+1,0}};
      return semop(sem_des, operazioni, 1);
}

int main(int argc, char const *argv[]) {
    int semid, shmid, children = atoi(argv[2]);
    // Controllo i parametri
    if (argc < 3) {
        printf("Errore, numero di parametri insufficiente");
        exit(1);
    }
    // Init dell'area di memoria condivisa
    if ((shmid = shmget(IPC_PRIVATE, SIZE, IPC_CREAT | 0600)) == -1) {
        perror("Errore nella creazione dell'area di memoria condivisa");
        exit(1);
    }
    // Init del semaforo
    if ((semid = semget(IPC_PRIVATE, children, IPC_CREAT | 0600)) == -1) {
        perror("Errore nella creazione del semaforo");
        exit(1);
    }
    // Set dei valori per il semaforo
    for (int i = 0; i < children; i++) semctl(semid, i, SETVAL, 0);

    

    for (int i = 0; i < children; i++) {
        // ------------------
        // Processo Offerenti
        // ------------------
        if (fork() == 0) {
            // Attacco l'area di memoria condivisa
            auction * shm;
            if ((shm = (auction *)shmat(shmid, NULL, 0)) == (auction *)-1) {
                perror("Errore nell'annessione dell'area di memoria condivisa");
                exit(1);
            }
            while (1) {
                WAIT(semid, i);
                if (shm->min == -1) 
                    break;
                srand(time(NULL));
                int offer = rand() % shm->max;
                if (offer >= shm->min) {
                    if (offer > shm->max_offered) {
                        shm->max_offered = offer;
                        shm->winner_bidder_index = i;
                    }
                    shm->valid_offers++;
                    shm->total_offers++;
                } else {
                    shm->total_offers++;
                }
                printf("B%d: Invio offerta di %d EUR per asta n.%d\n", i, offer, shm->auction_index);
                SIGNAL(semid, children);
                sleep(1);
            }
            // Stacco l'area di memoria condivisa
            shmdt(shm);
            exit(0);
        }
    }

    // --------------
    // Processo Padre
    // --------------

    // Attacco l'area di memoria condivisa
    auction * shm;
    if ((shm = (auction *)shmat(shmid, NULL, 0)) == (auction *)-1) {
        perror("Errore nell'annessione dell'area di memoria condivisa");
        exit(1);
    }

    FILE * in;
    if ((in = fopen(argv[1], "r")) == NULL) {
        perror("Errore nella creazione del file");
        exit(1);
    }

    char str[SIZE];
    int valid_auction = 0, total = 0;
    for (int k = 1; fgets(str, SIZE, in) != NULL; k++) {
        shm->auction_index = k;
        shm->name = strtok(str, ",");
        shm->min = atoi(strtok(NULL, ","));
        shm->max = atoi(strtok(NULL, "\n"));
        shm->max_offered = shm->total_offers = shm->valid_offers = shm->winner_bidder_index = 0;

        printf("J: Lancio asta n.%d per %s con offerta minima di %d EUR e massima di %d EUR\n", k, shm->name, shm->min, shm->max);

        for (int j = 0; j < children; j++) {
            SIGNAL(semid, j);
            sleep(1);
            WAIT(semid, children);
            printf("J: Ricevuta offerta da B%d\n", j);
            sleep(1);
        }

        if (shm->valid_offers > 0) {
            printf("J: L'asta n.%d per %s si è conclusa con %d offerte valide su %d; il vincitore è B%d che si aggiudica l'oggetto per %d EUR\n\n", k, shm->name, shm->valid_offers, shm->total_offers, shm->winner_bidder_index, shm->max_offered);
            valid_auction++;
            total += shm->max;
        } else {
            printf("J: L'asta n.%d per %s si è conclusa senza alcuna offerta valida pertanto l'oggetto non risulta assegnato\n\n", k, shm->name);
        }
    }
    shm->min = -1;
    for (int j = 0; j < children; j++) {
        SIGNAL(semid, j);
        sleep(1);
    }
    // Stacco l'area di memoria condivisa
    shmdt(shm);
    // Rimuovo l'area di memoria condivisa
    shmctl(shmid, 0, NULL);
    // Rimuovo i due semafori
    for (int i = 0; i < children; i++) semctl(semid, i, IPC_RMID, NULL);
    return 0;
}
