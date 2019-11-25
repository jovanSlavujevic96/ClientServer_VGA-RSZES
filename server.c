#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <dirent.h>
#include <arpa/inet.h>

#include <errno.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>

#define MAX_PKT_SIZE (640*480*4)

#define BLUE 0x001f
#define BLACK 0x0000
#define YELLOW 0xffe0
#define RED 0xf800
#define GREEN 0x07e0

#define max_clients 4

unsigned int vectorColor[480][640] = {{0}};

void doprocessing (int socket);
void fillHorizLine(const int color, const int Xmin, const int Xmax, const int Y);
void fillVertLine(const int color, const int X, const int Ymin, const int Ymax);
void fillRectangle(const int color, const int X0, const int X1, const int Y0, const int Y1);

int main( int argc, char **argv )
{
	//fillRectangle(RED, 10,50,10,50);
	fillHorizLine(BLUE, 0, 640, 240);
	fillVertLine(BLUE, 320,0,480);

	int opt = true;
	int master_socket, new_socket, client_socket[max_clients] = {0}, addrlen, valread, activity;
	int sd, max_sd;
	char buffer[1025], *message = "ECHO Daemon v1.0 \r\n";

	struct sockaddr_in address;	
	fd_set readfds;

	master_socket = socket(AF_INET, SOCK_STREAM, 0);
	if(master_socket < 0)
    {
		perror("ERROR opening socket\n");
		return -1;
    }

	if( setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt) )  < 0 )
	{
		perror("ERROR setsockopt\n");	  
		return -1;
	}
  
	bzero((char *) &address, sizeof(address)); 
	address.sin_family = AF_INET; 
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(5001); 

    if( bind(master_socket, (struct sockaddr *) &address, sizeof(address) ) < 0)
    {
        perror("ERROR on binding\n");
        return -1;
    }

	if( listen(master_socket,4) < 0)
	{
		perror("ERROR on listen\n");
		return -1;	 
	}
    
	addrlen = sizeof(address);
	puts("Waiting for connections..\n");

	while (true)
    {
		FD_ZERO(&readfds);
		FD_SET(master_socket, &readfds);
		max_sd = master_socket;
		
		int i;
		for(i = 0; i<max_clients; ++i)
		{
			sd = client_socket[i];
			if(sd>0)
					FD_SET( sd, &readfds);
			if(sd > max_sd)
				max_sd = sd;
		}
		activity = select( max_sd +1, &readfds, NULL, NULL, NULL);
		if( (activity <0) && (errno!=EINTR))
			printf("select error\n");
			
		if(FD_ISSET(master_socket, &readfds) )
		{
			if((new_socket = accept(master_socket, (struct sockaddr *)&address, (socklen_t*)&addrlen))<0)
			{
				perror("accept\n");
				exit(1);		
			}
			printf("New connection. Socket fd is %d, ip is: %s, port: %d\n", new_socket, inet_ntoa(address.sin_addr), ntohs(address.sin_port) );

			if( send(new_socket, message, strlen(message), 0) != strlen(message) )
				perror("send\n");
			
			int incrementer=0;	
			for(i = 0; i< max_clients; ++i)
			{
				if(!client_socket[i])
				{
					client_socket[i] = new_socket;
					printf("Adding to list of sockets as %d\n\n", i+1);
					break;
				}
				incrementer++;
			}
			if(incrementer == max_clients)
				puts("cannot connect new client, the stack is full\n\n");
			else
				doprocessing(new_socket);			
		}
		   
		for(i=0; i<max_clients; ++i)
		{
			sd = client_socket[i];
			
			if(FD_ISSET( sd, &readfds))
			{
				if(! (valread = read(sd, buffer, 1024)) )
				{
					getpeername(sd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
					printf("Host disconnected, ip %s, port %d \n\n", inet_ntoa(address.sin_addr), ntohs(address.sin_port));
					close(sd);
					client_socket[i] = 0;
				}
				else
					buffer[valread] = '\0';
			}
		}
    }
	return 0; 
}

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

void doprocessing(int socket)
{
    int n;
	char buffer;
    while (true)
    {
		printf("[IDLE] Waiting for command..\n");
		read(socket, &buffer, 255);
		printf("Received command: %c : %d\n",buffer, buffer);
		if(buffer == 'W' || buffer == 'w')
			printf("move forward\n");
		else if(buffer == 'A' || buffer == 'a')
			printf("move left\n");
		else if(buffer == 'S' || buffer == 's')
			printf("move back\n");
		else if(buffer == 'D' || buffer == 'd')
			printf("move right\n");
		else if(buffer == 'Q' || buffer == 'q' || !buffer)
			break;
		buffer = 0;	
    }
}
