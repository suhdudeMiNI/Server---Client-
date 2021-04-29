
#define _GNU_SOURCE 
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <signal.h>
#include <netdb.h>
#include<stdio_ext.h>

#define SSIZE_MAX 2000
#define MAX_ANS 100

#define ERR(source) (perror(source),\
                fprintf(stderr,"%s:%d\n",__FILE__,__LINE__),\
                exit(EXIT_FAILURE))

volatile sig_atomic_t do_work=1;

void sigint_handler(int sig) 
{
    do_work = 0;
}

int sethandler( void (*f)(int), int sigNo) {
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = f;
    if (-1==sigaction(sigNo, &act, NULL))
        return -1;
    return 0;
}

int make_socket(void){
    int sock;
    sock = socket(PF_INET,SOCK_STREAM,0);
    if(sock < 0) ERR("socket");
    return sock;
}

struct sockaddr_in make_address(char *address, char *port){
    int ret;
    struct sockaddr_in addr;
    struct addrinfo *result;
    struct addrinfo hints = {};
    hints.ai_family = AF_INET;
    if((ret=getaddrinfo(address,port, &hints, &result))){
            fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret));
            exit(EXIT_FAILURE);
    }
    addr = *(struct sockaddr_in *)(result->ai_addr);
    freeaddrinfo(result);
    return addr;
}

int connect_socket(char *name, char *port){
    struct sockaddr_in addr;
    int socketfd;
    socketfd = make_socket();
    addr=make_address(name,port);
    if(connect(socketfd,(struct sockaddr*) &addr,sizeof(struct sockaddr_in)) < 0){
        if(errno!=EINTR) ERR("connect");
        else {
                fd_set wfds ;
                int status;
                socklen_t size = sizeof(int);
                FD_ZERO(&wfds);
                FD_SET(socketfd, &wfds);
                if(TEMP_FAILURE_RETRY(select(socketfd+1,NULL,&wfds,NULL,NULL))<0) ERR("select");
                if(getsockopt(socketfd,SOL_SOCKET,SO_ERROR,&status,&size)<0) ERR("getsockopt");
                if(0!=status) ERR("connect");
        }
    }
    return socketfd;
}

char ReadFromConsole()
{
    char* ans = (char*)malloc(sizeof(char)*MAX_ANS);
    read(STDIN_FILENO, ans, sizeof(char)*MAX_ANS);
    char mess = ans[0];
    free(ans);
    return mess;
}

void WriteMess(int fdT, char ans)
{  
    write(fdT, &ans, sizeof(char));
}

void ReadFromFD(int* fdT, int n)
{
    int ReadySend = 0;
    int* kol2 = (int*) malloc(sizeof(int)*n);
    char** buf = (char**) malloc(sizeof(char*)*n);
    for(int i = 0; i<n; i++)
    {
        buf[i] = (char*) malloc(sizeof(char)*SSIZE_MAX);
    }
    memset(kol2, 0, n*sizeof(int));
    fd_set base_rfds, rfds;
    sigset_t mask, oldmask;
    FD_ZERO(&base_rfds);
    FD_SET(STDIN_FILENO, &base_rfds);
    sigemptyset (&mask);
    sigaddset (&mask, SIGINT);
    sigprocmask (SIG_BLOCK, &mask, &oldmask);
    int MaxDS = STDIN_FILENO;
    char answer;
    int WhichServer = -1;
    while(do_work)
    {
        rfds=base_rfds;
        
        for(int i = 0; i < n; i++)
        {
            FD_SET(fdT[i], &rfds);
            if(fdT[i] > MaxDS)
            {
                MaxDS = fdT[i];
            }
        }
        if(pselect(MaxDS+1,&rfds,NULL,NULL,NULL,&oldmask)>0)
        {
            if(FD_ISSET(STDIN_FILENO, &rfds))
            {         
                answer = ReadFromConsole();
                printf("User: %c\n", answer);
                if(WhichServer>-1)
                {
                   
                    ReadySend = 1;
                }
                else{
                    printf("nie teraz!\n");
                }

            }
            for (int i = 0; i < n; i++)
            {
                if(i == WhichServer && ReadySend)
                {
                    WhichServer = -1;
                    WriteMess(fdT[i], answer); 
                    ReadySend = 0;
                    continue;
                }

                if(FD_ISSET(fdT[i], &rfds))
                {
                    int kol = read(fdT[i], buf[i] + kol2[i], SSIZE_MAX);
                    if(kol < 1) 
                    {
                        for(int j = i; j < n-1; j++)
                        {
                            fdT[j] = fdT[j+1];
                            buf[j] = buf[j+1];
                            kol2[j] = kol2[j+1];
                        }
                        if(WhichServer == i)
                            WhichServer--;
                        n--;
                        i--; 

                        continue;
                    }
                    kol2[i]+= kol;
                    if(buf[i][kol2[i]-1] == '\0')
                    {
                        printf("Server %i: %s\n", i, buf[i]);            
                        kol2[i] = 0;

                        if (WhichServer != -1)
                        {
                            WriteMess(fdT[WhichServer], '0');
                        }
                        
                        WhichServer = i;                        
                    } 
                }
            }
        }
    }
}

void usage(char * name){
    fprintf(stderr,"USAGE: %s ip port \n",name);
}

int main(int argc, char** argv) {    
    int n = (argc - 1)/2;
    int* fd = (int*) malloc(sizeof(int)*n);    
    if(argc % 2 == 0) {
            usage(argv[0]);
            return EXIT_FAILURE;
    }
    
    if(sethandler(SIG_IGN,SIGPIPE)) ERR("Seting SIGPIPE:");  
    for (int i = 0; i < n; i++)
        fd[i] = connect_socket(argv[2*i+1], argv[2*i+2]);
     
    ReadFromFD(fd, n);    
    return EXIT_SUCCESS;
}