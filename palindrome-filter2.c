#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>
#include <sys/shm.h>

#define BUFSIZE 1024
#define READ 0
#define WRITE 1

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
    int r_to_p[2], p_to_w[2];
    char buffer[BUFSIZE];
    char *message;

    // Controllo dei parametri
    if (argc < 2) {
        printf("Parametri insufficienti!");
        exit(1);
    }

    // Pipe per i processi R -> Padre
    if (pipe(r_to_p) == -1) {
        perror("Errore nella creazione della pipe R->P");
        exit(1);
    }
    // Pipe per i processi Padre->W
    if (pipe(p_to_w) == -1) {
        perror("Errore nella creazione della pipe P->W");
        exit(1);
    }
    
    /**
     * Processo R
     * Lavora su un testo ricevuto mediante file o stdin, scansionando
     * questo parola per parola, le quali verrano inserite (in messaggi distinti)
     * all'interno di una pipe tra R e Padre.
     */
    if (fork() == 0) {
        int file_desc, size, result;
        char buffer[BUFSIZE];

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
            printf("Questo è il buffer adesso: %s\n", buffer);


            // Leggo il messaggio ricevuto ed inserisco il contenuto all'interno della pipe per W (se palindroma)
            message = strtok(buffer," ,.-");
            while (message != NULL) {

                // Invia il messaggio al processo padre
                if (write(r_to_p[1], message, BUFSIZE) == -1) {
                    perror("Errore nella scrittura da parte di R");
                    exit(1);
                }
                printf("R: messaggio inviato a P uguale a %s\n", message);
                    
                message = strtok (NULL, " ,.-\n");
                sleep(1);
            }
            
            // sleep(1);
            // Chiudiamo per sicurezza il canale della pipe che non usiamo


        } while (size == BUFSIZE); // Svolge l'operazione finchè size non è uguale a BUFSIZE

        // Chiusura del file
        close(file_desc);
    }
    close(r_to_p[1]);   


    // Processo W
    if (fork() == 0) {
        // Lettura di tutti i messaggi inviati dal processo Padre
        int n_bytes;
        while ((n_bytes = read(p_to_w[READ], buffer, BUFSIZE)) > 0) {
            printf("W: %s\n", buffer);
        }
        if (n_bytes < 0) {
            perror("Errore nella lettura del messaggio di Padre");
            exit(1);
        }
    }

    // Processo Padre

    // Lettura di tutti i messaggi inviati dal processo figlio R - DA SCRIVERE
    int n_bytes;
    while ((n_bytes = read(r_to_p[READ], buffer, BUFSIZE)) > 0) {
        printf("P: messaggio ricevuto da parte di R, %s\n", buffer);

        if (palindrome(buffer) == 1) {
            printf("P: la stringa è palindroma, invio a W...\n");
            if (write(p_to_w[WRITE], buffer, strlen(buffer)) == -1) {
                perror("Errore nella scrittura da parte di Padre");
                exit(1);
            }
        }
    }
    if (n_bytes < 0) {
        perror("Errore nella lettura del messaggio di R");
        exit(1);
    }

    // Chiudiamo per sicurezza il canale della pipe che non usiamo
    close(r_to_p[0]); 
    // Chiudiamo per sicurezza il canale della pipe che non usiamo
    close(p_to_w[1]); 




    return 0;
}
