#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

#define BUFFER_SIZE     1024        // dimensione massima del buffer applicazione
#define MAX_USERS       10          // dimensione massima di utenti che si possono aggiungere ad una chat

typedef enum {false, true} bool;

int global_cmd;                     // identifica il comando inserito dall'utente
int init_flag;                      // init_flag = 1 -> fase di login, init_flag = 0 -> menu applicazione

// strutture dati globali per gestire i vari comandi
char username[64];                   
char password[64];
char filename[64];
char client_username[64];           
char chat_username[64];             // username del device con cui si inizia una chat
char username_group[64];            // username del device che vogliamo aggiungere ad una chat, comando \a username
int srv_port;                       // porta del server, di default = 4242

// strutture dati per gestire una chat di gruppo
int num_users;
struct user {
    char username[64];
    int port;
};

struct user users_group[MAX_USERS];

// funzione di utility che controlla la validita' dei comandi di input (signup, in, hanging, etc.)
int check_cmd(char* buffer)
{
    const char* signup = "signup";
    const char* in = "in";
    const char* hanging = "hanging";
    const char* show = "show";
    const char* chat = "chat";
    const char* out = "out";
    char* cmd;                          // comando di input
    char aux[64];                       // stringa di appoggio per contare gli argomenti
    char* count;            
    char token[64];
    int argn;

    // ricavo il primo elemento, cioe' il comando inserito
    strcpy(token, buffer);
    cmd = strtok(token, " ");
    argn = 0;

    // conto quanti argomenti ci sono, utilizzando aux come appoggio
    strncpy(aux, buffer, strlen(buffer));
    count = strtok(aux, " ");
    while(count != NULL){
        count = strtok(NULL, " ");
        argn++;
    }
    argn--; 

    if( (strncmp(cmd, signup, strlen(signup)) == 0) && (init_flag == 1) ){
        if(argn == 2){
            global_cmd = 0;
            return 1;
        }
        return -1;
    }
    else if( (strncmp(cmd, in, strlen(in)) == 0) && (init_flag == 1) ){
        if(argn == 3){
            global_cmd = 1;
            return 1;
        }
        else{
            return -1;
        }
    }
    else if( (strncmp(cmd, hanging, strlen(hanging)) == 0) && (init_flag == 0) ){
        if(argn == 0){
            global_cmd = 2;
            return 1;
        }
        return -1;
    }
    else if( (strncmp(cmd, show, strlen(show)) == 0) && (init_flag == 0) ){
        if(argn == 1){
            global_cmd = 3;
            return 1;
        }
        return -1;
    }
    else if( (strncmp(cmd, chat, strlen(chat)) == 0) && (init_flag == 0) ){
        if(argn == 1){
            global_cmd = 4;
            return 1;
        }
        return -1;
    }
    else if( (strncmp(cmd, out, strlen(out)) == 0) && (init_flag == 0) ){
        if(argn == 0){
            global_cmd = 6;
            return 1;
        }
        return -1;
    }
    else{
        return -1;
    }
}

// funzione di utility che inizializza le variabili globali in base al comando di input ricevuto
void set_arguments(char* buffer)
{
    char aux[64];
    char* token;
    int argn = 0;

    strcpy(aux, buffer);
    token = strtok(aux, " ");

    while(token != NULL){

        token = strtok(NULL, " ");

        if( ((global_cmd == 0 || global_cmd == 3 || global_cmd == 4) && (argn == 0)) ||
            ((global_cmd == 1) && (argn == 1)) ){
            strcpy(username, token);
        }
        if( ((global_cmd == 0) && (argn == 1)) || ((global_cmd == 1) && argn == 2) ){
            strcpy(password, token);
        }
        if( (global_cmd == 1) && (argn == 0) ){
            srv_port = strtol(token, NULL, 10);
        }
        if( (global_cmd == 5) && (argn == 0) ){
            strcpy(filename, token);
        }

        argn++;
    }
}

// controlla se l'utente loggato ha un altro utente in rubrica
int controlla_rubrica(char* buffer)
{
    char filename[64];
    char aux[64];
    char* token;
    char utente[64];
    FILE* ptr;

    strcpy(filename, "rubrica_");
    strcat(filename, client_username);
    strcat(filename, ".txt");
    ptr = fopen(filename, "r");

    if(ptr == NULL){
        printf("Errore durante la controlla_rubrica\n");
        return -1;
    }

    // scompongo il comando chat username
    strcpy(aux, buffer);
    token = strtok(aux, " ");
    token = strtok(NULL, " ");

    while(!feof(ptr)){
        fscanf(ptr, "%63s\n", utente);

        if( strncmp(utente, token, strlen(utente)) == 0 ){
            strcpy(chat_username, utente);
            fclose(ptr);
            return 1;
        }
    }

    fclose(ptr);
    return -1;
}

// inizializza la stringa client_username per riconoscere l'utente loggato
void set_client_username(char* buffer)
{
    char aux[64];
    char* token;

    strcpy(aux, buffer);

    token = strtok(aux, " ");
    token = strtok(NULL, " ");
    token = strtok(NULL, " ");
    strcpy(client_username, token);
}

// formatta il comando \a username, ovvero: \a username -> add username
void set_cmd_add_user(char* buffer)
{
    char aux[64];
    char* token;

    strcpy(aux, buffer);

    token = strtok(aux, " ");
    token = strtok(NULL, " ");
    token[strcspn(token, "\n")] = 0;
    strcpy(username_group, token);

    memset(buffer, 0, BUFFER_SIZE);
    strcat(buffer, "add ");
    strcat(buffer, token);
}

// formatta un messaggio della chat, buffer contiene il testo del messaggio da inviare
void formatta_messaggio(char* buffer, char* user_dest)
{
    int month, year;
    char timestamp_sent[64];
    char aux[BUFFER_SIZE];
    char* cmd = "message";
    char* delimiter = "||";

    time_t rawtime;
    struct tm* timeinfo;

    time(&rawtime);
    timeinfo = localtime(&rawtime);
    month = timeinfo->tm_mon;
    month++;
    year = timeinfo->tm_year;

    strcpy(aux, buffer);
    sprintf(timestamp_sent, "%d-%d-%d,%d:%d:%d",
            timeinfo->tm_mday, month, year,
            timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
    sprintf(aux, "%s%s%s%s%s%s%s%s%s%s%s", cmd, delimiter, client_username, delimiter, user_dest,
            delimiter, timestamp_sent, delimiter, timestamp_sent, delimiter, buffer);

    memset(buffer, 0, BUFFER_SIZE);
    strcpy(buffer, aux);
}

// salva il messaggio ricevuto nel file username.txt contenente la cronologia dei messaggi
void salva_cronologia(char* message, char* destinatario)
{
    FILE* ptr;
    char filename[64];
    char text[BUFFER_SIZE];

    strcpy(filename, client_username);
    strcat(filename, ".txt");

    ptr = fopen(filename, "a");

    if(ptr == NULL){
        printf("Errore durante la salva_cronologia\n");
        exit(-1);
    }

    strcpy(text, "*");
    strcat(text, message);

    formatta_messaggio(text, destinatario);

    fputs(text, ptr);
    fclose(ptr);
}

// concatena al file della cronologia locale un nuovo messaggio ricevuto dal server
void scarica_messaggio(char* buffer)
{
    char messaggio[BUFFER_SIZE];
    char supp[BUFFER_SIZE];

    char file_text[BUFFER_SIZE];
    char file_from[64];
    char file_to[64];
    char file_tm_sent[64];
    char file_tm_received[64];
    char* msg_token;

    int i = 0;

    char filename[64];
    FILE* ptr;

    strcpy(messaggio, buffer);
    msg_token = strtok(messaggio, "||");

    while(msg_token != NULL){
        if( i == 1 ){   // campo from del messaggio
            strcpy(file_from, msg_token);
        }
        if( i == 2 ){   // campo to del messaggio
            strcpy(file_to, msg_token);
        }
        if( i == 3 ){   // campo timestamp sent
            strcpy(file_tm_sent, msg_token);
        }
        if( i == 4 ){   // cmpo timestamp received
            strcpy(file_tm_received, msg_token);
        }
        if( i == 5 ){   // campo testo messaggio
            strcpy(file_text, msg_token);
        }

        msg_token = strtok(NULL, "||");
        i++;
    }    

    strcpy(supp, "**");
    strcat(supp, file_text);

    sprintf(buffer, "message||%s||%s||%s||%s||%s", file_from, file_to, file_tm_sent, file_tm_received, supp);

    strcpy(filename, client_username);
    strcat(filename, ".txt");    

    ptr = fopen(filename, "a");

    if(ptr == NULL){
        printf("Errore durante la scarica_messaggio\n");
        exit(-1);
    }

    fputs(buffer, ptr);
    fclose(ptr);
}

// aggiorna i messaggi destinati a "to_username" nel file della cronologia locale
void aggiorna_cronologia(char* to_username)
{
    char row[BUFFER_SIZE];
    char file_text[BUFFER_SIZE];
    char file_from[64];
    char file_to[64];
    char file_tm_sent[64];
    char file_tm_received[64];
    char filename[64];
    char* file_token;
    int i = 0;

    FILE* ptr;
    FILE* tmp;

    //printf("Aggiorna cronologia: from %s to %s\n", to_username, client_username);

    strcpy(filename, client_username);
    strcat(filename, ".txt");
    ptr = fopen(filename, "r+");
    tmp = fopen("replace.tmp", "w");

    if( (ptr == NULL) || (tmp == NULL) ){
        printf("Errore durante la aggiorna_cronologia\n");
        exit(-1);
    }

    while(!feof(ptr)){
        fscanf(ptr, "%[^\n]\n", row);

        file_token = strtok(row, "||");
        i = 0;
        while(file_token != NULL){
            if( i == 1 ){   // campo from del messaggio
                strcpy(file_from, file_token);
            }
            if( i == 2 ){   // campo to del messaggio
                strcpy(file_to, file_token);
            }
            if( i == 3 ){   // campo timestamp sent
                strcpy(file_tm_sent, file_token);
            }
            if( i == 4 ){   // campo timestamp received
                strcpy(file_tm_received, file_token);
            }
            if( i == 5 ){   // campo text
                strcpy(file_text, file_token);
            }

            file_token = strtok(NULL, "||");
            i++;
        }
        //printf("%s %s %s %s %s\n", file_from, file_to, file_tm_sent, file_tm_received, file_text);

        if( (strncmp(file_from, client_username, strlen(file_from)) == 0) && (strncmp(file_to, to_username, strlen(file_to)) == 0) ){
            
            if( strncmp(file_text, "**", 2) != 0 ){
                fprintf(tmp, "message||%s||%s||%s||%s||*%s\n", file_from, file_to, file_tm_sent, file_tm_received, file_text);
            }
            else{
                fprintf(tmp, "message||%s||%s||%s||%s||%s\n", file_from, file_to, file_tm_sent, file_tm_received, file_text);
            }
        }
        else{
            fprintf(tmp, "message||%s||%s||%s||%s||%s\n", file_from, file_to, file_tm_sent, file_tm_received, file_text);
        }
    }

    fclose(ptr);
    fclose(tmp);

    remove(filename);
    rename("replace.tmp", filename);
}

// scrive un messaggio gia' formattato dentro il file della cronologia utente
void scrivi_messaggio(char* messaggio)
{
    char filename[64];
    FILE* ptr;

    strcpy(filename, client_username);
    strcat(filename, ".txt");

    ptr = fopen(filename, "a");

    if( ptr == NULL ){
        printf("Errore durante la scrivi_messaggio\n");
        exit(-1);
    }

    fclose(ptr);
}

// stampa il messaggio dentro "messaggio"
void stampa_messaggio(char* messaggio)
{
    char msg_aux[BUFFER_SIZE];

    char file_text[BUFFER_SIZE];
    char file_from[64];
    char file_to[64];
    char file_tm_sent[64];
    char file_tm_received[64];
    char* msg_token;

    int i = 0;

    strcpy(msg_aux, messaggio);
    msg_token = strtok(msg_aux, "||");

    while(msg_token != NULL){
        if(i == 1){
            strcpy(file_from, msg_token);
        }
        if(i == 2){
            strcpy(file_to, msg_token);
        }
        if(i == 3){
            strcpy(file_tm_sent, msg_token);
        }
        if(i == 4){
            strcpy(file_tm_received, msg_token);
        }
        if(i == 5){
            strcpy(file_text, msg_token);
        }

        msg_token = strtok(NULL, "||");
        i++;
    }

    printf("%s: %s", file_from, file_text);
}

// stampa l'intera chat con "chat_username"
void stampa_chat(char* chat_username)
{
    char filename[64];
    char file_from[64];
    char file_to[64];
    char row[BUFFER_SIZE];
    char file_text[BUFFER_SIZE];
    char* file_token;

    int i = 0;

    FILE* ptr;

    strcpy(filename, client_username);
    strcat(filename, ".txt");

    ptr = fopen(filename, "r");

    if( ptr == NULL ){
        return;
    }

    fseek(ptr, 0, SEEK_END);
    if( ftell(ptr) == 0 ){
        printf("Nuova conversazione\n");
    }
    else{
        printf("Apertura chat: %s %s\n", chat_username, client_username);
        fseek(ptr, 0, SEEK_SET);

        while(!feof(ptr)){
            fscanf(ptr, "%[^\n]\n", row);

            i = 0;
            file_token = strtok(row, "||");
            while(file_token != NULL){
                if(i == 1){
                    strcpy(file_from, file_token);
                }
                if(i == 2){
                    strcpy(file_to, file_token);
                }
                if(i == 5){
                    strcpy(file_text, file_token);
                }
                file_token = strtok(NULL, "||");
                i++;
            }

            if( ( (strncmp(file_from, chat_username, strlen(file_from)) == 0) && (strncmp(file_to, client_username, strlen(file_to)) == 0 ) ) ||
                ( (strncmp(file_from, client_username, strlen(file_from)) == 0) && (strncmp(file_to, chat_username, strlen(file_to)) == 0 ) ) ){
                printf("%s: %s\n", file_from, file_text);
            }  
        }
    }
    fclose(ptr);
}

// controlla l'esistenza di un file nella directory corrente
bool file_exists(char* pathname)
{
    FILE* ptr;

    pathname[strcspn(pathname, "\n")] = 0;

    ptr = fopen(pathname, "r");

    if(ptr == NULL){
        return false;
    }
    return true;
}

// invia il file "filename" ad un socket, il file viene diviso in chunk e spedito a blocchi
int send_file(int socket, char* filename)
{
    FILE* ptr;
    int dim, read_dim, ret, chunk_index;        // variabili per gestire l'invio in chunk del file
    char chunk[10240];      
    chunk_index = 1;

    ptr = fopen(filename, "r");

    fseek(ptr, 0, SEEK_END);
    dim = ftell(ptr);          // dimensione totale del file
    fseek(ptr, 0, SEEK_SET);

    // invio la dimensione del file al socket
    write(socket, (void*)&dim, sizeof(int));       // scrivo nel socket la dimensione del file

    // invio il file a blocchi, dividendolo in chunk di 10240 byte
    while( !feof(ptr) ){
        read_dim = fread(chunk, 1, sizeof(chunk)-1, ptr);       // leggo dal file un chunk

        do{
            ret = write(socket, chunk, read_dim);               // scrivo nel socket il chunk appena letto
        } while( ret < 0 );

        chunk_index++;

        bzero(chunk, sizeof(chunk));        // pulisco chunk
    }

    fclose(ptr);
    return 0;
}

// ricevo un file dal socket, il filename viene settato in base all'username del device
int receive_file(int socket, char* filename)
{
    FILE* ptr;

    fd_set fds;

    int c_size = 0;
    struct timeval timeout = {10, 0};

    int buffer_fd, recv_size, read_size, write_size, chunk_index, c_stat;
    char chunk[10241];
    char tmp_file[256];
    char new_filename[256];
    char* est_file;

    chunk_index = 1;
    recv_size = 0;

    strcpy(tmp_file, filename);
    est_file = strtok(tmp_file, ".");
    est_file = strtok(NULL, " "); 

    strcpy(new_filename, "file_");
    strcat(new_filename, client_username);
    strcat(new_filename, ".");
    strcat(new_filename, est_file);

    ptr = fopen(new_filename, "w");

    if( ptr == NULL ){
        printf("Errore nell'apertura file\n");
    }

    // leggo dal socket la dimensione del file
    do{
        c_stat = read(socket, &c_size, sizeof(int));
    } while( c_stat < 0 );

    // ciclo finche' non ricevo completamente tutto il file
    while( recv_size < c_size ){
        FD_ZERO(&fds);              // inizializzo il file descriptor set fds
        FD_SET(socket, &fds);       // inserisci il socket dentro il file descriptor set fds

        buffer_fd = select(FD_SETSIZE, &fds, NULL, NULL, &timeout);

        if(buffer_fd < 0){
            printf("Errore: file descriptor set non corretto.\n");
        }

        if(buffer_fd == 0){
            printf("Errore: buffer read timeout expired\n");
        }

        if(buffer_fd > 0){ 
            do{
                read_size = read(socket, chunk, 10241);         // leggo dal socket destinatario il chunk del file
            } while( read_size < 0 );

            write_size = fwrite(chunk, 1, read_size, ptr);      // scrivo nel file il chunk appena ricevuto

            if( read_size != write_size ){
                printf("Errore nella lettura o scrittura del chunk\n");
            }

            recv_size += read_size;
            chunk_index++;
        }
    }

    printf("Hai ricevuto un nuovo file\n");
    fclose(ptr);
    return 0;
}

// rimanda indietro il pacchetto ricevuto dal server, per comunicare che e' ancora online
void send_back(int socket, char* msg)
{
    int ret, len;
    uint16_t lmsg;

    len = strlen(msg)+1;
    lmsg = htons(len);

    ret = send(socket, (void*)&lmsg, sizeof(uint16_t), 0);
    if(ret < 0){
        perror("send_back -> errore durante l'invio della dimensione");
        return;
    }

    ret = send(socket, (void*)&msg, len, 0);
    if(ret < 0){
        perror("send_back -> errore durante l'invio del messaggio");
        return;
    }
}

void salva_tm_logout()
{
    FILE* ptr;
    time_t rawtime;
    struct tm* timeinfo;
    char filename[64];
    char timestamp_logout[64];
    int month, year;

    strcpy(filename, client_username);
    strcat(filename, "_tm_logout.txt");

    ptr = fopen(filename, "w");

    if( ptr == NULL ){
        printf("Errore durante la salva_tm_logout\n");
        return;
    }

    time(&rawtime);
    timeinfo = localtime(&rawtime);
    month = timeinfo->tm_mon;
    month++;
    year = timeinfo->tm_year;

    sprintf(timestamp_logout, "%d-%d-%d,%d:%d:%d",
            timeinfo->tm_mday, month, year,
            timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
    fprintf(ptr, "%s\n", timestamp_logout);
    printf("Salvato timestamp locale\n");
}

void send_timestamp_logout(int socket)
{
    FILE* ptr;
    char filename[64];
    char send_buffer[128];
    uint16_t lmsg;
    int ret, len;

    strcpy(filename, client_username);
    strcat(filename, "_tm_logout.txt");

    if( access(filename, F_OK) == 0 ){ // il file esiste
        // invia
        ptr = fopen(filename, "r");

        if( ptr == NULL ){
            printf("Errore durante la send_timestamp_logout\n");
            return;
        }

        fgets(send_buffer, 64, ptr);
        fclose(ptr);
    } 
    else{  // il file non esiste
        strcpy(send_buffer, "0000");
    }

    strcat(send_buffer, " ");
    strcat(send_buffer, client_username);

    //printf("send_buffer: %s\n", send_buffer);
    len = strlen(send_buffer)+1;
    lmsg = htons(len);

    ret = send(socket, (void*)&lmsg, sizeof(uint16_t), 0);
    if(ret < 0){
        perror("Errore durante la send_timestamp_logout ");
        return;
    }

    ret = send(socket, (void*)&send_buffer, len, 0);
    if(ret < 0){
        perror("Errore durante la send_timestamp_logout");
        return;
    }

    remove(filename);
}

// invia un messaggio (msg) ad un peer, che e' in ascolto sulla porta (porta)
bool send_message(char* msg, int porta)
{
    struct sockaddr_in peer_addr;
    uint16_t lmsg;
    int ret, len, peer_sd;
    char send_buffer[BUFFER_SIZE];

    //printf("Msg: %s\nPorta: %d\n", msg, porta);

    strcpy(send_buffer, msg);

    peer_sd = socket(AF_INET, SOCK_STREAM, 0);
    memset(&peer_addr, 0, sizeof(peer_addr));
    peer_addr.sin_family = AF_INET;
    peer_addr.sin_port = htons(porta);
    inet_pton(AF_INET, "127.0.0.1", &peer_addr.sin_addr);

    ret = connect(peer_sd, (struct sockaddr*)&peer_addr, sizeof(peer_addr));
    if(ret < 0){
        perror("send_message -> Errore durante la connect al peer ");
        return false;
    }

    len = strlen(send_buffer)+1;
    lmsg = htons(len);

    ret = send(peer_sd, (void*)&lmsg, sizeof(uint16_t), 0);
    if(ret < 0){
        perror("send_message -> Errore durante l'invio della dimensione ");
        return false;
    }

    ret = send(peer_sd, (void*)&send_buffer, len, 0);
    if(ret < 0){
        perror("send_message -> Errore durante l'invio del messaggio ");
        return false;
    }

    close(peer_sd);
    return true;
}

// invia il comando share||filename.ext ai peer all'interno di un gruppo chat, per poi mandare il file
void share_file_to_peers(char* filename)
{
    struct sockaddr_in peer_addr;
    int peer_sd, ret, len, i;
    uint16_t lmsg;
    char buffer[BUFFER_SIZE];

    for(i = 0; i<num_users; i++){

        memset(buffer, 0, BUFFER_SIZE);
        sprintf(buffer, "share||%s\n", filename);
        //printf("buffer: %s", buffer);

        peer_sd = socket(AF_INET, SOCK_STREAM, 0);
        memset(&peer_addr, 0, sizeof(peer_addr));
        peer_addr.sin_family = AF_INET;
        peer_addr.sin_port = htons(users_group[i].port);
        inet_pton(AF_INET, "127.0.0.1", &peer_addr.sin_addr);

        ret = connect(peer_sd, (struct sockaddr*)&peer_addr, sizeof(peer_addr));
        if(ret < 0){
            perror("share_file_to_peers -> Errore sulla connect al peer ");
            return;
        }

        len = strlen(buffer) + 1;
        lmsg = htons(len);

        ret = send(peer_sd, (void*)&lmsg, sizeof(uint16_t), 0);
        if(ret < 0){
            printf("share_file_to_peers -> Errore nella send dimensione: ");
            return;
        }

        ret = send(peer_sd, (void*)&buffer, len, 0);
        if(ret < 0){
            printf("share_file_to_peers -> Errore nella send messaggio: ");
            return;
        }    

        send_file(peer_sd, filename);
    }
}

int main(int argc, char* argv[])
{
    // variabili che gestiscono i valori di ritorno delle system call: socket, connect, send, etc.
    int ret, sd, client_sd, len, tot_msg;                               // ret: valore di ritorno delle system call, sd: socket di comunicazione, etc.
    int peer_sd;                        
    int child_len, child_ret;
    socklen_t child_size;
    int new_sd;
    const int opt = 1;
    int i;
    uint16_t lmsg, child_lmsg;
    int32_t totale_messaggi;                                            // totale messaggi pendenti, viene inviato dal server

    struct sockaddr_in server_addr, client_addr, peer_addr, new_addr;   // indirizzi vari del server e di altri peer/client
    struct user utente;

    char buffer[BUFFER_SIZE];                                           // buffer principale applicativo
    char child_buffer[BUFFER_SIZE];                                     // buffer utilizzato dal processo child
    char message_text[BUFFER_SIZE];                                     // buffer di appoggio per il testo di una chat (con singolo utente)
    char group_buffer[BUFFER_SIZE];                                     // buffer di appoggio per il testo di un gruppo chat
    char aux_msg_text[BUFFER_SIZE];                                     // buffer ausiliario
    char porta[64];                                                     // porta del device locale
    char aux[64];                                                       // array ausiliario

    char chat_cmd[64];                                                  // contiene il comando di una chat, \u, \a username, etc.
    // strutture dati di supporto per gestire i comandi del device
    char* token;                                                        
    char* usr_dest;
    char* stato_utente;
    char* notifica;
    char* file_token;
    char porta_peer[64];

    // strutture dati del processo child
    pid_t listener_child;

    char* c_token;
    char c_aux[BUFFER_SIZE];

    bool add_user_flag = false;
    bool flag_chat = false;
    bool flag_msg = false;

    const char* invalid_arg = "Argomento non valido\0";
    const char* invalid_cmd = "Comando non valido\0";
    const char* primo_msg = "Inserisci username e password, o registrati al servizio\n\0";
    const char* signup_msg = "Utente registrato con successo\0";
    const char* login_code = "conferma\0";
    const char* wrong_login = "Credenziali non valide\0";
    const char* login_msg = "*** MENU INIZIALE ***\nDigita un comando:\n\n1) hanging --> guarda i messaggi ricevuti mentre eri offline\n"
                            "2) show 'username' --> guarda i messaggi pendenti che 'username' ti ha inviato mentre eri offline\n"
                            "3) chat 'username' --> avvia una chat con 'username'\n"
                            "4) share 'filename' --> invia un file ad un utente con cui stai chattando\n"
                            "5) out --> disconettiti dal servizio\n\0";

    init_flag = 1;

    // controllo gli argomenti passati dal comando ./dev
    if(argc == 2){
        strcpy(porta, argv[1]);
    }
    else{
        printf("%s\n", invalid_arg);
    }

    printf("%s", primo_msg);  

    while(1){

        memset(buffer, 0, sizeof(buffer));
        fgets(buffer, BUFFER_SIZE, stdin);

        // controlla se i comandi iniziali sono validi, compresi gli argomenti
        if(check_cmd(buffer) < 0){
            printf("%s\n", invalid_cmd);
        }
        else{

            if(global_cmd < 6){
                set_arguments(buffer);
            }

            if(global_cmd == 0){    // signup, utilizzo la porta 4242 nella connect al server
                sd = socket(AF_INET, SOCK_STREAM, 0);
                memset(&server_addr, 0, sizeof(server_addr));
                server_addr.sin_family = AF_INET;
                server_addr.sin_port = htons(4242);
                inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

                ret = connect(sd, (struct sockaddr*)&server_addr, sizeof(server_addr));
                if(ret < 0){
                    perror("1) Errore durante la connect al server ");
                    continue;
                }
            }

            if(global_cmd >= 1){    // comando generico, utilizzo la srv_port nella connect, passata inizialmente dal comando in

                if( (global_cmd == 4) || (global_cmd == 3) ){    // controlli preliminari dei comandi "chat username" or "show username"
                    // controllo se username esiste nella rubrica del device
                    if( controlla_rubrica(buffer) < 0 ){
                        printf("Questo utente non e' in rubrica\n");
                        continue;
                    }
                }

                sd = socket(AF_INET, SOCK_STREAM, 0);
                memset(&server_addr, 0, sizeof(server_addr));
                server_addr.sin_family = AF_INET;
                server_addr.sin_port = htons(srv_port);
                inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

                ret = connect(sd, (struct sockaddr*)&server_addr, sizeof(server_addr));
                if(ret < 0){
                    perror("1) Errore durante la connect al server ");
                    if(global_cmd == 6){
                        // salva timestamp_logout del device
                        salva_tm_logout();
                        close(sd);
                        kill(listener_child, SIGKILL);
                        break;
                    }
                    continue;
                } 
            }

            if(global_cmd == 1){    // comando in, invio la porta del device al server
                buffer[strcspn(buffer, "\n")] = 0;
                strncat(buffer, " ", 1);
                strncat(buffer, porta, strlen(porta));
                strncat(buffer, "\n", 1);

                set_client_username(buffer);

                //printf("Dopo la set_client_username: %s\n", client_username);
            }

            /* COMANDI PRIMA DELLA LOGIN */
            // 0 = signup, 1 = in
            if( (global_cmd == 0) || (global_cmd == 1) ){       
                len = strlen(buffer) + 1;
                lmsg = htons(len);

                // invio al server i comandi di signup o in
                ret = send(sd, (void*)&lmsg, sizeof(uint16_t), 0);
                if(ret < 0){
                    perror("2) Errore durante l'invio della dimensione ");
                    continue;
                }

                ret = send(sd, (void*)&buffer, len, 0);
                if(ret < 0){
                    perror("2) Errore durante l'invio del messaggio ");
                    continue;
                }

                // ricevo la risposta del server
                ret = recv(sd, (void*)&lmsg, sizeof(uint16_t), 0);
                if(ret < 0){
                    perror("3) Errore durante la ricezione della dimensione ");
                    continue;
                }

                len = ntohs(lmsg);
                ret = recv(sd, (void*)&buffer, len, 0);
                if(ret < 0){
                    perror("3) Errore durante la receive del messaggio ");
                    continue;
                }

                strncat(buffer, "\0", 1);

                // se il login e' andato a buon fine, il device puo' utilizzare i comandi successivi, ed e' in ascolto di altri peer
                if( (global_cmd == 1) && (strncmp(buffer, login_code, strlen(login_code)) == 0) ){
                    printf("%s", login_msg);
                    init_flag = 0;

                    // invia il timestamp logout locale del device al server,
                    // solo nel caso in cui il timestamp del device e' stato salvato localmente
                    send_timestamp_logout(sd);

                    listener_child = fork();
                    if(listener_child == 0){
                        close(sd);
                        //printf("Porta child in ascolto su: %s\n", porta);
                        client_sd = socket(AF_INET, SOCK_STREAM, 0);
                        memset(&client_addr, 0, sizeof(client_addr));
                        client_addr.sin_family = AF_INET;
                        client_addr.sin_port = htons(strtol(porta, NULL, 10));
                        client_addr.sin_addr.s_addr = INADDR_ANY;

                        setsockopt(client_sd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
                        child_ret = bind(client_sd, (struct sockaddr*)&client_addr, sizeof(client_addr));
                        if(child_ret < 0){
                            perror("Errore durante la bind del child process: ");
                            exit(-1);
                        }

                        child_ret = listen(client_sd, 10);
                        if(child_ret < 0){
                            perror("Errore durante la listen del child process: ");
                            exit(-1);
                        }

                        while(1){
                            child_size = sizeof(new_addr);
                            new_sd = accept(client_sd, (struct sockaddr*)&new_addr, &child_size);

                            fflush(stdout);

                            // ricezione del comando da parte di un peer o del server
                            child_ret = recv(new_sd, (void*)&child_lmsg, sizeof(uint16_t), 0);
                            if(child_ret < 0){
                                perror("Errore durante la receive del child process: ");
                                continue;
                            }
                            child_len = ntohs(child_lmsg);

                            child_ret = recv(new_sd, (void*)&child_buffer, child_len, 0);
                            if(child_ret < 0){
                                perror("Errore durante la receive del child process: ");
                                continue;
                            }

                            strncat(child_buffer, "\0", 1);
                            strcpy(c_aux, child_buffer);
                            c_token = strtok(c_aux, "||");

                            //printf("Comando child ricevuto: %s\n", c_token);

                            // il peer riceve una notifica dal server
                            if( strcmp(c_token, "notifica") == 0 ){ 
                                c_token = strtok(NULL, "||");
                                //printf("%s\n", c_token);
                                aggiorna_cronologia(c_token);
                            }

                            // il peer riceve un messaggio da un altro peer
                            if( strcmp(c_token, "message") == 0 ){
                                stampa_messaggio(child_buffer);
                                scarica_messaggio(child_buffer);
                            }

                            // il peer riceve un file (all'interno della chat) da un altro peer
                            if( strncmp(c_token, "share", 5) == 0 ){
                                c_token = strtok(NULL, "||");
                                c_token[strcspn(c_token, "\n")] = 0;
                                //printf("%s\n", c_token);

                                receive_file(new_sd, c_token);
                            }

                            if( strcmp(c_token, "test") == 0 ){
                                // il peer e' ancora online
                                send_back(new_sd, child_buffer);
                            }

                            close(new_sd);
                        }
                    }
                }
                else{
                    if(global_cmd == 0){
                        printf("%s\n", signup_msg);
                    }
                    else{
                        printf("%s\n", wrong_login);
                    }
                }
            }

            /* COMANDI DOPO AVER EFFETTUATO IL LOGIN */
            if(global_cmd >= 2){

                buffer[strcspn(buffer, "\n")] = 0;
                strncat(buffer, " ", 1);
                strncat(buffer, client_username, strlen(client_username));
                strncat(buffer, "\n", 1);
                //printf("%s", buffer);

                len = strlen(buffer) + 1;
                lmsg = htons(len);

                // invio il comando al server
                ret = send(sd, (void*)&lmsg, sizeof(uint16_t), 0);
                if(ret < 0){
                    perror("4) Errore durante l'invio della dimensione del comando ");
                    if(global_cmd == 6){
                        // salva timestamp_logout del device 
                        salva_tm_logout();
                        close(sd);
                        kill(listener_child, SIGKILL);
                        break;
                    }
                    continue;
                }

                ret = send(sd, (void*)&buffer, len, 0);
                if(ret < 0){
                    perror("4) Errore durante l'invio del comando ");
                    if(global_cmd == 6){
                        // salva timestamp_logout del device
                        salva_tm_logout();
                        close(sd);
                        kill(listener_child, SIGKILL);
                        break;
                    }
                    continue;
                }
            }

            if(global_cmd == 2){    // comando hanging
                // ricevo dal server la lista di messaggi pendenti
                ret = recv(sd, (void*)&lmsg, sizeof(uint16_t), 0);
                if(ret < 0){
                    perror("5) hanging -> Errore durante la recv della dimensione ");
                    continue;
                }

                len = ntohs(lmsg);

                ret = recv(sd, (void*)&buffer, len, 0);
                if(ret < 0){
                    perror("5) hanging -> Errore durante la recv del messaggio ");
                    continue;
                }

                strncat(buffer, "\0", 1);
                printf("%s", buffer);
            }

            if(global_cmd == 3){    // comando show username
                // ricevo il numero totale dei messaggi
                ret = recv(sd, (void*)&totale_messaggi, sizeof(int32_t), 0);
                if(ret < 0){
                    perror("6) show -> Errore durante la ricezione del numero messaggi ");
                    continue;
                }

                tot_msg = htonl(totale_messaggi);
                printf("totale messaggi: %d\n", tot_msg);

                if(tot_msg > 0){
                    // ciclo tot_msg volte per ricevere tutti i messaggi bufferizzati
                    for(i = 0; i<tot_msg; i++){
                        // dimensione del singolo messaggio
                        ret = recv(sd, (void*)&lmsg, sizeof(uint16_t), 0);
                        if(ret < 0){
                            perror("6) show -> Errore durante la ricezione della dimensione ");
                            continue;
                        }

                        len = ntohs(lmsg);
                        // contenuto del singolo messaggio
                        ret = recv(sd, (void*)&buffer, len, 0);
                        if(ret < 0){
                            perror("6) show -> Errore durante la ricezione della lista messaggi ");
                            continue;
                        }

                        strcat(buffer, "\0");
                        scarica_messaggio(buffer);
                        printf("%s", buffer);
                    }
                }
                else{
                    printf("Non ci sono messaggi da parte di questo utente\n");
                }
            }

            if(global_cmd == 4){    // comando chat username

                memset(chat_cmd, 0, sizeof(aux));
                strcpy(chat_cmd, buffer);
                //printf("chat_cmd: %s\n", chat_cmd);
                usr_dest = strtok(chat_cmd, " ");
                usr_dest = strtok(NULL, " ");
                //printf("1) usr_dest: %s\n", usr_dest);

                // contatto il server per sapere se l'utente destinatario e' online/offline
                ret = recv(sd, (void*)&lmsg, sizeof(uint16_t), 0);
                if(ret < 0){
                    perror("Errore durante la ricezione della dimensione: ");
                    exit(-1);
                }

                len = ntohs(lmsg);
                // il buffer contiene la porta dell'utente destinatario o del server
                ret = recv(sd, (void*)&buffer, len, 0);
                if(ret < 0){
                    perror("Errore durante la ricezione del messaggio: ");
                    exit(-1);
                }

                // scompongo il messaggio per sapere se inviare i messaggi al server all'utente destinatario
                memset(stato_utente, 0, strlen(stato_utente));
                memset(aux, 0, sizeof(aux));
                strncpy(aux, buffer, strlen(buffer));
                token = strtok(aux, " ");
                stato_utente = strtok(NULL, " ");
                notifica = strtok(NULL, " ");

                // all'apertura della chat apro il file e stampo il contenuto a video
                // da questo punto in poi l'utente si trova nella schermata della chat aperta
                //printf("Utente: %s\n", buffer);
                close(sd);
                //printf("Stato utente: %s\n", stato_utente);
                //printf("Notifica: %s\n", notifica);

                // se il server ha bufferizzato una notifica, aggiorna i messaggi letti
                if( strncmp(notifica, "s-notifica", strlen(notifica)) == 0 ){
                    //printf("2) usr_dest: %s\n", usr_dest);
                    aggiorna_cronologia(usr_dest);
                }

                if( strncmp(stato_utente, "notfound", strlen(stato_utente)) == 0 ){
                    printf("Questo utente non e' registrato al servizio\n");
                }
                else{   // avvio la chat
                    stampa_chat(usr_dest);
                    num_users = 0;

                    if( strncmp(stato_utente, "online", strlen(stato_utente)) == 0 ){
                        //printf("Utente online, in ascolto su porta: %s\n", token);
                        strcpy(porta_peer, token);

                        while(1){

                            memset(buffer, 0, BUFFER_SIZE);
                            fgets(buffer, BUFFER_SIZE, stdin);

                            strcpy(chat_cmd, buffer);

                            if( strcmp(buffer, "\\q\n") == 0 ){
                                printf("%s", login_msg);    // l'utente chiude la chat
                                break;                      // torno al menu iniziale
                            }
                            else if( strcmp(buffer, "\\u\n") == 0 ){
                                // chiedo al server di mandarmi la lista degli utenti online
                                memset(aux, 0, sizeof(aux));
                                strcpy(aux, buffer);
                                memset(buffer, 0, sizeof(buffer));
                                strcpy(buffer, "onlineusers ");
                                strncat(buffer, client_username, strlen(client_username));
                                add_user_flag = false;
                                flag_msg = false;
                            }
                            else if( strncmp(buffer, "\\a", 2) == 0 ){

                                if( controlla_rubrica(buffer) < 0 ){
                                    printf("Questo utente non e' in rubrica\n");
                                    continue;
                                }

                                // chiedo al server di mandarmi la porta dell'utente aggiunto
                                memset(aux, 0, sizeof(aux));
                                strcpy(aux, buffer);
                                set_cmd_add_user(buffer);

                                flag_chat = false;
                                for(i = 0; i<num_users; i++){
                                    if( (strcmp(users_group[i].username, username_group) == 0) || (strcmp(chat_username, username_group) == 0) ||
                                        (strcmp(client_username, username_group) == 0) ){
                                        printf("Questo utente fa gia' parte della chat\n");
                                        flag_chat = true;
                                    }
                                }

                                if( (flag_chat) ){
                                    continue;
                                }

                                add_user_flag = true;
                                flag_msg = false;   
                            }
                            else if( strncmp(buffer, "share", 5) == 0 ){
                                // 
                                memset(aux, 0, sizeof(aux));
                                strcpy(aux, buffer);
                                file_token = strtok(aux, " ");
                                file_token = strtok(NULL, " ");
                                add_user_flag = false;
                                flag_msg = false;
                            }
                            else{   // si tratta di un messaggio generico
                                memset(message_text, 0, sizeof(message_text));
                                memset(group_buffer, 0, sizeof(group_buffer));
                                strcpy(message_text, buffer);
                                strcpy(group_buffer, buffer);
                                formatta_messaggio(buffer, chat_username);
                                add_user_flag = false;
                                flag_msg = true;
                            }

                            if( (strncmp(chat_cmd, "\\u\n", 2) == 0) || (strncmp(chat_cmd, "\\a", 2) == 0) ){
                                // inizializzo il socket del server
                                sd = socket(AF_INET, SOCK_STREAM, 0);
                                memset(&server_addr, 0, sizeof(server_addr));
                                server_addr.sin_family = AF_INET;
                                server_addr.sin_port = htons(srv_port);
                                inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

                                ret = connect(sd, (struct sockaddr*)&server_addr, sizeof(server_addr));
                                if(ret < 0){
                                    perror("Errore durante la connect al server ");
                                    continue;
                                }

                                len = strlen(buffer) + 1;
                                lmsg = htons(len);

                                ret = send(sd, (void*)&lmsg, sizeof(uint16_t), 0);
                                if(ret < 0){
                                    perror("Errore durante l'invio della dimensione ");
                                    continue;
                                }

                                ret = send(sd, (void*)&buffer, len, 0);
                                if(ret < 0){
                                    perror("Errore durante l'invio del messaggio ");
                                    continue;
                                }

                                ret = recv(sd, (void*)&lmsg, sizeof(uint16_t), 0);
                                if(ret < 0){
                                    perror("Errore durante la ricezione dimensione ");
                                    continue;
                                }

                                len = ntohs(lmsg);
                                memset(buffer, 0, sizeof(buffer));

                                ret = recv(sd, (void*)&buffer, len, 0);
                                if(ret < 0){
                                    perror("Errore durante la ricezione messaggio ");
                                    continue;
                                }

                                printf("%s\n", buffer);
                            }
                            else if( strncmp(chat_cmd, "share", 5) == 0 ){

                                if( file_exists(file_token) ){
                                    peer_sd = socket(AF_INET, SOCK_STREAM, 0);

                                    memset(buffer, 0, BUFFER_SIZE);
                                    sprintf(buffer, "share||%s\n", file_token);
                                    //printf("buffer: %s", buffer);

                                    memset(&peer_addr, 0, sizeof(peer_addr));
                                    peer_addr.sin_family = AF_INET;
                                    peer_addr.sin_port = htons(strtol(porta_peer, NULL, 10));
                                    inet_pton(AF_INET, "127.0.0.1", &peer_addr.sin_addr);

                                    ret = connect(peer_sd, (struct sockaddr*)&peer_addr, sizeof(peer_addr));
                                    if(ret < 0){
                                        perror("Errore sulla connect al peer ");
                                        continue;
                                    }

                                    len = strlen(buffer) + 1;
                                    lmsg = htons(len);

                                    ret = send(peer_sd, (void*)&lmsg, sizeof(uint16_t), 0);
                                    if(ret < 0){
                                        printf("Errore nella send dimensione: ");
                                        continue;
                                    }

                                    ret = send(peer_sd, (void*)&buffer, len, 0);
                                    if(ret < 0){
                                        printf("Errore nella send messaggio: ");
                                        continue;
                                    }

                                    file_token[strcspn(file_token, "\n")] = 0;

                                    send_file(peer_sd, file_token);

                                    if(num_users > 0){  // share file to other peers
                                        share_file_to_peers(file_token);
                                    }

                                    memset(buffer, 0, BUFFER_SIZE);
                                    strcpy(buffer, "fine\0");
                                    
                                    close(peer_sd);
                                }
                                else{
                                    printf("Il file %s non esiste\n", file_token);
                                }
                            }
                            else{
                                // si tratta di un messaggio generico
                                //printf("Porta peer: %s\n", porta_peer);
                                peer_sd = socket(AF_INET, SOCK_STREAM, 0);
                                memset(&peer_addr, 0, sizeof(peer_addr));
                                peer_addr.sin_family = AF_INET;
                                peer_addr.sin_port = htons(strtol(porta_peer, NULL, 10));
                                inet_pton(AF_INET, "127.0.0.1", &peer_addr.sin_addr);

                                ret = connect(peer_sd, (struct sockaddr*)&peer_addr, sizeof(peer_addr));
                                if(ret < 0){
                                    perror("Errore sulla connect al peer ");
                                    continue;
                                }

                                len = strlen(buffer) + 1;
                                lmsg = htons(len);

                                ret = send(peer_sd, (void*)&lmsg, sizeof(uint16_t), 0);
                                if(ret < 0){
                                    perror("Errore durante la send dimensione ");
                                    continue;
                                }

                                ret = send(peer_sd, (void*)&buffer, len, 0);
                                if(ret < 0){
                                    perror("Errore durante la send messaggio: ");
                                    continue;
                                }

                                scarica_messaggio(buffer);
                                close(peer_sd);
                            }

                            if( (num_users > 0) && (flag_msg) ){
                                for(i = 0; i<num_users; i++){
                                    //printf("%s||%s", users_group[i].username, message_text);

                                    formatta_messaggio(group_buffer, users_group[i].username);

                                    if( send_message(group_buffer, users_group[i].port) ){
                                        // aggiungo un * all'inizio di message_text
                                        memset(aux_msg_text, 0, sizeof(aux_msg_text));
                                        strcpy(aux_msg_text, "*");
                                        strcat(aux_msg_text, message_text);
                                        salva_cronologia(aux_msg_text, users_group[i].username);
                                    }
                                }
                            }

                            if(add_user_flag){
                                if( strcmp(buffer, "notfound") == 0 ){
                                    printf("%s\n", "Impossibile aggiungere questo utente alla chat\n");
                                }
                                else{
                                    // aggiungi utente al gruppo chat
                                    if(num_users < MAX_USERS){
                                        strcpy(utente.username, username_group);
                                        utente.port = strtol(buffer, NULL, 10);
                                        users_group[num_users] = utente;
                                        num_users++;
                                        
                                        for(i = 0; i<num_users; i++){
                                            //printf("%s %d\n", users_group[i].username, users_group[i].port);
                                        }

                                    }
                                    else{
                                        printf("%s\n", "Raggiunto numero massimo di utenti");
                                    }
                                }
                            }

                            
                        }
                    }
                    else{   // utente offline, mando i messaggi al server
                        num_users = 0;
                        //printf("Utente offline\n");

                        while(1){
                            sd = socket(AF_INET, SOCK_STREAM, 0);
                            memset(&server_addr, 0, sizeof(server_addr));
                            server_addr.sin_family = AF_INET;
                            server_addr.sin_port = htons(srv_port);
                            inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

                            memset(buffer, 0, sizeof(buffer));
                            fgets(buffer, BUFFER_SIZE, stdin);

                            if( strcmp(buffer, "\\q\n") == 0 ){
                                close(sd);
                                printf("%s", login_msg);    // torno al menu iniziale
                                break;                      // chiusura della chat
                            }
                            else if( strcmp(buffer, "\\u\n") == 0 ){
                                memset(buffer, 0, sizeof(buffer));
                                strcpy(buffer, "onlineusers ");
                                strncat(buffer, client_username, strlen(client_username));
                                add_user_flag = false;
                                flag_msg = false;
                            }
                            else if( strncmp(buffer, "\\a", 2) == 0 ){

                                if( controlla_rubrica(buffer) < 0 ){
                                    printf("Questo utente non e' in rubrica\n");
                                    continue;
                                }

                                // chiedo al server di mandarmi la porta dell'utente aggiunto alla chat
                                set_cmd_add_user(buffer);

                                flag_chat = false;
                                for(i = 0; i<num_users; i++){
                                    if( (strcmp(users_group[i].username, username_group) == 0) || (strcmp(chat_username, username_group) == 0) ||
                                        (strcmp(client_username, username_group) == 0) ){
                                        printf("Questo utente fa gia' parte della chat\n");
                                        flag_chat = true;
                                    }
                                }

                                if(flag_chat){
                                    continue;
                                }

                                add_user_flag = true;
                                flag_msg = false;
                            }
                            else if( strncmp(buffer, "share", 5) == 0 ){
                                printf("Non e' possibile condividere file se l'utente e' offline\n");
                                continue;
                            }
                            else{   // si tratta di un messaggio generico
                                memset(message_text, 0, sizeof(message_text));
                                memset(group_buffer, 0, sizeof(group_buffer));
                                strcpy(message_text, buffer);
                                strcpy(group_buffer, buffer);
                                formatta_messaggio(buffer, chat_username);
                                add_user_flag = false;
                                flag_msg = true;
                            }

                            ret = connect(sd, (struct sockaddr*)&server_addr, sizeof(server_addr));
                            if(ret < 0){
                                perror("Errore durante la connect al server ");
                                continue;
                            }

                            len = strlen(buffer) + 1;
                            lmsg = htons(len);

                            ret = send(sd, (void*)&lmsg, sizeof(uint16_t), 0);
                            if(ret < 0){
                                perror("Errore durante l'invio della dimensione ");
                                continue;
                            }

                            ret = send(sd, (void*)&buffer, len, 0);
                            if(ret < 0){
                                perror("Errore durante l'invio del messaggio ");
                                continue;
                            }

                            ret = recv(sd, (void*)&lmsg, sizeof(uint16_t), 0);
                            if(ret < 0){
                                perror("Errore durante la ricezione dimensione ");
                                continue;
                            }

                            len = ntohs(lmsg);
                            memset(buffer, 0, sizeof(buffer));

                            ret = recv(sd, (void*)&buffer, len, 0);
                            if(ret < 0){
                                perror("Errre durante la ricezione messaggio ");
                                continue;
                            }

                            if( (num_users > 0) && (flag_msg) ){
                                for(i = 0; i<num_users; i++){
                                    //printf("%s||%s", users_group[i].username, message_text);

                                    formatta_messaggio(group_buffer, users_group[i].username);

                                    if( send_message(group_buffer, users_group[i].port) ){
                                        // aggiungo un * all'inizio di message_text
                                        memset(aux_msg_text, 0, sizeof(aux_msg_text));
                                        strcpy(aux_msg_text, "*");
                                        strcat(aux_msg_text, message_text);
                                        salva_cronologia(aux_msg_text, users_group[i].username);
                                    }
                                }
                            }

                            if(add_user_flag){
                                if( strcmp(buffer, "notfound") == 0 ){
                                    printf("%s\n", "Impossibile aggiungere questo utente alla chat\n");
                                }
                                else{
                                    // aggiungi utente al gruppo chat
                                    if(num_users < MAX_USERS){

                                        strcpy(utente.username, username_group);
                                        utente.port = strtol(buffer, NULL, 10);
                                        users_group[num_users] = utente;
                                        num_users++;

                                        for(i = 0; i<num_users; i++){
                                            printf("Utenti aggiunti:\n");
                                            printf("%s %d\n", users_group[i].username, users_group[i].port);
                                        }
                                    }
                                    else{
                                        printf("%s\n", "Raggiunto numero massimo di utenti");
                                    }
                                }
                            }
                            else{

                                if( strcmp(buffer, "salvato") == 0 ){
                                    salva_cronologia(message_text, chat_username);
                                }
                                else{
                                    printf("%s\n", buffer);
                                }

                            }
                            close(sd);
                        }

                    }
                }

            }

            if(global_cmd == 6){    // comando out
                close(sd);
                kill(listener_child, SIGKILL);
                break;
            }

            if(global_cmd != 4){
                close(sd);
            }
        }
    }

    return 0;
}