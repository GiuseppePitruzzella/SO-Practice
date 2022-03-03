#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>

#define MSGSIZE 1024
#define READ 0
#define WRITE 1

typedef struct {
    long type;
    char text[MSGSIZE];
} msg;

int main(int argc, char const *argv[]) {
    int _pipe[2];
    int msqid;
    if (pipe(_pipe) == -1) {
        perror("Errore nella creazione della pipe");
        exit(1);
    }
    if ((msqid = msgget(IPC_PRIVATE, IPC_CREAT | 0600)) == -1) {
        perror("Errore nella creazione della coda di messaggi");
        exit(1);
    }

    // -----------
    // Processo R
    // -----------
    if (fork() == 0) {
        char * map;
        int file_desc;
        struct stat statbuf;
        if ((file_desc = open(argv[2], O_RDONLY)) == -1) {
            perror("Errore nell'apertura del file");
            exit(1);
        }
        if (fstat(file_desc, &statbuf) == -1) {
            perror("Errore in fstat()");
            exit(1);
        }
        if (!S_ISREG(statbuf.st_mode)) {
            perror("Il file non è regolare");
            exit(1);
        }
        if ((map = (char *)mmap(NULL, statbuf.st_size, PROT_READ, MAP_SHARED, file_desc, 0)) == MAP_FAILED) {
            perror("Errore nella mappatura del file");
            exit(1);
        }

        int i = 0;
        for (; i < statbuf.st_size; i++) {
            char str[MSGSIZE];
            int j = 0;
            while (map[i] != '\n' && i < statbuf.st_size) {   
                str[j++] = map[i++];
            }
            str[j] = '\0';
            // printf("R: Invio a P il messaggio '%s'\n", str);
            write(_pipe[WRITE], str, MSGSIZE);
            sleep(1);
        }
        write(_pipe[WRITE], "quit", MSGSIZE);
        exit(0);
    }

    // -----------
    // Processo W
    // -----------
    if (fork() == 0) {
        msg _msg;
        do {
            if (msgrcv(msqid, &_msg, MSGSIZE, 0, 0) == -1) {
                perror("Errore nella ricezione del messaggio");
                exit(1);
            }
            if (strcmp(_msg.text, "quit") == 0) break;
            printf("%s\n", _msg.text);
        } while (1);
        exit(0);
    }

    // -----------
    // Processo P
    // -----------
    char str[MSGSIZE];
    char c[MSGSIZE];
    msg _msg;
    memcpy(c, argv[1], MSGSIZE);
    do {
        // printf("P: Ho ricevuto da R la stringa %s\n", str);
        if (read(_pipe[READ], str, MSGSIZE) == -1) {
            perror("Errore nella lettura del messaggio nella pipe");
            exit(1);
        }

        if (strcmp(str, "quit") == 0) break;

        if (strstr(str, c) != NULL) {
            // printf("P: Ho trovato corrispondenza, invio il messaggio\n");
            // Costruisco il messaggio;
            _msg.type = 1;
            memcpy(_msg.text, str, MSGSIZE);
            if (msgsnd(msqid, &_msg, MSGSIZE, 0) == -1) {
                perror("Errore nell'invio del messaggio");
                exit(1);
            }
            sleep(1);
        }
    } while (1);

    _msg.type = 1;
    memcpy(_msg.text, "quit", MSGSIZE);
    if (msgsnd(msqid, &_msg, MSGSIZE, 0) == -1) {
        perror("Errore nell'invio del messaggio");
        exit(1);
    }
    sleep(1);
    msgctl(msqid, IPC_RMID, NULL);
    exit(0);
}