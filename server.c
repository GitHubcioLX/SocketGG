#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include <string.h>

#define buf_size 128
#define max_nr_len 16

// Struktura opisujaca pojedyncza wiadomosc
struct Message {
    char *senderNumber;
    char *text;
};

typedef struct Message Message;

// Struktura opisujaca pojedynczego uzytkownika
struct User {
    char *number;
    Message *messages;
    int messageCount;
    int loggedIn;
    int cfd;
    pthread_mutex_t mutex;
};

typedef struct User User;

// Struktura opisujaca deskryptor
struct cln {
    int cfd;
    struct sockaddr_in caddr;
};

// Inicjalizacja zmiennych globalnych
User *users;
int userCount = 0;
// Utworzenie semafora kontrolujacego wspolbiezny dostep do tablicy users
pthread_mutex_t users_mutex = PTHREAD_MUTEX_INITIALIZER;

// Przeslanie wiadomosci do uzytkownika, gdy ten jest zalogowany
void passMessages(int id) {
    while(users[id].messageCount > 0 && users[id].loggedIn == 1)
    {
        pthread_mutex_lock(&users[id].mutex);

        char buf[buf_size] = "";
        // Konkatenacja numeru adresata, separatora oraz tresci wiadomosci
        strcat(buf, users[id].messages[0].senderNumber);
        strcat(buf, ";");
        strcat(buf, users[id].messages[0].text);

        write(users[id].cfd, &buf, buf_size);
        printf("Przekazano wiadomosc: %s\n", buf);

        users[id].messageCount--;
        // Zdjecie przeslanej wiadomosci ze stosu oczekujacych wiadomosci
        if(users[id].messageCount > 0)
            memmove(&users[id].messages[0], &users[id].messages[1], sizeof(Message)*users[id].messageCount);
        users[id].messages = realloc(users[id].messages, sizeof(Message)*users[id].messageCount);

        pthread_mutex_unlock(&users[id].mutex);
    }
}

// Zwroc id (indeks na liscie 'users') uzytkownika o danym numerze
int findUser(char *number)
{
    for(int i=0; i<userCount; i++){
        if( strcmp(number, users[i].number ) == 0)
            return i;
    }
    return -1;
}

// Obsluga klienta
void *serve_single_client(void *arg) {
    int bytesRead;
    char buf[buf_size];
    char flag; // rodzaj odebranej od klienta wiadomosci
    char *found; // wskaznik na znaleziony srednik w stringu
    int index; // index znalezionego srednika
    int id = -2; // id obslugiwanego klienta (indeks na liscie 'users')
    char recvNumber[max_nr_len]; // numer odbiorcy nadanej przez obslugiwanego uzytkownika wiadomosci
    int recvId; // id odbiorcy wiadomosci (indeks na liscie 'users')
    struct cln* c = (struct cln*)arg;
    printf("\nNowe polaczenie od %s:%d\n", inet_ntoa(c->caddr.sin_addr), c->caddr.sin_port);

    while(1) {
        // Czyszczenie zmiennych 'buf' i 'recvNumber'
        memset(buf, 0, buf_size);
        memset(recvNumber, 0, max_nr_len);
        // Odebranie od klienta wiadomosci (w sposob nieblokujacy)
        bytesRead = recv(c->cfd, buf, buf_size, MSG_DONTWAIT);
        if(bytesRead > 0) {
            flag = buf[0];
            printf("\nOdebrano wiadomosc: %s\n", buf);
            // Skrocenie zawartosci bufora o pierwszy znak (flage)
            memmove(&buf[0], &buf[1], buf_size);
            switch(flag) {
                // Logowanie/rejestracja uzytkownika
                case '1':
                    if(id == -2) {
                        id = findUser(buf);
                        if(id == -1) { // uzytkownik nie znajduje sie na liscie users
                            // Inicjalizacja nowego uzytkownika
                            User user;
                            user.number = malloc(sizeof(buf));
                            strcpy(user.number, buf);
                            user.cfd = c->cfd;
                            user.messages = malloc(sizeof(struct Message));
                            user.messageCount = 0;
                            user.loggedIn = 1;
                            // Inicjalizacja semafora kontrolujacego dostep do danych uzytkownika
                            user.mutex = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;

                            pthread_mutex_lock(&users_mutex);
                            id = userCount;
                            userCount++;
                            users = realloc(users, sizeof(User)*(userCount));
                            users[id] = user;
                            pthread_mutex_unlock(&users_mutex);

                            printf("> Nowy uzytkownik o numerze: %s\n", buf);
                        }
                        else { // logowanie istniejacego uzytkownika
                            users[id].loggedIn = 1;
                            users[id].cfd = c->cfd;
                            printf("> Zalogowano uzytkownika: %s\n", buf);
                        }
                    }
                    else {
                        printf("> Blad.\n");
                    }
                    break;
                // Nowa wiadomosc od obslugiwanego klienta
                case '2':
                    if(id > -1) {
                        found = strchr(buf, ';'); // wskaznik na pierwszy srednik w buforze
                        if(found != NULL) {
                            index = (int)(found-buf);
                            strncpy(recvNumber, buf, index);
                            // Skrocenie zawartosci bufora o numer odbiorcy oraz separator ';'
                            memmove(&buf[0], &buf[index+1], buf_size-index+1);
                            printf("> Nowa wiadomosc od %s do %s: %s.\n", users[id].number, recvNumber, buf);
                        }
                        else {
                            printf("> Blad.\n");
                        }

                        recvId = findUser(recvNumber);
                        if(recvId != -1) {
                            // Dopisanie odebranej wiadomosci do listy messages adresata
                            pthread_mutex_lock(&users[recvId].mutex);

                            users[recvId].messageCount++;
                            users[recvId].messages = realloc(users[recvId].messages, sizeof(Message)*users[recvId].messageCount);

                            Message message;
                            message.senderNumber = malloc(sizeof(users[id].number));
                            strcpy(message.senderNumber, users[id].number);
                            message.text = malloc(sizeof(buf));
                            strcpy(message.text, buf);
                            users[recvId].messages[users[recvId].messageCount-1] = message;

                            pthread_mutex_unlock(&users[recvId].mutex);
                        }
                        else {
                            printf("Nie ma takiego numeru.\n");
                            // Przeslanie uzytkownikowi komunikatu o bledzie (nie znaleziono odbiorcy)
                            memset(buf, 0, buf_size);
                            strcpy(buf, "$;Odbiorca nie istnieje.");
                            write(users[id].cfd, &buf, buf_size);
                        }
                    }
                    else {
                        printf("> Tylko zalogowani moga wysylac wiadomosci.\n");
                    }
                    break;
                // Wylogowanie klienta
                case '0':
                    if(id > -1) {
                        users[id].loggedIn = 0;
                        printf("> Uzytkownik o numerze %s wylogowal sie.\n", users[id].number);
                        close(c->cfd);
                        free(c);
                        return 0;
                    }
                    else {
                        printf("> Niezalogowany uzytkownik wyszedl.\n");
                        close(c->cfd);
                        free(c);
                        return 0;
                    }
                    break;
                default:
                    printf("\n");
                    break;
            }
        }
        if(id > -1 && users[id].loggedIn == 1) passMessages(id);
    }
    close(c->cfd);
    free(c);
    return 0;
}

int main() {
    // Inicjalizacja obiektow koniecznych do realizacji polaczenia z klientem
    struct sockaddr_in addr;
    pthread_t tid;
    int fd = socket(PF_INET, SOCK_STREAM, 0);
    int on=1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char*)&on, sizeof(on));
        addr.sin_family = PF_INET;
        addr.sin_port = htons(1237);
        addr.sin_addr.s_addr = INADDR_ANY;
    // Przypisanie adresu serwera do gniazda
    bind(fd, (struct sockaddr*) &addr, sizeof(addr));
    // Maksymalnie 10 oczekujacych polaczen
    listen(fd, 10);
    
    users = malloc(sizeof(User));
    while(1) {
        struct cln* c = malloc(sizeof(struct cln));
        socklen_t len = sizeof(c->caddr);
        c->cfd = accept(fd, (struct sockaddr*) &c->caddr, &len); // zaakceptowanie polaczenia od klienta
        pthread_create(&tid, NULL, serve_single_client, c); // utworzenie watku obslugujacego klienta
        pthread_detach(tid); // zasoby wykorzystywane przez watek zostana automatycznie zwolnione po jego zakonczeniu
    }
    close(fd);
    free(users);
}