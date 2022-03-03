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

#define MSGSIZE 1024

typedef struct {
    long type;
    char text[MSGSIZE];
} msg;

void _msgsnd(int msqid, int dest, char c[MSGSIZE]) {
    // Messaggio
    msg _msg;
    // Costruzione del messaggio
    _msg.type = dest;
    memcpy(_msg.text, c, MSGSIZE);
    // Invio il messaggio
    if (msgsnd(msqid, &_msg, MSGSIZE, 0) == -1) {
        perror("Errore nell'invio del messaggio da parte del processo Padre");
        exit(1);
    }
}

msg _msgrcv(int msqid, int dest) {
    // Messaggio
    msg _msg;
    // Il processo chiamante rimane in attesa il messaggio
    if (msgrcv(msqid, &_msg, MSGSIZE, dest, 0) == -1) {
        perror("Errore nella ricezione del messaggio da parte del processo n");
        exit(1);
    }
    return _msg;
}

int num_files(const char * dir_pathname) {
    int file_desc, counter = 0;
    DIR * dir;
    struct dirent * in;
    struct stat statbuf;
    // Apro la directory
    if ((dir = opendir(dir_pathname)) == NULL) {
        perror("Errore nell'apertura della directory");
        exit(1);
    }
    // Leggo i file contenuti all'interno della directory
    while ((in = readdir(dir)) != NULL) {
        // Noto se il file è regolare
        if ((stat(in->d_name, &statbuf)) && (S_ISREG(statbuf.st_mode) == 1)) {
            counter++;
        }
    }
    return counter;
}
int total_size(const char * dir_pathname) {
    int file_desc, counter = 0;
    DIR * dir;
    struct dirent * in;
    struct stat statbuf;
    // Apro la directory
    if ((dir = opendir(dir_pathname)) == NULL) {
        perror("Errore nell'apertura della directory");
        exit(1);
    }
    // Leggo i file contenuti all'interno della directory
    while ((in = readdir(dir)) != NULL) {
        // Noto se il file è regolare
        if ((stat(in->d_name, &statbuf)) && (S_ISREG(statbuf.st_mode) == 1)) {
            counter += statbuf.st_size;
        }   
    }
    return counter;
}
int search_char(const char * dir_pathname, char * file, char c) {
    int file_desc, counter = 0;
    DIR * dir;
    struct dirent * in;
    struct stat statbuf;
    char * map;
    // Apro la directory
    if ((dir = opendir(dir_pathname)) == NULL) {
        perror("Errore nell'apertura della directory");
        exit(1);
    }
    // Leggo i file contenuti all'interno della directory
    while ((in = readdir(dir)) != NULL) {
        if (strcmp(in->d_name, file) == 0) {
            // E' stata trovata corrispondenza
            

            // Apro il file specifico
            if ((file_desc = open(in->d_name, O_RDONLY)) == -1) {
                perror("Errore nell'apertura del file");
                exit(1);
            }
            fstat(file_desc, &statbuf);
            // Map del file in memoria
            if ((map = mmap(NULL, statbuf.st_size, PROT_READ, MAP_SHARED, file_desc, 0)) == MAP_FAILED) {
                perror("Errore nella mappatura del file");
                exit(2);
            }

            for (int i = 0; i < statbuf.st_size; i++) {
                if (map[i] == c) {
                    counter++;
                }
            }
        }
    }
    return counter;
}

int main(int argc, char const *argv[]) {
    int msqid;

    if ((msqid = msgget(IPC_PRIVATE, IPC_CREAT | 0600)) == -1) {
        perror("Errore nella creazione della coda");
        exit(1);
    }

    
    for (int i = 1; i < argc; i++) {
        // Prima di creare il processo figlio, controllo la correttezza della directory
        struct stat statbuf;
        if ((stat(argv[i], &statbuf)) && (!S_ISDIR(statbuf.st_mode))) {
            printf("La directory non è valida, passo alla prossima iterazione...\n");
        }
        // Il figlio i-esimo
        if (fork() == 0) {
            // Messaggio
            msg _msg;
            // Inizializzo una stringa per il risultato dell'operazione
            char str[MSGSIZE];

            while (1) {
                // Il processo attende un messaggio da parte del padre
                _msg = _msgrcv(msqid, i);

                // printf("Figlio %d: ha ricevuto il messaggio\n", i);
                if (strcmp(_msg.text, "quit") == 0) break;

                // Analisi del messaggio ricevuto dal padre
                char * token = strtok(_msg.text, " ");
                if (strcmp(token, "num-files") == 0) {
                    // Richiamo la funzione
                    int _ = num_files(argv[i]);
                    sprintf(str, "%d", _);
                    strcat(str, " file");
                } else if (strcmp(token, "total-size") == 0) {
                    // Richiamo la funzione
                    int _ = total_size(argv[i]);
                    sprintf(str, "%d", _);
                    strcat(str, " byte");
                } else if (strcmp(token, "search-char") == 0) {
                    token = strtok(NULL, " "); // Il parametro n
                    char * file = strtok(NULL, " "); // Il file
                    token = strtok(NULL, " "); // Il carattere
                    int _ = search_char(argv[i], file, token[0]);
                    sprintf(str, "%d", _);
                } else {
                    perror("Comando non riconosciuto!");
                    exit(1);
                }
                // Invio il risultato al processo padre 
                _msgsnd(msqid, argc, str);
                // printf("Figlio %d: ha inviato il messaggio al padre\n", i);
            }
            exit(0);
        }
    }
    // Processo padre
    do {
        char c[MSGSIZE], str[MSGSIZE];
        char * end;
        msg _msg;
        printf("> ");
        fgets(c, MSGSIZE, stdin);
        strcpy(str, c);
        int n;

        if (strcmp(c, "quit") == 0) break;
        // Analisi del comando inserito in input,
        // per ottenere il numero del processo
        char * token = strtok(c, " ");
        for (int i = 0; token != NULL; i++) {
            if ((n = (int)strtol(token, &end, 10)))
                break;
            token = strtok(NULL, " ");
        }

        // Invio il messaggio al processo destinatario
        _msgsnd(msqid, n, str);
        
        // printf("Padre: ha inviato il messaggio al figlio %d per il comando %s\n", n, str);
        sleep(2);

        _msg = _msgrcv(msqid, argc);
        // printf("Padre: ha ricevuto il messaggio dal figlio\n");

        printf("\t%s\n", _msg.text);
    } while (1);


    // Rimozione della coda
    msgctl(msqid, IPC_RMID, NULL);
    return 0;
}
