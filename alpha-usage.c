// ------------------------------------------------------------------------------------------------------------------------------
// Modificare il figlio Analyzer, analizzando il file secondo una mappatura in memoeria dello stesso (guarda slides).
// ------------------------------------------------------------------------------------------------------------------------------

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

#define KEY 1
#define SCANNER 1
#define ANALYZER 2
#define PARENT 3
#define MSGSIZE 2048

typedef struct {
    long type;
    char text[MSGSIZE];
} msg;

void read_file(char * pathname, int msqid) {
    struct dirent * in;
    struct stat statbuf;
    char reg_path[MSGSIZE];
    msg _msg;

    printf("Scansione della cartella %s\n", pathname);

    DIR * directory;
    if ((directory = opendir(pathname)) == NULL) {
        perror("Errore nell'apertura della directory");
        return;
    }

    // Analizzo ricorsivamente i file contenuti all'interno della directory
    in = readdir(directory);
    while ((in = readdir(directory)) != NULL) {
        // printf("%s\n", in->d_name);

        // Effettuo l'analisi per il singolo file
        stat(in->d_name, &statbuf);
        if (S_ISREG(statbuf.st_mode) == 1) {
            printf("Scanner: %s/%s\n", pathname, in->d_name);

            // Costruzione del messaggio per Analyzer
            // Set del destinatario
            _msg.type = ANALYZER;
            // Concatenuto le due stringhe per formare il pathname del file
            strcpy(reg_path, pathname);
            strcat(reg_path, "/");
            strcat(reg_path, in->d_name);
            // Set del path per il messaggio
            memcpy(_msg.text, reg_path, MSGSIZE);

            // Invio del messaggio ad Analyzer
            if (msgsnd(msqid, &_msg, MSGSIZE, 0) == -1) {
                perror("Errore nell'invio del messaggio da parte di Scanner");
                exit(1);
            }
            // printf("Scanner: Ho inviato il messaggio per Analyzer!\n");
        } else if (strcmp(in->d_name, "..") != 0 && S_ISDIR(statbuf.st_mode) == 1) {
            read_file(in->d_name, msqid);
        }
        sleep(2);
    }

    closedir(directory);
}

int main(int argc, char const *argv[]) {
    // Inizializzo la variabile pathname per la directory
    char pathname[MSGSIZE];
    // Inizializzo la variabile id per la coda di messaggi    
    int msqid;
    // Controllo dei parametri ed apertura della directory
    int reg_file_counter = 0;

    // Controllo dei parametri e set della directory
    if (argc < 2) strcpy(pathname, ".");
    else strcpy(pathname, argv[1]);

    // Creazione / Apertura della coda di messaggi
    if ((msqid = msgget(KEY, IPC_CREAT | 0660)) == -1) {
        perror("Errore nella creazione della coda di messaggi");
        exit(1);
    }

    // Processo Scanner
    if (fork() == 0) {
        msg _msg;
        // Lettura ricorsiva dei file regolari all'interno della cartella
        read_file(pathname, msqid);

        // Invio del messaggio di chiusura per Analyzer
        _msg.type = ANALYZER;
        strcpy(_msg.text, "quit");
        if (msgsnd(msqid, &_msg, MSGSIZE, 0) == -1) {
            perror("Errore nell'invio del messaggio da parte di Analyzer");
            exit(1);
        }
        // printf("Scanner: Ho inviato il messaggio finale ad Analyzer!\n");
        exit(0);
    }

    // Processo Analyzer
    if (fork() == 0) {
        // Inizializzo la variabile per lo stream
        FILE * in;
        
        char c;
        char n_letters_text[MSGSIZE];
        msg _msg;
        // Descrittore per il file
        int file_desc;
        // Struct stat per ottenere la dimensione totale del file
        struct stat statbuf;
        // Area di memoria mappata per il file
        char * map; 

        do {
            // Controllo ricezione messaggi
            if (msgrcv(msqid, &_msg, MSGSIZE, ANALYZER, 0) == -1) {
                perror("Errore nella ricezione dei messaggi da parte di Analyzer");
                continue;
            }
            // printf("Analyzer: Ho ricevuto il messaggio da parte di Scanner!\n");

            if (strcmp(_msg.text, "quit") == 0) break;
            int n_letters = 0;

            if ((file_desc = open(_msg.text, O_RDONLY)) == -1) {
                perror("Errore nell'apertura del file regolare");
                exit(1);
            }

            if (fstat(file_desc, &statbuf) == -1) {
                perror("Errore in fstat");
                exit(1);
            }

            if ((map = mmap(NULL, statbuf.st_size, PROT_READ, MAP_SHARED, file_desc, 0)) == MAP_FAILED) {
                perror("Errore nella mappatura del file");
                exit(2);
            }

            for (int i = 0; i < statbuf.st_size; i++) {
                if (map[i] >= 'a' && map[i] <= 'z' || map[i] >= 'A' && map[i] <= 'Z') {
                    n_letters++;
                    // printf("Ho trovato una lettera! %c\n", map[i]);
                }
            }

            // Stampa a video
            printf("%s <%d>\n", _msg.text, n_letters);

            // Invio del messaggio al padre
            _msg.type = PARENT;
            sprintf(n_letters_text, "%d", n_letters);
            strcpy(_msg.text, n_letters_text);
            
            if (msgsnd(msqid, &_msg, MSGSIZE, 0) == -1) {
                perror("Errore nell'invio del messaggio da parte di Analyzer");
                exit(1);
            }
            sleep(1);
        } while (1);

        _msg.type = PARENT;
        if (msgsnd(msqid, &_msg, MSGSIZE, 0) == -1) {
            perror("Errore nell'invio del messaggio da parte di Analyzer");
            exit(1);
        }
        // printf("Analyzer: Ho inviato il messaggio finale a Padre\n");
        exit(0);
    }
    // Processo padre
    msg _msg;
    long total = 0;
    char * n;
    // Controllo ricezione messaggi
    do {
        if (msgrcv(msqid, &_msg, MSGSIZE, PARENT, 0) == -1) {
            perror("Errore nella ricezione dei messaggi");
            exit(1);
        }
        // printf("Padre: Ho ricevuto il messaggio da parte di Analyzer!\n");

        // Se il messaggio ricevuto contiene la parola quit all'interno di path, allora il loop termina
        if (strcmp(_msg.text, "quit") == 0) break;

        // Incremento del numero totale di lettere all'interno del nuovo file regolare
        total += strtol(_msg.text, &n, 10);
        sleep(1);
    } while (1);
    printf("Padre: totale di <%ld> caratteri alfabetici", total);

    exit(0);

    // Rimozione della coda di messaggi
    msgctl(msqid, IPC_RMID, NULL);
    return 0;
}

