#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

typedef enum { IDLE, STRING, LS, FILE_SIZE, SENDING } ftp_state;


ftp_state stat = IDLE; 
int selection = 0;
int filesize;
int sent = 0;
FILE * file;

void doprocessing (int sock)
{
    int n;
	 char buffer[256];
    bzero(buffer,256);
    while (true)
    {
            printf("[IDLE] Waiting for command..\n");
            n = read(sock,buffer,255);
            buffer[n] = 0;
            printf("Received command: %s\n",buffer);
            if(buffer[0] == 'W' || buffer[0] == 'w')
					printf("move forward\n");
				else if(buffer[0] == 'A' || buffer[0] == 'a')
					printf("move left\n");
				else if(buffer[0] == 'S' || buffer[0] == 's')
					printf("move back\n");
				else if(buffer[0] == 'D' || buffer[0] == 'd')
					printf("move right\n");
            else if(buffer[0] == 'Q' || buffer[0] == 'q')
					break;	
    }
}



int main( int argc, char **argv )
{
    int sockfd, newsockfd, portno, clilen;
    char buffer[256];
    struct sockaddr_in serv_addr, cli_addr;
    int n;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
	 if (sockfd < 0)
    {
        perror("ERROR opening socket");
        return 1;
    }
    bzero((char *) &serv_addr, sizeof(serv_addr));
    portno = 5001;
    serv_addr.sin_family = AF_INET; //mora biti AF_INET
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno); 
    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr) ) < 0)
    {
        perror("ERROR on binding");
        exit(1);
    }
    printf("FTP server started.. waiting for clients ...\n");
    listen(sockfd,4); //maksimalno 5 klijenata moze da koristi moje usluge
    clilen = sizeof(cli_addr);
	 int incr = 0; 
	 while (1)
    {
        	if( incr < 4 && (newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen) )) 
			{
				printf("FTP client connected...\n");
				++incr;
			}
			else if(incr == 4)
			{
				printf("too many clients\n");
			}
			   
        	if (newsockfd < 0)
        	{
         	perror("ERROR on accept");
            exit(1);
        	}
        	int pid = fork();
        	if (pid < 0)
        	{
         	perror("ERROR on fork");
            exit(1);
       	}
        	else if (pid == 0)
        	{
            close(sockfd);
            doprocessing(newsockfd);
        	}
        	else
            close(newsockfd);
    } 
}
