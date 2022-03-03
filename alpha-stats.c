#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <string.h>
#include <sys/types.h> 
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/sem.h>

#define ALPHASIZE 26
#define UPCASE_OFF 65
#define LOWCASE_OFF 97



int main(int argc, char const *argv[]) {
    int shmid, semid;
    int * alpha;
    // Creazione dell'area di memoria condivisa
    if ((shmid = shmget(IPC_PRIVATE, ALPHASIZE, IPC_CREAT | 0600)) == -1) {
        perror("Errore nella creazione/apertura dell'area di memoria condivisa");
        exit(1);
    }
    // Annetto l'area di memoria condivisa
    if ((alpha = (int *)shmat(shmid, NULL, 0)) == (int *)-1) {
        perror("Errore nel map dell'area condivisa per il padre");
        exit(1);
    }
    // Creo il semaforo
    if ((semid = semget(IPC_PRIVATE, ALPHASIZE, IPC_CREAT | 0600)) == -1) {
        perror("Errore nella creazione/apertura del semaforo");
    }
    // Set a 0 i valori del semaforo
    if (semctl(semid, 0, SETVAL, 0) == -1) {
        perror("semctl");
        exit(1);
    }

    for (int i = 1; i < argc; i++) {
        // Processo figlio (i-esimo)
        if (fork() == 0) {
            char * map;
            int * alpha;
            int file_desc;
            // Init della SIGNAL
            struct sembuf SIGNAL[1] = { {0, +1, 0} };
            // Annetto l'area di memoria condivisa
            if ((alpha = (int *)shmat(shmid, NULL, 0)) == (int *)-1) {
                perror("Errore nel map dell'area condivisa per il padre");
                exit(1);
            }


            for (int j = 0; j < ALPHASIZE; j++) alpha[j] = 0;
            
            if ((file_desc = open(argv[i], O_RDONLY)) == -1) {
                perror("Errore nell'apertura del file");
                exit(1);
            }
            struct stat statbuf;
            fstat(file_desc, &statbuf);
            if ((map = mmap(NULL, statbuf.st_size, PROT_READ, MAP_SHARED, file_desc, 0)) == MAP_FAILED) {
                perror("Errore nella mappatura del file");
                exit(1);
            }
            
            for (int j = 0; j < statbuf.st_size; j++) {
                if ((map[j] >= 'a' && map[j] <= 'z')) {
                    alpha[map[j] - LOWCASE_OFF]++;
                    usleep((rand() % 1000) + 1000);
                }
                if ((map[j] >= 'A' && map[j] <= 'Z')) {
                    // printf("%c ", map[j]);
                    alpha[map[j] - UPCASE_OFF]++;
                    usleep((rand() % 1000) + 1000);
                }

            }
            semop(semid, SIGNAL, 1);
            // Stacco l'area di memoria condivisa
            shmdt(alpha);
            exit(0);
        }
    }
    // ---------------
    // Processo padre
    // ---------------
    struct sembuf SIGNAL[1] = { {0, +1, 0} };
    struct sembuf WAIT[1] = { {0, -1, 0} };
    int size;

    for (int i = 1; i < argc; i++) {
        semop(semid, WAIT, 1);
        for (int j = 0; j < ALPHASIZE; j++) size += alpha[j];
    }
    for (int i = 0; i < ALPHASIZE; i++) {
        printf("%c: %f%%\n", i + 97, (double)(alpha[i] * 100) / size);
    }

    // Distruggo rispettivamente l'area di memoria condivisa ed il semaforo
    shmctl(shmid, IPC_RMID, NULL);
    semctl(semid, 0, IPC_RMID, 0);
    return 0;
}
