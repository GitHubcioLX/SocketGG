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
 
struct Message {
    char *senderNumber;
    char *text;
};

typedef struct Message Message;

struct User {
    char *number;
    Message *awaitingMessages;
    int messageCount;
    int loggedIn;
    int cfd;
};

typedef struct User User;
 
struct cln {
    int cfd;
    struct sockaddr_in caddr;
};

User *users;
int userCount = 0;
 
Message * getMessages(char* number) {
    for(int i=0; i < userCount; i++) {
        if(strcmp(users[i].number, number) == 0) {
            return users[i].awaitingMessages;
        }
    }
    //miejsce na kod obslugujacy rejestracje klienta
    return NULL;
}
 
void *serve_single_client(void *arg) {
    /*struct Student *students = malloc(sizeof(struct Student)*numberOfStudents);
    students[0].name = "Kamil\n";
    students[0].indeks = "136780";
    students[1].name = "Daniel\n";
    students[1].indeks = "136775";
    students[2].name = "Maciej\n";
    students[2].indeks = "136698";*/
    const int buf_size=64;
    char buf[buf_size];
    
    //"8181901;Siema eniu"
 
    struct cln* c = (struct cln*)arg;
    printf("Connection from %s:%d\n", inet_ntoa(c->caddr.sin_addr), c->caddr.sin_port);
    read(c->cfd, buf, buf_size);
    //int id = getStudent(buf, students);
    //if(id == -1) {
    //    char msg[] = "Wrong index\n";
    //    write(c->cfd, msg, strlen(msg)+1);
    //} else {
    write(c->cfd, &buf, buf_size);
    //}
    write(c->cfd, "\nHello\n", 9);
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
        addr.sin_port = htons(1235);
        addr.sin_addr.s_addr = INADDR_ANY;
    bind(fd, (struct sockaddr*) &addr, sizeof(addr));
    listen(fd, 10);
 
    while(1) {
        struct cln* c = malloc(sizeof(struct cln));
        socklen_t l = sizeof(c->caddr);
        c->cfd = accept(fd, (struct sockaddr*) &c->caddr, &l);
 
        pthread_create(&tid, NULL, serve_single_client, c);
        pthread_detach(tid);
    }
    close(fd);
}
