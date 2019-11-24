#include <stdio.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <dirent.h>

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>

#define MAX_PKT_SIZE (640*480*4)

#define BLUE 0x001f
#define BLACK 0x0000
#define YELLOW 0xffe0
#define RED 0xf800
#define GREEN 0x07e0

unsigned int vectorColor[480][640] = {{0}};

void updateScreen(void)
{
	FILE *fp;
	int fd, *p;
	fd = open("/dev/vga_dma", O_RDWR|O_NDELAY);
	if( fd < 0)
	{
		printf("Cannot opet /dev/vga for write\n");
		return;
	}
	p = (int*)mmap(0,640*480*4, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	memcpy(p, vectorColor, MAX_PKT_SIZE);
	munmap(p, MAX_PKT_SIZE);
	close(fd);
	if(fd < 0)
		printf("Cannot close /dev/vga for write\n");
}

void fillVector(const int color, const int Xmin, const int Xmax, const int Ymin, const int Ymax)
{
	int x,y;
	for(x=Xmin; x<Xmax; ++x)
		for(y=Ymin; y<Ymax; ++y)
			vectorColor[y][x] = color;
}

void fillHorizLine(const int color, const int Xmin, const int Xmax, const int Y)
{
	fillVector(color, Xmin, Xmax, Y, Y+1);
	updateScreen();	
}

void fillVertLine(const int color, const int X, const int Ymin, const int Ymax)
{
	fillVector(color, X, X+1, Ymin, Ymax);
	updateScreen();
}

void fillRectangle(const int color, const int X0, const int X1, const int Y0, const int Y1)
{
	int i;
	for(i=Y0; i < Y1 ; ++i)
		fillHorizLine(color, X0, X1, i);
}

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

	 fillRectangle(RED, 10,50,10,50);
	 fillHorizLine(BLUE, 0, 640, 240);
	 fillVertLine(BLUE, 320,0,480);

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
