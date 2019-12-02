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

#include <iostream>
#include <thread>

#define MAX_PKT_SIZE (640*480*4)

#define BLUE 0x001f
#define BLACK 0x0000
#define YELLOW 0xffe0
#define RED 0xf800
#define GREEN 0x07e0

#define max_clients 4

unsigned int vectorColor[480][640] = {{0}};
unsigned int vectorsColor[4][239][319] = {{{0}}};

unsigned int xyPosition_Flag[4][2];

const char letter[8] = {'a','A','w','W','s','S','d','D'};
const char* commands[4] = {"left","forward","back","rigth"};

void fillHorizLine(const int color, const int Xmin, const int Xmax, const int Y);
void fillVertLine(const int color, const int X, const int Ymin, const int Ymax);
void fillRectangle(const int color, const int X0, const int X1, const int Y0, const int Y1);
void createCube(const int id);
void refreshCubes(void);
void updateScreen(void);
void eraseCube(const int id);
void moveCube(const int id, const char command);

char buffer[max_clients]={0}, check_buff=0;
int incrementer=0;

fd_set readfds;
struct sockaddr_in address;

int init(int &master_socket);
void updateSocket(int &master_socket, int &sd, int &addrlen, int client_socket[max_clients] );
void useSocket(int &sd, int &client_socket, int i)
{
	while(1)
	{
		if(i <= incrementer-1)
		{
			sd = client_socket;
			//std::cout << sd << ' ' << i << std::endl;
			if(sd )
				if(FD_ISSET( sd, &readfds) )
					while(1)
					{
						int n = recv(sd, &buffer[i], 1, 0);
						if(n < 0 || !buffer[i])
						{
							//client_socket = 0;
							break;
						}
						std::cout << "im in: " << i << std::endl;

						moveCube(i, buffer[i]);
						refreshCubes();
						updateScreen();
						buffer[i] = 0;		
					}
		}
	}
}

int main( int argc, char **argv )
{
	int master_socket, client_socket[max_clients] = {0}, addrlen, sd;

	struct sockaddr_in address;	
	fd_set readfds;

	addrlen = init(master_socket);

	puts("Waiting for connections..\n");

	int incrementer = 0;

	std::thread t1(updateSocket, std::ref(master_socket), std::ref(sd), std::ref(addrlen), client_socket);
	std::thread t2[max_clients];
	for(int i=0; i<max_clients; ++i)
		t2[i] = std::thread(useSocket, std::ref(sd), std::ref(client_socket[i]), i);
	t1.join();
	for(int i=0; i<max_clients; ++i)
		t2[i].join();

	return 0; 
}

void refreshCubes(void)
{
	int x,y;	
	for(x = 0; x < 319; ++x)
	{
		for(y = 0; y < 239; ++y)
			vectorColor[y][x] = vectorsColor[0][y][x];
		for(y = 241; y < 480; ++y)
			vectorColor[y][x] = vectorsColor[2][y-241][x];
	}
	
	for(x = 321; x < 640; ++x)
	{
		for(y = 0; y < 239; ++y)
			vectorColor[y][x] = vectorsColor[1][y][x-321];
		for(y = 241; y < 480; ++y)
			vectorColor[y][x] = vectorsColor[3][y-241][x-321];
	}
	
}

void createCube(const int id)
{
	std::cout << "here I am\n";
	std::cout << "incrementer: " << incrementer << std::endl;

	if(id > 3)
		return;
	int x,y;
	bool key;
	for(x = 140; x < 180; x++)
		for(y = 100; y < 140; y++)
		{	
			if(!key)
			{
				xyPosition_Flag[id][0] = x;
				xyPosition_Flag[id][1] = y;
				key = true;
			}
			vectorsColor[id][y][x] = RED;
		}
}

void eraseCube(const int id)
{
	if(id > 3)
		return;
	int x,y;
	for(x = 0; x < 319; x++)
		for(y = 0; y < 239; ++y)
			vectorsColor[id][y][x] = BLACK;
}

void moveCube(const int id, const char command)
{
	if(id > 3)
		return;
	int i,incr=0;
	for(i=0;i<8;++i)
	{
		if(command == letter[i])
			break;
		++incr;	
	}
	if(incr == 8)
		return;

	bool run=false;
	if( (command == 'd' || command == 'D') && (xyPosition_Flag[id][0]+40) < 318)
	{
		run = true;
		xyPosition_Flag[id][0] += 1;
	}
	else if( (command == 'w' || command == 'W') && (xyPosition_Flag[id][1] > 0) )
	{
		run = true;
		xyPosition_Flag[id][1] -= 1;
	}
	else if( (command == 'a' || command == 'A') && (xyPosition_Flag[id][0] > 0) )
	{
		run = true;
		xyPosition_Flag[id][0] -= 1;
	}
	else if( (command == 's' || command == 'S') && (xyPosition_Flag[id][1]+40) < 238)
	{
		run = true;
		xyPosition_Flag[id][1] += 1;
	}
	else
		run = false;

	
	if(!run)
		return;
	else
	{	
		int X = xyPosition_Flag[id][0], Y = xyPosition_Flag[id][1];
		eraseCube(id);
		printf("you pressed the %c : which is %s\n",command, commands[(int)incr/2]);
		for(int x=X; x<X+40; x++)
			for(int y=Y; y<Y+40; y++)
				vectorsColor[id][y][x] = RED;							
	}

	std::cout << "x: " << xyPosition_Flag[id][0] << " | y: " << xyPosition_Flag[id][1] << std::endl;
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


void updateSocket(int &master_socket, int &sd, int &addrlen, int client_socket[max_clients] )
{

	while(1)
	{
	
	int new_socket, max_sd, activity, valread;
	FD_ZERO(&readfds);
	FD_SET(master_socket, &readfds);
	max_sd = master_socket;
	
	for(int i = 0; i<max_clients; ++i)
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
			perror("accept error\n");
			exit(1);		
		}
		else
		{
			if(incrementer < max_clients)
				printf("New connection. Socket fd is %d, ip is: %s, port: %d\n", new_socket, inet_ntoa(address.sin_addr), ntohs(address.sin_port) );	
			else
				printf("cannot connect new client the stack is full\n\n");
		}
		for(int i = 0; i< max_clients; ++i)
		{
			if(!client_socket[i])
			{
				++incrementer;
				client_socket[i] = new_socket;
				printf("Adding to list of sockets as %d\n\n", i);
				createCube(i);
				refreshCubes();
				updateScreen();
				break;
			}
		}
	}
	
	for(int i=0; i<max_clients; ++i)
	{
		sd = client_socket[i];
		if(FD_ISSET( sd, &readfds))
		{
			if(! (valread = read(sd, &check_buff, 0)) )
			{
				getpeername(sd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
				printf("Host disconnected, ip %s, port %d \n\n", inet_ntoa(address.sin_addr), ntohs(address.sin_port));
				close(sd);
				incrementer--;
				client_socket[i] = 0;
				eraseCube(i);
				refreshCubes();
				updateScreen();
			}
		}
	}

	}
}

int init(int &master_socket)
{
	updateScreen();
	fillHorizLine(BLUE, 0, 640, 240);
	fillVertLine(BLUE, 320,0,480);
	
	int opt = true;

	
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
	return sizeof(address);
}
