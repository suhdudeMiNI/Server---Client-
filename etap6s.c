

#define _GNU_SOURCE 
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <signal.h>
#include <netdb.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include<time.h>

#define SSIZE_MAX 2000
#define TEXT_MAX 100000
 
#define ERR(source) (perror(source),\
                     fprintf(stderr,"%s:%d\n",__FILE__,__LINE__),\
                     exit(EXIT_FAILURE))

#define BACKLOG 3
volatile sig_atomic_t do_work=1;
volatile sig_atomic_t do_usr = 1;

void sigusr1_handler(int sig) 
{
    do_usr = 0;
}

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

int make_socket(int domain, int type){
	int sock;
	sock = socket(domain,type,0);
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

int bind_tcp_socket(char* ip_addr, char* port, int n){
	struct sockaddr_in addr;
	int socketfd,t=1;
	socketfd = make_socket(PF_INET,SOCK_STREAM);
	addr = make_address(ip_addr, port);
	if (setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR,&t, sizeof(t))) ERR("setsockopt");
	if(bind(socketfd,(struct sockaddr*) &addr,sizeof(addr)) < 0)  ERR("bind");
	if(listen(socketfd, n+1) < 0) ERR("listen");
	return socketfd;
}

int add_new_client(int sfd){
	int nfd;
	if((nfd=TEMP_FAILURE_RETRY(accept(sfd,NULL,NULL)))<0) {
		if(EAGAIN==errno||EWOULDBLOCK==errno) return -1;
		ERR("accept");
	}
	return nfd;
}

void usage(char * name){
	fprintf(stderr,"USAGE: %s ip, port, NumClients, file path\n",name);
}

int HowManyLines(char* FullMessage)
{
    int id = 0;
    int NumStr = 1;
    while(FullMessage[id]!= '\0')
    {
        if(FullMessage[id] == '\n')
        {
            NumStr++;
        }
        id++;
    }
    return NumStr;
}

char* RandomLine(char* FullMessage, int LineId)
{
    char* FMCopy = (char*) malloc(sizeof(char)*SSIZE_MAX);
    strcpy(FMCopy, FullMessage);
    char* tok;
    tok = strtok(FMCopy, "\n");
    int i = 0;
    while(i!=LineId)
    {
        tok = strtok(NULL, "\n");
        i++;
    }
    
    return tok;
}

char* SendMesSizeByte(char* stroka, int byt, int Which)
{
    char* newStr = (char*) malloc(sizeof(char)*TEXT_MAX);
    int id = 0;
    for(int i = Which; i < byt+Which; i++)
    {
        newStr[id] = stroka[i];
        id++;
    }
    return newStr;
}

void doServer(int fdT, int n, char* FullMessage)
{
    int cfd; 
    int ConnectedClients = 0;
    int* allClients = (int*) malloc(sizeof(int)*n); 
    int IsClosed = 0;
    int NumStr = HowManyLines(FullMessage);
    fd_set base_rfds, rfds;
    sigset_t mask, oldmask;
    struct timespec tv = {0, 330000000};
    FD_ZERO(&base_rfds);
    FD_SET(fdT, &base_rfds);
    sigemptyset (&mask);
    sigaddset (&mask, SIGINT);
    sigprocmask (SIG_BLOCK, &mask, &oldmask);
    srand(time(NULL));
    int* IdQuestion = (int*) malloc(sizeof(int)*n);
    int* SkolkoOtprawl = (int*) malloc(sizeof(int)*n);
    int MaxDS = fdT;
    while(do_work){
        rfds=base_rfds;
        for(int i = 0; i < ConnectedClients; i++)
        {
            FD_SET(allClients[i], &rfds);
        }
        if(pselect(MaxDS+1,&rfds,NULL,NULL,&tv,&oldmask)>0)
        {
            if(FD_ISSET(fdT, &rfds))
            {

                if(do_usr == 1)
                {
                    cfd = add_new_client(fdT);
                    allClients[ConnectedClients] = cfd;                   
                    IdQuestion[ConnectedClients] = rand() % NumStr;
                    SkolkoOtprawl[ConnectedClients] = 0;
                    if (cfd > MaxDS)
                        MaxDS = cfd;

                    if(cfd >= 0)
                    {
                        if(ConnectedClients < n)
                        {
                            ConnectedClients++;
                            char tekst[6] = "Hello\n";
                            write(cfd, tekst, sizeof(char[6]));
                        }
                        else
                        {
                            char tekst[5] = "NIE\n";
                            write(cfd, tekst, sizeof(char[5]));
                            if(TEMP_FAILURE_RETRY(close(cfd))) ERR("close: ");
                        }
                    }
                }
                else
                {
                    IsClosed = 1;
                    if(TEMP_FAILURE_RETRY(close(fdT))) ERR("close: ");
                    FD_ZERO(&base_rfds);
                }
            }
            else
            {
                
                for(int i = 0; i < ConnectedClients; i++)
                {
                    if(!FD_ISSET(allClients[i], &rfds))
                    {
                        continue;
                    }
                    char TakenMes[100];
                    
                    if(read(allClients[i], TakenMes, sizeof(char))<1)
                    {
                        if(TEMP_FAILURE_RETRY(close(allClients[i]))) ERR("close: ");
                        FD_CLR(allClients[i], &base_rfds);
                        for(int j = i; j < ConnectedClients-1; j++)
                        {
                            allClients[j] = allClients[j+1];
                        }
                        ConnectedClients--;
                        i--;  
                        continue;
                    }
                    IdQuestion[i] = rand() % NumStr;
                    SkolkoOtprawl[i] = 0;
                }
            }
        }
        else
        {
            for(int i = 0; i < ConnectedClients; i++)
            {  
                if(strlen(RandomLine(FullMessage, IdQuestion[i])) + 1 == SkolkoOtprawl[i])
                {
                    continue;
                }
                int SendBytes = rand() % ((strlen(RandomLine(FullMessage, IdQuestion[i]))+ 1 - SkolkoOtprawl[i]) + 1);
                if(SendBytes > 2000)
                {
                    SendBytes = 2000;
                }
                char* SendMes;
                char* data =  RandomLine(FullMessage, IdQuestion[i]);
                SendMes = SendMesSizeByte(data, SendBytes, SkolkoOtprawl[i]);
                SkolkoOtprawl[i] += SendBytes;
                char* bf = (char*)malloc(SendBytes*sizeof(char));
                strcpy(bf, SendMes);
                if (strlen(SendMes) != SendBytes)
                    bf[SendBytes] = '\0';
                if(write(allClients[i], bf, sizeof(char[SendBytes])) <= 0 && errno == EPIPE)
                {                    
                    if(TEMP_FAILURE_RETRY(close(allClients[i]))) ERR("close: ");
                    FD_CLR(allClients[i], &base_rfds);
                    for(int j = i; j < ConnectedClients-1; j++)
                    {
                        allClients[j] = allClients[j+1];
                    }
                    ConnectedClients--;
                    i--;                    
                }
                free(bf);
            }
        }
    }
    char bufer[7] = "Koniec";
    for(int i = 0; i < ConnectedClients; i++)
    {
        write(allClients[i], bufer, sizeof(char[7]));
    }
    if(!IsClosed)
    {
        if(TEMP_FAILURE_RETRY(close(fdT))<0)ERR("close");
    }
    sigprocmask (SIG_UNBLOCK, &mask, NULL);
    free(allClients);
}

char* ReadFile(char* name)
{
    char* buf = (char*) malloc(sizeof(char)* TEXT_MAX * 10); 
    char* tekst = (char*) malloc(sizeof(char)*TEXT_MAX);
    FILE *fp;
    if((fp = fopen(name, "r")) == NULL) ERR("ReadFile: ");
    while((fgets(tekst, TEXT_MAX, fp))!= NULL)
    {
        strcat(buf, tekst);
    }    
    fclose(fp);
    free(tekst);
    return buf;
}

int main(int argc, char** argv) {
	int fdT;
	int new_flags;
    char* FullMessage;
	if(argc!=5) {
		usage(argv[0]);
		return EXIT_FAILURE;
	}
	if(sethandler(SIG_IGN,SIGPIPE)) ERR("Seting SIGPIPE:");
	if(sethandler(sigint_handler,SIGINT)) ERR("Seting SIGINT:");
    if(sethandler(sigusr1_handler,SIGUSR1)) ERR("Seting SIGUSR1:");
	fdT=bind_tcp_socket((argv[1]), (argv[2]), atoi(argv[3]));
	new_flags = fcntl(fdT, F_GETFL) | O_NONBLOCK;
	fcntl(fdT, F_SETFL, new_flags);
    FullMessage =  ReadFile(argv[4]);
	doServer(fdT, atoi(argv[3]), FullMessage);
	fprintf(stderr,"Server has terminated.\n");
	return EXIT_SUCCESS;
}
