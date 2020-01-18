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

#define buf_size 64
#define max_nr_len 16
 
struct Message {
    char *senderNumber;
    char *text;
};

typedef struct Message Message;

struct User {
    char *number;
    Message *messages;
    int messageCount;
    int loggedIn;
    int cfd;
    pthread_mutex_t mutex;
};

typedef struct User User;
 
struct cln {
    int cfd;
    struct sockaddr_in caddr;
};

User *users;
int userCount = 0;

pthread_mutex_t users_mutex = PTHREAD_MUTEX_INITIALIZER;
 
void passMessages(int id) {
    for(int i=users[id].messageCount-1; i>=0; i--)
    {        
        pthread_mutex_lock(&users[id].mutex);
        
        char buf[buf_size] = "";
        strcat(buf, users[id].messages[i].senderNumber);
        strcat(buf, ";");
        strcat(buf, users[id].messages[i].text);
        
        write(users[id].cfd, &buf, buf_size);
        
        users[id].messageCount--;
        users[id].messages = realloc(users[id].messages, sizeof(Message)*users[id].messageCount);
        
        pthread_mutex_unlock(&users[id].mutex);       
    }
}

int findUser(char *number)
{
    for(int i=0; i<userCount; i++){
        if( strcmp(number, users[i].number ) == 0)
            return i;
    }
    return -1;
}
 
void *serve_single_client(void *arg) {
    int bytesRead;
    char buf[buf_size];
    char flag;
    char *found; //wskaznik na znaleziony srednik w stringu
    int index; //index znalezionego srednika
    int id = -2; //id obslugiwanego klienta (na liscie 'users')
    char recvNumber[max_nr_len]; //numer odbiorcy nadanej przez obslugiwanego uzytkownika wiadomosci
    int recvId; //id odbiorcy wiadomosci (na liscie 'users')
    struct cln* c = (struct cln*)arg;
    printf("Connection from %s:%d\n", inet_ntoa(c->caddr.sin_addr), c->caddr.sin_port);

    while(1) {
        memset(buf, 0, buf_size);
        memset(recvNumber, 0, max_nr_len);
        bytesRead = recv(c->cfd, buf, buf_size, MSG_DONTWAIT);
        if(bytesRead > 0) {
            flag = buf[0];
            memmove(&buf[0], &buf[1], buf_size);
            printf("\n%s\n", buf);
            switch(flag) {
                case '1':
                    if(id == -2) {
                        id = findUser(buf);
                        if(id == -1) {
                            printf("Tworze nowego uzytkownika");
                            
                            //inicjalizacja nowego uzytkownika
                            User user;
                            user.number = malloc(sizeof(buf));
                            strcpy(user.number, buf);
                            user.cfd = c->cfd;
                            user.messages = malloc(sizeof(struct Message));
                            user.messageCount = 0;
                            user.loggedIn = 1;
                            user.mutex = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
                            
                            pthread_mutex_lock(&users_mutex);
                            id = userCount;
                            userCount++;
                            users = realloc(users, sizeof(User)*(userCount));
                            users[id] = user;
                            pthread_mutex_unlock(&users_mutex);
                            
                            printf("\nNowy uzytkownik o numerze: %s", buf);
                        }
                        else {
                            users[id].loggedIn = 1;
                            printf("\nZalogowano uzytkownika: %s", buf);
                        }
                    }
                    else {
                        printf("\nBlad.");
                    }
                    break;
                case '2':
                    if(id > -1) {
                        found = strchr(buf, ';'); //wskaznik na pierwszy srednik w buforze
                        if(found != NULL) {
                            index = (int)(found-buf);
                            strncpy(recvNumber, buf, index);
                            memmove(&buf[0], &buf[index+1], buf_size-index+1);
                            printf("\nNowa wiadomosc od %s do %s: %s.", users[id].number, recvNumber, buf);
                        }
                        else {
                            printf("\nBlad.");
                        }
                        
                        recvId = findUser(recvNumber);
                        if(recvId != -1) {
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
                            printf("\nNie ma takiego numeru");
                        }
                    }
                    else {
                        printf("\nTylko zalogowani moga wysylac wiadomosci");
                    }
                    break;
                case '0':
                    if(id > -1) {
                        users[id].loggedIn = 0;
                        printf("\nUzytkownik o numerze %s wylogowal sie.", users[id].number);
                    }
                    else {
                        printf("\nBlad");
                    }
                    break;
                default:
                    printf("\n");
                    break;
            }
            printf("\n");
            printf("\nLiczba uzytkownikow: %d\n", userCount);
        }
        sleep(1);
        printf("\nNasluchuje...");
        if(id > -1 && users[id].loggedIn == 1) passMessages(id);
    }
    close(c->cfd);
    free(c);
    return 0;
}
 
int main() {
    struct sockaddr_in addr;
    pthread_t tid;
    int fd = socket(PF_INET, SOCK_STREAM, 0);
    int on=1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char*)&on, sizeof(on)); //free port number when turned off
        addr.sin_family = PF_INET;
        addr.sin_port = htons(1237);
        addr.sin_addr.s_addr = INADDR_ANY;
    bind(fd, (struct sockaddr*) &addr, sizeof(addr));
    listen(fd, 10);
 
    users = malloc(sizeof(User));
    while(1) {
        struct cln* c = malloc(sizeof(struct cln));
        socklen_t len = sizeof(c->caddr);
        c->cfd = accept(fd, (struct sockaddr*) &c->caddr, &len);
        pthread_create(&tid, NULL, serve_single_client, c);
        pthread_detach(tid);
    }
    close(fd);
    free(users);
}
