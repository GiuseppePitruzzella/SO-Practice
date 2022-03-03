#include <stdio.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#define CHILDREN 27
#define SIZE 1024
#define NSEM 3
#define OFFSET 97

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
    // Controllo parametri
    if (argc < 2) {
        printf("Errore: numero di parametri insufficienti!");
        exit(1);
    }
    // // Creo un segmento di memoria condiviso
    if ((shmid = shmget(IPC_PRIVATE, SIZE, IPC_CREAT | 0600)) == -1) {
        perror("Errore nella creazione del segmento di memoria condiviso");
        exit(1);
    }
    // Creo i due semafori (KEY, num_sem, flags)
    if ((semid = semget(IPC_PRIVATE, NSEM, IPC_CREAT | 0600)) == -1) {
        perror("Errore nella creazione del primo semaforo");
        exit(1);
    }
    // Imposta il valore iniziale del semaforo a 0
    if (semctl(semid, 0, SETVAL, 0) == -1) {
        perror("semctl");
        exit(1);
    }
    
    
    // -------------
    // Processi L(i)
    // -------------

    for (int i = 0; i < CHILDREN; i++) {
        if (fork() == 0) {
            char * str;
            // Annetto l'area di memoria condivisa
            if ((str = (char *)shmat(shmid, NULL, 0)) == (char *)-1) {
                perror("Errore nell'annessione la memoria condivisa");
                exit(1);
            }

            while (1) {
                WAIT(semid, 0);
                if (strcmp(str, "quit") == 0) break;
                int counter = 0;
                for (int j = 0; j < SIZE; j++) 
                    if ((int)str[j] == i + OFFSET)
                        counter++;
                printf("L[%c]: ho trovato un numero di %c uguale a %d\n", (char)(i + OFFSET), (char)(i + OFFSET), counter);
                SIGNAL(semid, 0);
                sleep(1);
            }
            exit(0);
        }
    }

    // // -----------
    // // Processo S
    // // -----------
    // if (fork() == 0) {
    //     char * str;
    //     // Annetto l'area di memoria condivisa
    //     if ((str = (char *)shmat(shmid, NULL, 0)) == (char *)-1) {
    //         perror("Errore nell'annessione la memoria condivisa");
    //         exit(1);
    //     }
    //     int alpha_counter = 0;
    //     while (1) {
    //         WAIT(semid, 1);
    //         if (strcmp(str, "quit") == 0) break;
    //         printf("S\n");
    //         SIGNAL(semid, 2);
    //     }
    //     exit(0);
    // }

    // -----------
    // Processo P
    // -----------
    int i = 0, row_index = 0;
    char * str;
    // Annetto l'area di memoria condivisa
    if ((str = (char *)shmat(shmid, NULL, 0)) == (char *)-1) {
        perror("Errore nell'annessione la memoria condivisa");
        exit(1);
    }
    // Apertura del file in input
    FILE * in;
    if ((in = fopen(argv[1], "r")) == NULL) {
        perror("Errore nell'apertura dello stream");
        exit(1);
    }

    while (1) {
        str[i] = fgetc(in);
        if (str[i] == '\n') { // Se il carattere ricevuto è '\n', allora abbiamo completato la riga,
            // Stampo la riga i-esima sullo standard output
            printf("riga n.%d: ", row_index++);
            for (int j = 0; j < i; j++) 
                printf("%c", str[j]);
            printf("\n");
            str[i] = '\0';

            // ------------------------------------------------------------------------------------------------

            // Effettuo una SIGNAL sul primo semaforo per risvegliare tutti i processi figli L_i
            SIGNAL(semid, 0);
            sleep(1);
            WAIT(semid, 1);
            // Effettuo una WAIT. Attendo una SIGNAL da parte del processo figlio S  
            // WAIT(semid, 2);
            // sleep(1);

            // ------------------------------------------------------------------------------------------------

            // Azzero il segmento di memoria condivisa
            for (int j = 0; j < SIZE; j++)
                str[j] = '0';
            i = -1;
        } else if (str[i] == EOF) {
            // Se l'i-esimo carattere è anche l'ultimo carattere, allora eseguiamo il tutto un ultima volta
            printf("riga n.%d: ", row_index);
            for (int j = 0; j < i; j++) 
                printf("%c", str[j]);
            printf("\n");
            str[i] = '\0';

            // ------------------------------------------------------------------------------------------------

            // Effettuo una SIGNAL sul primo semaforo per risvegliare tutti i processi figli L_i
            SIGNAL(semid, 0);
            sleep(1);


            // Effettuo una WAIT. Attendo una SIGNAL da parte del processo figlio S  
            // WAIT(semid, 2);


            // Inserisco nel segmento di memoria condivisa il messaggio terminatore
            memcpy(str, "quit", 1024);
            // Effettuo un ultima SIGNAL sul primo semaforo per risvegliare tutti i processi figli L_i
            SIGNAL(semid, 0);

            // ------------------------------------------------------------------------------------------------

            break;
        }
        i++;
    }


    // Rimozione del segmento di memoria condiviso
    semctl(semid, 0, IPC_RMID, 0);
    // Rimozione del semaforo
    shmctl(shmid, IPC_RMID, NULL);
    return 0;
}
