#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#define BUFSIZE 1024
#define KEY 9
#define R_TYPE 1
#define P_TYPE 2
#define W_TYPE 3

typedef struct {
    long type;
    char text[BUFSIZE];
} message;

// Palindrome() prende in input una singola parola,
// ritornando 0 se quest'ultima è palindroma. Altrimenti, -1.
int palindrome(char *str) {
    int start = 0;
    int end = strlen(str) - 1;
    while (start < end) {
        if (str[start++] != str[end--])
            return 0; // Se non viene trovata corrispondenza, ritorna 1
    }
    return 1;
}

int main(int argc, char const *argv[]) {
    int msg_queue_id;
    message msg_r, msg_p, msg_w;
    char buffer[BUFSIZE];
    int n_bytes;

    // Controllo dei parametri
    if (argc < 2) {
        printf("Parametri insufficienti!");
        exit(1);
    }

    // Inizializzo la coda di messaggi
    if ((msg_queue_id = msgget(KEY, IPC_CREAT | IPC_EXCL | 0660)) == -1) {
        perror("Errore nella crezione della coda");
        exit(1);
    }

    // ---------------
    // Processo R
    // ---------------
    
    /**
     * Processo R
     * Lavora su un testo ricevuto mediante file o stdin, scansionando
     * questo parola per parola, le quali verrano inserite (in messaggi distinti)
     * all'interno di una pipe tra R e Padre.
     */
    if (fork() == 0) {
        int file_desc, size, result;
        char buffer[BUFSIZE];
        char *str;

        // Apre il file sorgente in sola lettura
        if ((file_desc = open(argv[1], O_RDONLY)) == -1) {
            perror(argv[1]);
            exit(1);
        }

        // Copia i dati dalla sorgente alla destinazione
        do {

            // Legge dal file fino ad un massimo di BUFSIZE byte dal file
            if ((size = read(file_desc, buffer, BUFSIZE)) == -1) {
                perror(argv[1]);
                exit(1);
            }

            // Leggo il messaggio ricevuto ed inserisco il contenuto all'interno della pipe per W (se palindroma)
            str = strtok(buffer," ,.-\n");
            while (str != NULL) {

                // Costruisco il messaggio da inviare a P
                msg_r.type = P_TYPE;
                memcpy(msg_r.text, str, BUFSIZE);

                // Invia il messaggio al processo padre

                if (msgsnd(msg_queue_id, &msg_r, BUFSIZE, 0)) {
                    perror("Errore nell'invio del messaggio a P");
                    exit(1);
                }

                // printf("R Child, messaggio inviato a P uguale a: %s\n", str);
                    
                str = strtok (NULL, " ,.-\n");
                sleep(1);
            }

            // Costruisco il messaggio da inviare a P

            // sleep(1);
            // Chiudiamo per sicurezza il canale della pipe che non usiamo


        } while (size == BUFSIZE); // Svolge l'operazione finchè size non è uguale a BUFSIZE

        // Chiusura del file
        close(file_desc);
        exit(0);
    }

    // ---------------
    // Processo W
    // ---------------

    if (fork() == 0) {
        // Lettura di tutti i messaggi inviati dal processo Padre
        while ((n_bytes = msgrcv(msg_queue_id, &msg_w, BUFSIZE, W_TYPE, 0)) > 0)
            printf("W Child, messaggio ricevuto da Parent: %s\n\n", msg_w.text);
        if (n_bytes < 0) {
            perror("Errore nella lettura del messaggio di Padre");
            exit(1);
        }
        exit(0);
    }

    // ---------------
    // Processo Padre
    // ---------------

    // Lettura di tutti i messaggi inviati dal processo figlio R - DA SCRIVERE
    while ((n_bytes = msgrcv(msg_queue_id, &msg_p, BUFSIZE, P_TYPE, 0)) > 0) {
        // printf("Parent, messaggio ricevuto nella coda da parte di R: %s %d\n", msg_p.text, palindrome(msg_p.text));

        if (palindrome(msg_p.text) == 1) {
            // printf("\nLa stringa uguale a %s è palindroma! Invio al processo W...\n\n", msg_p.text);
            // Costruzione del messaggio
            msg_p.type = W_TYPE;
            // Invio del messaggio
            if (msgsnd(msg_queue_id, &msg_p, BUFSIZE, 0) == -1) {
                perror("Errore nella scrittura da parte di Padre");
                exit(1);
            }
        }
    }
    if (n_bytes < 0) {
        perror("Errore nella lettura del messaggio di R");
        exit(1);
    }



    msgctl(msg_queue_id, IPC_RMID, NULL);
    return 0;
}