#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>

#define BUFFER_SIZE 1024                        // dimensione massima del buffer applicativo
#define MAX_MSG     16                          // numero massimo di messaggi pendenti
#define MAX_USERS   32                          // numero massimo di utenti online
#define PING_TIME   10                          // periodo per verificare se un peer e' ancora online

typedef enum {false, true} bool;

int global_cmd;                                 // identifica il comando inserito da terminale, 0 = help, 1 = list, etc.

// strutture dati per gestire un utente appena loggato
char* glob_username;
char* glob_password;
char* glob_port;

// strutture dati per gestire gli utenti/device che risultano online
int dim_users;
struct user {
    char username[64];
    char password[64];
    int port;
    char tm_login[64];
    char tm_logout[64];
};

struct user online_users[MAX_USERS];

// strutture dati per gestire il comando hanging
int dim_hanging;
struct hanging {
    char from[64];
    int count_msg;
    char tm_most_recent[64];
};

// strutture dati per gestire il comando show username
char from_username[64];
int dim_arr_message;
struct message {
    char from[64];
    char to[64];
    char timestamp_sent[64];
    char timestamp_received[64];
    char text[BUFFER_SIZE];
};

struct message arr_message[MAX_MSG];

// funzione di utility che controlla la corretteza di un comando di input
int check_cmd(char* buffer)
{
    const char* help = "help";
    const char* list = "list";
    const char* esc = "esc";

    if( strncmp(buffer, help, strlen(help)) == 0 ){
        global_cmd = 0;
        return 1;
    }
    else if( strncmp(buffer, list, strlen(list)) == 0 ){
        global_cmd = 1;
        return 1;
    }
    else if( strncmp(buffer, esc, strlen(esc)) == 0 ){
        global_cmd = 2;
        return 1;
    }
    else{
        return -1;
    }
}

// mostra una breve guida dei comandi sul server
void help()
{
    const char* testo = "list = mostra un elenco degli utenti connessi al servizio, nel formato username*timestamp*porta\n"
                        "esc = terminazione del server, non impedisce alle chat di continuare\n";

    printf("%s", testo);
}

// mostra un elenco degli utenti connessi
void list()
{
    char username[64];
    char password[64];
    char port[16];
    char tm_login[64];
    char tm_logout[64];
    FILE* ptr;
    ptr = fopen("users.txt", "r");

    if(ptr == NULL){
        printf("Errore durante la list\n");
        return;
    }

    while(!feof(ptr)){
        fscanf(ptr, "%s %s %s %s %s\n", username, password, port, tm_login, tm_logout);

        if( strcmp(tm_logout, "0") == 0 ){
            printf("%s*%s*%s\n", username, tm_login, port);
        }
    }

    fclose(ptr);
}

// verifica se un utente e' registrato al servizio o meno
bool is_signup(char* buffer, int len)
{
    char* aux;
    char* token;
    int argn = 0;

    char file_username[64];
    char file_password[64];
    char port[16];
    char tm_login[64];
    char tm_logout[64];
    FILE* ptr;

    aux = strdup(buffer);
    token = strtok(aux, " ");
    while( token != NULL ){

        token = strtok(NULL, " ");
        if( argn == 0 ){
            glob_username = strdup(token);
        }
        if( argn == 1 ){
            glob_password = strdup(token);
        }
        argn++;

    }

    // controllo se l'username fa parte della lista users.txt
    ptr = fopen("users.txt", "r");

    if( ptr == NULL ){
        printf("Errore durante la is_signup");
        return false;
    }

    while( !feof(ptr) ){
        fscanf(ptr, "%s %s %s %s %s\n", file_username, file_password, port, tm_login, tm_logout);

        if( strncmp(file_username, glob_username, strlen(file_username)) == 0 ){
            fclose(ptr);
            return true;
        }       
    }

    fclose(ptr);
    return false;
}

// registro l'utente nel file users.txt
bool register_user()
{
    FILE* ptr;

    ptr = fopen("users.txt", "a");

    if(ptr == NULL){
        printf("Errore durante la register_user\n");
        return false;
    }

    fputs(glob_username, ptr);
    fputs(" ", ptr);
    glob_password = strtok(glob_password, "\n");
    fputs(glob_password, ptr);
    fputs(" 0000 0000 0000\n", ptr);
    fclose(ptr);

    return true;
}

// controlla se le credenziali inserite dall'utente sono corrette
bool check_username_password(char* buffer, int len)
{
    char* aux;
    char* token;
    int argn = 0;
    char file_username[64];
    char file_password[64];
    char port[64];
    char tm_login[64];
    char tm_logout[64];
    FILE* ptr;

    aux = strdup(buffer);
    token = strtok(aux, " ");
    while(token != NULL){
        token = strtok(NULL, " ");

        if(argn == 1){
            glob_username = strdup(token);
        }
        if(argn == 2){
            glob_password = strdup(token);
        }
        if(argn == 3){
            glob_port = strdup(token);
            glob_port = strtok(glob_port, "\n");
        }
        argn++;
    }

    ptr = fopen("users.txt", "r");

    if(ptr == NULL){
        printf("Errore durante la check_username_password\n");
        return false;
    }

    while(!feof(ptr)){
        fscanf(ptr, "%s %s %s %s %s\n", file_username, file_password, port, tm_login, tm_logout);

        if( (strncmp(file_username, glob_username, strlen(file_username)) == 0) && 
            (strncmp(file_password, glob_password, strlen(file_password)) == 0) ){
                fclose(ptr);
                return true;
        }

    }

    fclose(ptr);
    return false;
}

// aggiorna il record utente nel file users.txt, alla login aggiorna timestamp login e porta,
// alla logout, aggiorna timestamp logout
bool aggiorna_utente(char* buffer, int cmd)
{
    char file_username[64];
    char file_password[64];
    char port[64];
    char tm_login[64];
    char new_tm[64];
    char tm_logout[64];
    int month;
    int year;
    char* aux;
    char* token;
    char client_username[64];
    char* new_port;

    FILE* ptr;
    FILE* tmp;

    time_t rawtime;
    struct tm* timeinfo;

    time(&rawtime);
    timeinfo = localtime(&rawtime);
    month = timeinfo->tm_mon;
    month++;
    year = timeinfo->tm_year;

    sprintf(new_tm, "%d-%d-%d,%d:%d:%d",
            timeinfo->tm_mday, month, year,
            timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);

    ptr = fopen("users.txt", "r+");
    tmp = fopen("replace.tmp", "w");

    aux = strdup(buffer);
    if(cmd == 0){
        token = strtok(aux, " ");
        token = strtok(NULL, " ");
        strcpy(client_username, token);       
    }
    if(cmd == 1){
        token = strtok(aux, " ");
        token = strtok(NULL, " ");
        token = strtok(NULL, " ");
        strcpy(client_username, token);
        token = strtok(NULL, " ");
        new_port = strtok(NULL, " ");
        new_port[strcspn(new_port, "\n")] = 0;
    }
    //printf("Aggiorna utente buffer: %s", buffer);

    if( (ptr == NULL) || (tmp == NULL) ){
        printf("Errore durante la aggiorna_utente\n");
        return false;
    } 

    while(!feof(ptr)){
        fscanf(ptr, "%s %s %s %s %s\n", file_username, file_password, port, tm_login, tm_logout);

        if( strncmp(file_username, client_username, strlen(file_username)) == 0 ){
            if(cmd == 0){   // comando out
                fprintf(tmp, "%s %s %s %s %s\n", file_username, file_password, port, tm_login, new_tm);
            }
            else{   // comando in
                fprintf(tmp, "%s %s %s %s %s\n", file_username, file_password, new_port, new_tm, "0");
            }
        }
        else{
            fprintf(tmp, "%s %s %s %s %s\n", file_username, file_password, port, tm_login, tm_logout);
        }   
    }

    fclose(ptr);
    fclose(tmp);

    remove("users.txt");
    rename("replace.tmp", "users.txt");

    return true;
}

// legge la lista degli utenti che hanno inviato messaggi a username mentre era offline
void leggi_messaggi_pendenti(char* buffer)
{
    char text[BUFFER_SIZE];
    char* errore = "Errore durante la lettura del file\0";
    char* no_msg = "Non ci sono messaggi pendenti\n";

    FILE* ptr;

    char client_username[64];   // username dell'utente che ha richiesto il comando
    char aux[64];
    char* token;

    char most_recent[64];
    char from[64];
    int i, j;                   // per cicli for
    int dim_hanging = 0;        // all'inizio la lista hanging e' vuota
    char supp[BUFFER_SIZE];
    char* token_file;
    int ind_msg;
    bool hanging_flag;

    char file_from[64];
    char file_to[64];
    char file_timestamp[64];
    char messaggio_hanging[64];

    // strutture dati per gestire la lettura/scrittura dei messaggi dentro il file
    struct hanging messaggi_pendenti[10];
    struct hanging row_messaggio_pendente;

    strcpy(aux, buffer);
    token = strtok(aux, " ");
    token = strtok(NULL, " ");
    strcpy(client_username, token);

    ptr = fopen("messages.txt", "r");

    if(ptr == NULL){
        strncpy(buffer, errore, strlen(errore));
        return;
    }

    // controllo se il file e' vuoto
    fseek(ptr, 0, SEEK_END);
    if( ftell(ptr) == 0 ){
        strcpy(buffer, "Non sono presenti messaggi pendenti\n");
        return;
    }
    fseek(ptr, 0, SEEK_SET);
    
    memset(messaggi_pendenti, 0, sizeof(messaggi_pendenti));
    memset(buffer, 0, BUFFER_SIZE);

    while(!feof(ptr)){
        fscanf(ptr, "%[^\n]\n", text);

        strcpy(supp, text);
        token_file = strtok(supp, "||");
        ind_msg = 0;
        memset(file_to, 0, sizeof(file_to));
        memset(file_from, 0, sizeof(file_from));
        memset(file_timestamp, 0, sizeof(file_timestamp));

        // prendo i singoli campi di una riga del file messages.txt
        while(token_file != NULL){

            if(ind_msg == 1){
                strcpy(from, token_file);
                strcpy(file_from, token_file);
            }
            if(ind_msg == 2){
                strcpy(file_to, token_file);
            }
            if(ind_msg == 3){
                strcpy(most_recent, token_file);
                strcpy(file_timestamp, token_file);
            }

            token_file = strtok(NULL, "||");
            ind_msg++;
        }

        // se il messaggio e' destinato al device che ha richiesto il comando, aggiungo il messaggio alla lista
        if( strncmp(file_to, client_username, strlen(file_to)) == 0 ){
            hanging_flag = false;

            if(dim_hanging == 0){
                strcpy(row_messaggio_pendente.from, file_from);
                row_messaggio_pendente.count_msg = 1;
                strcpy(row_messaggio_pendente.tm_most_recent, file_timestamp);
                messaggi_pendenti[dim_hanging] = row_messaggio_pendente;
                dim_hanging++;
            }
            else{
                for(j = 0; j<dim_hanging; j++){
                    if( strcmp(messaggi_pendenti[j].from, file_from) == 0 ){
                        hanging_flag = true;
                    }
                }

                if(!hanging_flag){
                    strcpy(row_messaggio_pendente.from, file_from);
                    row_messaggio_pendente.count_msg = 1;
                    strcpy(row_messaggio_pendente.tm_most_recent, file_timestamp);
                    messaggi_pendenti[dim_hanging] = row_messaggio_pendente;
                    dim_hanging++;
                }
                else{
                    for(i = 0; i<dim_hanging; i++){
                        if( strcmp(messaggi_pendenti[i].from, file_from) == 0 ){
                            messaggi_pendenti[i].count_msg++;
                            memset(messaggi_pendenti[i].tm_most_recent, 0, sizeof(messaggi_pendenti[i].tm_most_recent));
                            strcpy(messaggi_pendenti[i].tm_most_recent, file_timestamp);
                        }
                    }
                }
            }
        }
    }

    // se ci sono messaggi per l'utente, scrivo la lista nel buffer
    memset(buffer, 0, BUFFER_SIZE);
    if(dim_hanging > 0){
        for(i = 0; i<dim_hanging; i++){
            sprintf(messaggio_hanging, "%s %d %s\n", messaggi_pendenti[i].from, messaggi_pendenti[i].count_msg, messaggi_pendenti[i].tm_most_recent);
            strcat(buffer, messaggio_hanging);
        }
    }
    else{
        strncat(buffer, no_msg, strlen(no_msg));
    }

    fclose(ptr);
    //printf("%s", buffer);
}

// scrive dentro buffer lo stato dell'utente
bool user_is_online(char* buffer)
{
    char file_username[64];
    char file_password[64];
    char file_porta[64];
    char file_tm_login[64];
    char file_tm_logout[64];
    char* aux;
    char* token;
    char dest_username[64];
    char* errore = "Errore durante la lettura del file\0";
    char* no_msg = "Questo utente non e' registrato al servizio\n";
    char* offline_cond = "0";
    char* offline = "offline";
    char* online = "online";
    FILE* ptr;

    ptr = fopen("users.txt", "r");

    if( ptr == NULL ){
        strncpy(buffer, errore, strlen(errore));
        return false;
    }

    aux = strdup(buffer);
    token = strtok(aux, " ");
    token = strtok(NULL, " ");
    strcpy(dest_username, token);

    while(!feof(ptr)){
        fscanf(ptr, "%s %s %s %s %s\n", file_username, file_password, file_porta, file_tm_login, file_tm_logout);

        if( (strncmp(file_username, dest_username, strlen(file_username)) == 0) &&
            (strncmp(file_tm_logout, offline_cond, strlen(file_tm_logout)) == 0) ){
            strncat(buffer, online, strlen(online));
            fclose(ptr);
            return true;
        }       

        if( (strncmp(file_username, dest_username, strlen(file_username)) == 0) &&
            (strncmp(file_tm_logout, offline_cond, strlen(file_tm_logout)) != 0) ){
            strncat(buffer, offline, strlen(offline));
            strncat(buffer, " ", 1);
            strncat(buffer, file_porta, strlen(file_porta));
            strncat(buffer, "\n", 1);
            fclose(ptr);
            return false;
        }
    }

    if(strlen(buffer) == 0){
        strncat(buffer, no_msg, strlen(no_msg));
    }
    fclose(ptr);
    return false;
}

// scrive dentro buffer la porta dell'utente richiesto
void get_user_port(char* buffer)
{
    char file_username[64];
    char file_password[64];
    char file_porta[64];
    char file_tm_login[64];
    char file_tm_logout[64];

    char* aux;
    char* token;
    char dest_username[64];
    char* errore = "Errore durante la lettura del file\0";

    FILE* ptr;

    ptr = fopen("users.txt", "r");

    if(ptr == NULL){
        strncpy(buffer, errore, strlen(errore));
        return;
    }

    aux = strdup(buffer);
    token = strtok(aux, " ");
    token = strtok(NULL, " ");
    strcpy(dest_username, token);
    //printf("Get user port: %s\n", dest_username);

    memset(buffer, 0, BUFFER_SIZE);
    while(!feof(ptr)){
        fscanf(ptr, "%s %s %s %s %s\n", file_username, file_password, file_porta, file_tm_login, file_tm_logout);

        if( strncmp(file_username, dest_username, strlen(file_username)) == 0 ){
            strncat(buffer, file_porta, strlen(file_porta));
            fclose(ptr);
            return;
        }       
    }

    fclose(ptr);
}

// controlla l'esistenza di un utente in users.txt
bool user_exists(char* buffer)
{
    char file_username[64];
    char file_password[64];
    char file_porta[64];
    char file_tm_login[64];
    char file_tm_logout[64];

    char* aux;
    char* token;
    char dest_username[64];
    char* errore = "0000 Errore\0";

    FILE* ptr;

    ptr = fopen("users.txt", "r");

    if(ptr == NULL){
        strncpy(buffer, errore, strlen(errore));
        return false;
    }

    aux = strdup(buffer);
    token = strtok(aux, " ");
    token = strtok(NULL, " ");
    strcpy(dest_username, token);
    //printf("User exists: %s\n", dest_username);

    while(!feof(ptr)){
        fscanf(ptr, "%s %s %s %s %s\n", file_username, file_password, file_porta, file_tm_login, file_tm_logout);

        if( strncmp(file_username, dest_username, strlen(file_username)) == 0 ){
            fclose(ptr);
            return true;
        }
    }

    fclose(ptr);
    return false;
}

// scrive dentro buffer gli utenti online
void get_online_users(char* buffer)
{
    char file_username[64];
    char file_password[64];
    char file_porta[64];
    char file_tm_login[64];
    char file_tm_logout[64];
    char* aux;
    char* user_cmd;
    char* errore = "Errore durante la lettura del file\0";
    char* no_msg = "Non ci sono utenti online";
    FILE* ptr;

    aux = strdup(buffer);
    user_cmd = strtok(aux, " ");
    user_cmd = strtok(NULL, " ");

    ptr = fopen("users.txt", "r");
    memset(buffer, 0, BUFFER_SIZE);

    if(ptr == NULL){
        strncpy(buffer, errore, strlen(errore));
        return;
    }

    while(!feof(ptr)){
        fscanf(ptr, "%s %s %s %s %s\n", file_username, file_password, file_porta, file_tm_login, file_tm_logout);

        if( (strncmp(file_tm_logout, "0", strlen(file_tm_logout)) == 0) && (strncmp(file_username, user_cmd, strlen(user_cmd)) != 0) ){
            strncat(buffer, file_username, strlen(file_username));
            strncat(buffer, " ", 1);
        }
    }

    if(strlen(buffer) == 0){
        strncat(buffer, no_msg, strlen(no_msg));
    }
    fclose(ptr);
}

// bufferizza i messaggi pendenti
void salva_messaggio(char* buffer)
{
    FILE* ptr;

    ptr = fopen("messages.txt", "a");

    if(ptr == NULL){
        printf("Errore durante la salva_messaggio\n");
        return;
    }

    fputs(buffer, ptr);
    fclose(ptr);

    memset(buffer, 0, BUFFER_SIZE);
    strcpy(buffer, "salvato");
}

// controlla se ci sono messaggi per il client da parte dell'username inserito
void check_messages(char* buffer)
{
    char aux[64];
    char* token;
    char* client_username;

    char messaggio[BUFFER_SIZE];
    char file_from[64];
    char file_to[64];
    char file_tm_sent[64];
    char file_tm_received[64];
    char file_text[BUFFER_SIZE];
    char* msg_token;
    int i = 0;
    struct message tmp_msg;

    FILE* ptr;

    int month, year;
    time_t rawtime;
    struct tm* timeinfo;

    time(&rawtime);
    timeinfo = localtime(&rawtime);
    month = timeinfo->tm_mon;
    month++;
    year = timeinfo->tm_year;

    sprintf(file_tm_received, "%d-%d-%d,%d:%d:%d",
            timeinfo->tm_mday, month, year,
            timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);

    ptr = fopen("messages.txt", "r");

    if(ptr == NULL){
        printf("Errore durante la check_messages\n");
        exit(-1);
    }

    strcpy(aux, buffer);
    token = strtok(aux, " ");
    token = strtok(NULL, " ");
    client_username = strtok(NULL, " ");
    //printf("check_messages -> from: %s, to: %s", token, client_username);
    strcpy(from_username, token);

    dim_arr_message = 0;
    memset(arr_message, 0, sizeof(arr_message));
    while(!feof(ptr)){
        fscanf(ptr, "%[^\n]\n", messaggio);

        msg_token = strtok(messaggio, "||");
        i = 0;
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
                //strcpy(file_tm_received, msg_token)
            }
            if(i == 5){
                strcpy(file_text, msg_token);
            }

            msg_token = strtok(NULL, "||");
            i++;
        }

        if( (strncmp(file_from, token, strlen(file_from)) == 0) && (strncmp(file_to, client_username, strlen(file_to)) == 0) ){
            strcpy(tmp_msg.from, file_from);
            strcpy(tmp_msg.to, file_to);
            strcpy(tmp_msg.timestamp_sent, file_tm_sent);
            strcpy(tmp_msg.timestamp_received, file_tm_received);
            strcpy(tmp_msg.text, file_text);
            arr_message[dim_arr_message] = tmp_msg;
            dim_arr_message++;
        }
    }
    //printf("totale messaggi: %d\n", dim_arr_message);
    fclose(ptr);
}

// invia la notifica ad un utente, se l'utente e' offline, viene bufferizzata nel server
void send_notification(char* to_username)
{
    char file_username[64];
    char file_password[64];
    char file_porta[64];
    char peer_porta[64];
    char file_tm_login[64];
    char file_tm_logout[64];
    char str_tmp[64];
    bool online;

    int ret, sd, len;
    uint16_t lmsg;
    struct sockaddr_in peer_addr;

    FILE* ptr;
    FILE* tmp;

    ptr = fopen("users.txt", "r");

    if(ptr == NULL){
        printf("Errore durante la send_notification\n");
        exit(-1);
    }

    online = false;
    while(!feof(ptr)){
        fscanf(ptr, "%s %s %s %s %s\n", file_username, file_password, file_porta, file_tm_login, file_tm_logout);

        if( (strcmp(file_tm_logout, "0") == 0) && (strcmp(file_username, from_username) == 0) ){
            online = true;
            strcpy(peer_porta, file_porta);
        }
    }

    if(online){     // utente online, invia la notifica
        //printf("utente %s online\n", from_username);
        sd = socket(AF_INET, SOCK_STREAM, 0);
        memset(&peer_addr, 0, sizeof(peer_addr));
        peer_addr.sin_family = AF_INET;
        peer_addr.sin_port = htons(strtol(peer_porta, NULL, 10));
        inet_pton(AF_INET, "127.0.0.1", &peer_addr.sin_addr);

        ret = connect(sd, (struct sockaddr*)&peer_addr, sizeof(peer_addr));
        if(ret < 0){
            perror("Errore durante la connect: ");
            exit(-1);
        }

        sprintf(str_tmp, "notifica||%s||%s||l'utente %s ha letto i tuoi messaggi\n", to_username, from_username, to_username);
        len = strlen(str_tmp) + 1;
        lmsg = htons(len);

        ret = send(sd, (void*)&lmsg, sizeof(uint16_t), 0);
        if(ret < 0){
            perror("Errore durante l'invio della dimensione: ");
            exit(-1);
        }

        ret = send(sd, (void*)&str_tmp, len, 0);
        if(ret < 0){
            perror("Errore durante l'invio del messaggio: ");
            exit(-1);
        }

        close(sd);
    }
    else{   // utente offline, salvo la notifica nel server
        //printf("utente %s offline\n", from_username);
        tmp = fopen("notifications.txt", "a");

        if(tmp == NULL){
            printf("Errore durante la send_notification\n");
            exit(-1);
        }

        sprintf(str_tmp, "notifica||%s||%s||l'utente %s ha letto i tuoi messaggi\n", to_username, from_username, to_username);
        fputs(str_tmp, tmp);
        fclose(tmp);
    }

    fclose(ptr);
}

// cancella i messaggi pendenti che sono stati letti da un device
void delete_messages(char* from_username, char* to_username)
{
    char row[BUFFER_SIZE];
    char file_from[64];
    char file_to[64];
    char file_tm_sent[64];
    char file_tm_received[64];
    char file_text[BUFFER_SIZE];
    char* file_token;
    int i = 0;

    FILE* ptr;
    FILE* tmp;

    ptr = fopen("messages.txt", "r");
    tmp = fopen("replace.tmp", "w");

    if( (ptr == NULL) || (tmp == NULL) ){
        printf("Errore durante la delete_messages\n");
        exit(-1);
    }

    while(!feof(ptr)){
        fscanf(ptr, "%[^\n]\n", row);

        file_token = strtok(row, "||");
        i = 0;
        while(file_token != NULL){
            if(i == 1){
                strcpy(file_from, file_token);
            }
            if(i == 2){
                strcpy(file_to, file_token);
            }
            if(i == 3){
                strcpy(file_tm_sent, file_token);
            }
            if(i == 4){
                strcpy(file_tm_received, file_token);
            }
            if(i == 5){
                strcpy(file_text, file_token);
            }

            file_token = strtok(NULL, "||");
            i++;
        }

        if( (strncmp(file_from, from_username, strlen(file_from)) == 0) && (strncmp(file_to, to_username, strlen(file_to)) == 0) ){
            continue;
        }
        else{
            fprintf(tmp, "message||%s||%s||%s||%s||%s\n", file_from, file_to, file_tm_sent, file_tm_received, file_text);
        }
    }

    fclose(ptr);
    fclose(tmp);

    remove("messages.txt");
    rename("replace.tmp", "messages.txt");
}

// cancella le notifiche bufferizzate nel server
void delete_notifications(char* from, char* to)
{
    char row[256];
    char file_from[64];
    char file_to[64];
    char file_text[64];
    char* file_token;
    int i = 0;

    FILE* ptr;
    FILE* tmp;

    ptr = fopen("notifications.txt", "r");
    tmp = fopen("replace.tmp", "w");

    if( (ptr == NULL) || (tmp == NULL) ){
        printf("Errore durante la delete_notifications\n");
        exit(-1);
    }

    while(!feof(ptr)){
        fscanf(ptr, "%[^\n]\n", row);

        file_token = strtok(row, "||");
        i = 0;
        while(file_token != NULL){
            if(i == 1){
                strcpy(file_from, file_token);
            }
            if(i == 2){
                strcpy(file_to, file_token);
            }
            if(i == 3){
                strcpy(file_text, file_token);
            }

            file_token = strtok(NULL, "||");
            i++;
        }

        if( (strncmp(file_from, from, strlen(file_from)) == 0) && (strncmp(file_to, to, strlen(file_to)) == 0) ){
            continue;
        }
        else{
            fprintf(tmp, "notifica||%s||%s||%s\n", file_from, file_to, file_text);
        }
    }

    fclose(ptr);
    fclose(tmp);

    remove("notifications.txt");
    rename("replace.tmp", "notifications.txt");
}

// controlla se sono presenti notifiche per l'utente il cui username e' dentro cmd
void check_notifications(char* cmd, char* buffer)
{
    char aux[64];
    char from_username[64];
    char client_username[64];
    char* token;

    char row[256];
    char file_from[64];
    char file_to[64];
    char file_text[64];
    char* file_token;
    bool found;
    int i;

    FILE* ptr;

    ptr = fopen("notifications.txt", "r");

    if(ptr == NULL){
        printf("Errore durante la check_notifications\n");
        return;
    }

    strcpy(aux, cmd);
    token = strtok(aux, " ");
    token = strtok(NULL, " ");
    strcpy(from_username, token);
    token = strtok(NULL, " ");
    strcpy(client_username, token);

    //printf("Notifica da %s per %s", from_username, client_username);

    found = false;
    i = 0;
    fseek(ptr, 0, SEEK_END);
    if( ftell(ptr) == 0 ){
        //printf("File vuoto\n");
    }
    else{
        fseek(ptr, 0, SEEK_SET);

        while(!feof(ptr)){
            fscanf(ptr, "%[^\n]\n", row);

            file_token = strtok(row, "||");
            while(file_token != NULL){
                if(i == 1){
                    strcpy(file_from, file_token);
                }
                if(i == 2){
                    strcpy(file_to, file_token);
                }
                if(i == 3){
                    strcpy(file_text, file_token);
                }
                
                file_token = strtok(NULL, "||");
                i++;
            }

            if( (strncmp(file_from, from_username, strlen(file_from)) == 0) && (strncmp(file_to, client_username, strlen(file_to)) == 0) ){
                found = true;
            }
        }
    }

    if(found){
        delete_notifications(from_username, client_username);
        strcat(buffer, " s-notifica");
    }
    else{
        strcat(buffer, " n-notifica");
    }

    fclose(ptr);
}

// modifica la struttura dati relativa agli utenti online, se uno di questi risulta esserlo nel file users.txt
void set_online_users()
{
    FILE* ptr;
    char username[64];
    char password[64];
    char port[16];
    char tm_login[64];
    char tm_logout[64];

    ptr = fopen("users.txt", "r");

    if(ptr == NULL){
        printf("Errore durante la lettura del file users.txt\n");
        return;
    }

    dim_users = 0;
    while(!feof(ptr)){
        fscanf(ptr, "%s %s %s %s %s\n", username, password, port, tm_login, tm_logout);

        if( strncmp(tm_logout, "0", strlen(tm_logout)) == 0 ){
            strcpy(online_users[dim_users].username, username);
            strcpy(online_users[dim_users].password, password);
            online_users[dim_users].port = strtol(port, NULL, 10);
            strcpy(online_users[dim_users].tm_login, tm_login);
            strcpy(online_users[dim_users].tm_logout, tm_logout);
            dim_users++;
        }
    }

    fclose(ptr);
}

// invia un pacchetto "ping" ai peer che risultano online
int send_ping()
{
    struct sockaddr_in peer_addr;
    int i, peer_sd, ret, len;
    uint16_t lmsg;
    char msg[64];
    char record[128];

    for(i = 0; i<dim_users; i++){
        //printf("User online: %s %d\n", online_users[i].username, online_users[i].port);
        sprintf(record, "test %s %s %d %s %s\n", online_users[i].username, online_users[i].password,
                online_users[i].port, online_users[i].tm_login, online_users[i].tm_logout);

        peer_sd = socket(AF_INET, SOCK_STREAM, 0);
        memset(&peer_addr, 0, sizeof(peer_addr));
        peer_addr.sin_family = AF_INET;
        peer_addr.sin_port = htons(online_users[i].port);
        inet_pton(AF_INET, "127.0.0.1", &peer_addr.sin_addr);

        ret = connect(peer_sd, (struct sockaddr*)&peer_addr, sizeof(peer_addr));
        if(ret < 0){
            //perror("Errore durante la connect ");
            printf("Il peer %s e' andato offline\n", online_users[i].username);
            aggiorna_utente(record, 0);
            continue;
        }

        strcpy(msg, "test||test");
        len = strlen(msg) + 1;
        lmsg = htons(len);

        ret = send(peer_sd, (void*)&lmsg, sizeof(uint16_t), 0);
        if(ret < 0){
            //perror("Errore durante l'invio dimensione: ");
            printf("Il peer %s e' andato offline\n", online_users[i].username);
            aggiorna_utente(record, 0);
            continue;
        }

        ret = send(peer_sd, (void*)&msg, len, 0);
        if(ret < 0){
            //perror("Errore durante l'invio del messaggio: ");
            printf("Il peer %s e' andato offline\n", online_users[i].username);
            aggiorna_utente(record, 0);
            continue;
        }

        ret = recv(peer_sd, (void*)&lmsg, sizeof(uint16_t), 0);
        if(ret < 0){
            printf("Il peer %s e' andato offline\n", online_users[i].username);
            aggiorna_utente(record, 0);
           continue;
        }

        len = ntohs(lmsg);

        ret = recv(peer_sd, (void*)&msg, len, 0);
        if(ret < 0){
            printf("Il peer %s e' andato offline\n", online_users[i].username);
            aggiorna_utente(record, 0);
            continue;
        }

        close(peer_sd);
    }

    return 0;
}

// aggiorna il timestamp logout di un peer se questo, dopo la send_ping, e' risultato offline
void aggiorna_timestamp_logout(char* buffer)
{
    char new_tm_logout[64];
    char* aux;
    char* token;
    char client_username[64];

    int i;

    aux = strdup(buffer);
    token = strtok(aux, " ");
    strcpy(new_tm_logout, token);
    token = strtok(NULL, " ");
    strcpy(client_username, token);
    //printf("aggiorna_timestamp_logout: %s %s\n", client_username, new_tm_logout);

    if( dim_users > 0 ){
        for(i = 0; i<dim_users; i++){
            if( strncmp(online_users[i].username, client_username, strlen(client_username)) == 0 ){
                strcpy(online_users[i].tm_logout, new_tm_logout);
            }
        }
    }
}

int main(int argc, char* argv[])
{
    int ret;                                // per gestire l'output delle system call
    int sd, new_sd;                         // per gestire socket di ascolto e comunicazione 
    int len;                                // lunghezza di un messaggio
    const int opt = 1;
    socklen_t s_len;                        // lunghezza del socket
    uint16_t lmsg;                          // dimensione di un messaggio (utilizzato nelle system call)
    int32_t totale_messaggi;                // numero totale dei messaggi pendenti
    char buffer[BUFFER_SIZE];               // buffer a livello applicativo
    char aux[BUFFER_SIZE];                  // buffer ausiliario
    char* porta;                            // porta del server

    pid_t listener_child, ping_child;

    // strutture dati ausiliarie per gestire i comandi ricevuti dai client
    char* test;
    char msg_buffer[BUFFER_SIZE];
    char* msg_aux;
    char* supp;
    char to_username[64];

    struct sockaddr_in my_addr, client_addr;

    const char* invalid_arg = "Argomento non valido";
    const char* benvenuto = "*** SERVER STARTED ***\nDigita un comando:\n\n1) help --> mostra i dettagli dei comando\n"
                            "2) list --> mostra un elenco degli utenti connessi\n"
                            "3) esc --> chiude il server\n";
    const char* signup = "signup";
    const char* in = "in";
    const char* hanging = "hanging";
    const char* show = "show";
    const char* chat = "chat";
    const char* out = "out";
    const char* err_signup = "Utente gia' registrato";
    const char* noerr_signup = "Utente registrato con successo";
    const char* err_login = "Credenziali non valide";
    const char* noerr_code = "conferma";
    const char* online = "online";
    const char* offline = "offline";
    char* notfound = "0000 notfound";

    int i;  

    if(argc == 2){
        porta = strdup(argv[1]);
    }
    else if(argc > 2){
        printf("%s\n", invalid_arg);
        exit(-1);
    }
    else{
        porta = "4242";
    }

    sd = socket(AF_INET, SOCK_STREAM, 0);
    memset(&my_addr, 0, sizeof(my_addr));
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(strtol(porta, NULL, 10));
    my_addr.sin_addr.s_addr = INADDR_ANY;

    setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    ret = bind(sd, (struct sockaddr*)&my_addr, sizeof(my_addr));
    if(ret < 0){
        perror("Errore durante la bind: ");
        exit(-1);
    }

    ret = listen(sd, 10);
    if(ret < 0){
        perror("Errore durante la listen: ");
        exit(-1);
    }

    printf("%s", benvenuto);

    // processo figlio che si occupa di monitorare lo stato dei client
    ping_child = fork();
    if(ping_child == 0){
        close(sd);

        while(1){
            sleep(PING_TIME);

            // inizializzo l'array di struct user per mandare un pacchetto agli utenti online
            set_online_users();
            
            send_ping();
        }
    }

    // processo figlio che si occupa di servire iterativamente le richieste dei client
    listener_child = fork();

    while(1){

        if(listener_child == 0){

            s_len = sizeof(client_addr);
            new_sd = accept(sd, (struct sockaddr*)&client_addr, &s_len);

            while(1){
                memset(buffer, 0, sizeof(buffer));

                ret = recv(new_sd, (void*)&lmsg, sizeof(uint16_t), 0);
                if(ret < 0){
                    perror("Errore durante la receive: ");
                    continue;
                }

                len = ntohs(lmsg);

                ret = recv(new_sd, (void*)&buffer, len, 0);
                if(ret < 0){
                    perror("Errore durante la ricezione del messaggio: ");
                    continue;
                }

                strncpy(aux, buffer, len);
                strncpy(msg_buffer, buffer, len);
                test = strtok(aux, " ");
                msg_aux = strtok(msg_buffer, "||");

                // il device ha inviato il comando signup
                if( strncmp(test, signup, strlen(signup)) == 0 ){

                    if( !is_signup(buffer, len) ){  // se non e' registrato, lo registro
                        
                        if( register_user() ){
                            printf("%s\n", noerr_signup);
                            strncpy(buffer, noerr_signup, strlen(noerr_signup));
                        }
                        else{
                            printf("%s\n", "Errore durante la registrazione");
                            strncpy(buffer, err_signup, strlen(err_signup));
                        }

                    }
                    else{   // altrimenti e' gia' registrato
                        printf("%s\n", err_signup);
                    }

                }

                // il device ha inviato il comando in
                if( strncmp(test, in, strlen(in)) == 0 ){
                    // controllo la validita' di username e password
                    if( check_username_password(buffer, len) ){ 
                        // aggiorno i campi porta e timestamp login
                        if( !aggiorna_utente(buffer, 1) ){
                            printf("%s\n", "Errore durante aggiornamento record utente");
                        }
                        else{
                            //printf("%s\n", noerr_code);
                            strncpy(buffer, noerr_code, strlen(noerr_code));
                        }
                    }
                    else{   // credenziali errate
                        printf("%s\n", err_login);
                        strncpy(buffer, err_login, strlen(err_login));
                    }
                }

                // se i comandi inviati dal device sono in o signup, restituisco al client la conferma
                if( (strncmp(test, in, strlen(in)) == 0) || 
                    (strncmp(test, signup, strlen(signup)) == 0) ){

                    len = strlen(buffer) + 1;
                    lmsg = htons(len);

                    ret = send(new_sd, (void*)&lmsg, sizeof(uint16_t), 0);
                    if(ret < 0){
                        perror("Errore durante l'invio dimensione: ");
                        continue;
                    }

                    ret = send(new_sd, (void*)&buffer, len, 0);
                    if(ret < 0){
                        perror("Errore durante l'invio del messaggio: ");
                        continue;
                    }

                    // se il comando e' "in", riceve il timestamp logout del device
                    if( strncmp(test, in, strlen(in)) == 0 ){

                        ret = recv(new_sd, (void*)&lmsg, sizeof(uint16_t), 0);
                        if(ret < 0){
                            perror("Errore durante la receive tm_logout del peer ");
                            continue;
                        }

                        len = ntohs(lmsg);

                        ret = recv(new_sd, (void*)&buffer, len, 0);
                        if(ret < 0){
                            perror("Errore durante la receive tm_logout del peer");
                            continue;
                        }

                        //printf("tm_logout device: %s\n", buffer);
                        aggiorna_timestamp_logout(buffer);          // il messaggio arriva nel formato: "tm_logout username"
                    }
                }

                // il device ha inviato il comando hanging
                if( strncmp(test, hanging, strlen(hanging)) == 0 ){
                    //printf("%s", buffer);
                    // scrivo nel buffer la lista dei messaggi pendenti del device
                    leggi_messaggi_pendenti(buffer);

                    len = strlen(buffer) + 1;
                    lmsg = htons(len);

                    ret = send(new_sd, (void*)&lmsg, sizeof(uint16_t), 0);
                    if(ret < 0){
                        perror("Errore durante l'invio della dimensione: ");
                        continue;
                    }

                    ret = send(new_sd, (void*)&buffer, len, 0);
                    if(ret < 0){
                        perror("Errore durante l'invio del messaggio: ");
                        continue;
                    }
                }

                // il device ha inviato il comando show 'username'
                if( strncmp(test, show, strlen(show)) == 0 ){
                    /*  Controllo se ci sono messaggi pendenti in messages.txt:
                        se non ci sono -> il server restituisce nessun messaggio presente
                        se ci sono -> invia i messaggi pendenti al client destinatario:
                            se client mittente online -> invia notifica
                            se client mittente offline -> mantieni notifica nel server */
                    totale_messaggi = 0;
                    dim_arr_message = 0;
                    check_messages(buffer);
                    if( dim_arr_message > 0 ){  // sono presenti messaggi pendenti

                        totale_messaggi = htonl(dim_arr_message);

                        ret = send(new_sd, (void*)&totale_messaggi, sizeof(int32_t), 0);
                        if(ret < 0){
                            perror("Errore durante l'invio del numero totale di messaggi: ");
                            continue;
                        }

                        for(i = 0; i<dim_arr_message; i++){
                            sprintf(buffer, "message||%s||%s||%s||%s||%s\n", arr_message[i].from,
                                    arr_message[i].to, arr_message[i].timestamp_sent,
                                    arr_message[i].timestamp_received,
                                    arr_message[i].text);
                            sprintf(to_username, arr_message[i].to);
                            //printf("%s", buffer);

                            len = strlen(buffer) + 1;
                            lmsg = htons(len);

                            ret = send(new_sd, (void*)&lmsg, sizeof(uint16_t), 0);
                            if(ret < 0){
                                perror("Errore durante l'invio della dimensione singolo mess: ");
                                continue;
                            }

                            ret = send(new_sd, (void*)&buffer, len, 0);
                            if(ret < 0){
                                perror("Errore durante l'invio del singolo mess: ");
                                continue;
                            }
                        }

                        //printf("Utente a cui mandare la notifica: %s\n", from_username);
                        // se il device mittente e' online, invio la notifica 
                        send_notification(to_username);
                        // cancello i messaggi dal buffer/file messages.txt
                        delete_messages(from_username, to_username);
                    }
                    else{   // non sono presenti messaggi pendenti
                        //printf("Non ci sono messaggi da parte di questo utente\n");

                        totale_messaggi = htonl(dim_arr_message);

                        ret = send(new_sd, (void*)&totale_messaggi, sizeof(int32_t), 0);
                        if(ret < 0){
                            perror("Errore durante l'invio del numero totale di mess: ");
                            continue;
                        }
                    }
                }

                // il device ha inviato il comando chat 'username'
                if( strncmp(test, chat, strlen(chat)) == 0 ){

                    memset(aux, 0, sizeof(aux));
                    strcpy(aux, buffer);
                    // controllo l'esistenza dell'utente 
                    if( user_exists(buffer) ){
                        // controllo se l'utente e' offline/online
                        if( user_is_online(buffer) ){
                            // scrivo in buffer la porta su cui e' in ascolto l'utente
                            get_user_port(buffer);
                            strncat(buffer, " ", 1);
                            strncat(buffer, online, strlen(online));
                        }
                        else{   // utente offline, mando la porta del server
                            memset(buffer, 0, sizeof(buffer));
                            strncat(buffer, porta, strlen(porta));
                            strncat(buffer, " ", 1);
                            strncat(buffer, offline, strlen(offline));
                        }
                        // controllo se ci sono notifiche per il device che ha aperto la chat
                        check_notifications(aux, buffer);
                    }
                    else{
                        memset(buffer, 0, sizeof(buffer));
                        strncpy(buffer, "0000 ", 4);
                        strncpy(buffer, notfound, strlen(notfound));
                    }
                    //printf("Dopo user exists: %s\n", buffer);

                    len = strlen(buffer) + 1;
                    lmsg = htons(len);

                    ret = send(new_sd, (void*)&lmsg, sizeof(uint16_t), 0);
                    if(ret < 0){
                        perror("Errore durante l'invio della dimensione mess: ");
                        continue;
                    }

                    ret = send(new_sd, (void*)&buffer, len, 0);
                    if(ret < 0){
                        perror("Errore durante l'invio del messaggio: ");
                        continue;
                    }

                }

                // il device ha inviato il comando out
                if( strncmp(test, out, strlen(out)) == 0 ){
                    // aggiorno la entry relativa al device che ha effettuato il logout
                    printf("%s", buffer);
                    aggiorna_utente(buffer, 0);
                }

                /* SOTTOCOMANDI DI CHAT: \u -> onlineusers, \a username -> add username */
                // il device ha chiesto di sapere quali sono gli utenti online
                if( strncmp(test, "onlineusers", strlen(test)) == 0 ){
                    get_online_users(buffer);

                    len = strlen(buffer) + 1;
                    lmsg = htons(len);

                    ret = send(new_sd, (void*)&lmsg, sizeof(uint16_t), 0);
                    if(ret < 0){
                        perror("Errore durante l'invio della dimensione del messaggio: ");
                        continue;
                    }

                    ret = send(new_sd, (void*)&buffer, len, 0);
                    if(ret < 0){
                        perror("Errore durante l'invio del messaggio di users online: ");
                        continue;
                    }
                }

                // il device ha chiesto di aggiungere un utente online ad una chat
                if( strncmp(test, "add", strlen(test)) == 0 ){
                    supp = strdup(buffer);

                    if( user_is_online(supp) ){
                        get_user_port(buffer);
                    }
                    else{
                        memset(buffer, 0, sizeof(buffer));
                        strcpy(buffer, "notfound");
                    }
                    //printf("%s\n", buffer);

                    len = strlen(buffer) + 1;
                    lmsg = htons(len);

                    ret = send(new_sd, (void*)&lmsg, sizeof(uint16_t), 0);
                    if(ret < 0){
                        perror("Errore durante l'invio della dimensione mess: ");
                        continue;
                    }

                    ret = send(new_sd, (void*)&buffer, len, 0);
                    if(ret < 0){
                        perror("Errore durante l'invio del mess: ");
                        continue;
                    }
                }

                // il device ha inviato un messaggio all'interno di una chat ad un peer offline
                if( strncmp(msg_aux, "message", strlen(msg_aux)) == 0 ){
                    //printf("%s", buffer);

                    salva_messaggio(buffer);

                    if( strcmp(buffer, "salvato") == 0 ){
                        printf("Messaggio salvato con successo\n");
                    }

                    len = strlen(buffer) + 1;
                    lmsg = htons(len);

                    ret = send(new_sd, (void*)&lmsg, sizeof(uint16_t), 0);
                    if(ret < 0){
                        perror("Errore durante l'invio della dimensione: ");
                    }

                    ret = send(new_sd, (void*)&buffer, len, 0);
                    if(ret < 0){
                        perror("Errore durante l'invio del messaggio: ");
                    }

                    //printf("Fine message\n");
                }
                /* FINE SOTTOCOMANDO DI CHAT */
                break;
            }


        }

        else{
            close(sd);
            fgets(buffer, BUFFER_SIZE, stdin);

            // help, list, esc
            if( check_cmd(buffer) < 0 ){
                printf("Comando non valido\n");
            }
            else{
                if(global_cmd == 0){
                    help();
                }
                if(global_cmd == 1){
                    list();
                }
                if(global_cmd == 2){
                    kill(ping_child, SIGKILL);
                    kill(listener_child, SIGKILL);
                    break;
                }
            }
        }

    }
    close(sd);
    return 0;
}
