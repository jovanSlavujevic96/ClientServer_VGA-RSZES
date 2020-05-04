#include <unistd.h> 
#include <stdio.h> 
#include <sys/socket.h> 
#include <stdlib.h> 
#include <netinet/in.h> 
#include <string.h> 
#define PORT 8080

#include <iostream>
#include <string>
#include <sstream>
#include <map>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable> 
#include <memory>

#define MAX_PKT_SIZE (640*480*4)
//colors
#define BLUE 0x001f
#define BLACK 0x0000
#define YELLOW 0xffe0
#define RED 0xf800
#define GREEN 0x07e0

#include <sys/mman.h>
#include <fcntl.h>

typedef struct Point
{
    unsigned int x,y;
}Point_s;

static volatile unsigned long int PixelMatrix[480][640] = {{0}};
static volatile unsigned long int PixelMatrixFragment[4][480][640] = {{0}};
static volatile Point_s PointFlag[4];

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
	memcpy(p, (unsigned long int*)PixelMatrix, MAX_PKT_SIZE);
	munmap(p, MAX_PKT_SIZE);
	close(fd);
	if(fd < 0)
		printf("Cannot close /dev/vga for write\n");
}

void RefreshCubes(void)
{
	for(unsigned x = 0; x < 319; ++x)
	{
		for(unsigned y = 0; y < 239; ++y)
			PixelMatrix[y][x] = PixelMatrixFragment[0][y][x];
		for(unsigned y = 241; y < 480; ++y)
			PixelMatrix[y][x] = PixelMatrixFragment[2][y-241][x];
	}
	for(unsigned x = 321; x < 640; ++x)
	{
		for(unsigned y = 0; y < 239; ++y)
			PixelMatrix[y][x] = PixelMatrixFragment[1][y][x-321];
		for(unsigned y = 241; y < 480; ++y)
			PixelMatrix[y][x] = PixelMatrixFragment[3][y-241][x-321];
	}
    	updateScreen();
}

void CreateCube(const int ID)
{
    if(ID < 1 || ID > 4)
        return;

    unsigned int x,y;
    bool key = false;
    for(x=140; x<180; x++)
        for(y=100; y<140; ++y)
        {
            if(!key)
            {
                PointFlag[ID-1].x = x;
                PointFlag[ID-1].y = y;
                key = true;
            }
            PixelMatrixFragment[ID-1][y][x] = RED;
        }
    RefreshCubes();
}

void EraseCube(const int ID)
{
    if(ID < 1 || ID > 4)
        return;

    unsigned int x,y;
	for(x = 0; x < 319; x++)
		for(y = 0; y < 239; ++y)
			PixelMatrixFragment[ID-1][y][x] = BLACK;
}

void MoveCube(const int ID, char command)
{
    if(ID < 1 || ID > 4)
        return;

    if(command > 96) //small letters turn into big (a -> A) //ASCII vals
        command -= 32;
    
    unsigned int *X = (unsigned int*)&PointFlag[ID-1].x, *Y = (unsigned int*)&PointFlag[ID-1].y;
	if(command == 'D' && *X+40 < 319)
	{
        	EraseCube(ID);
		*X += 1;
	}
	else if(command == 'W' && *Y > 1) 
	{
        	EraseCube(ID);
		*Y -= 1;
	}
	else if(command == 'A' && *X > 1) 
	{
        	EraseCube(ID);
		*X -= 1;
	}
	else if(command == 'S' && *Y+40 < 239)
	{
        	EraseCube(ID);
		*Y += 1;
	}
	else
		return;

    for(unsigned int x=*X; x<*X+40; x++)
        for(unsigned int y=*Y; y<*Y+40; y++)
            PixelMatrixFragment[ID-1][y][x] = RED;
    RefreshCubes();
}

void DrawLine(const Point_s& start, const Point_s& end, const unsigned long int color)
{
    int dx, dy, p, incr=1;
    unsigned int x=start.x, y=start.y, x_lim=end.x;
    dx = (int)end.x-(int)start.x;
    if(dx<0)
        dx *= -1;
    dy = (int)end.y-(int)start.y;
    if(dy<0)
        dy *= -1;
    if(start.x > end.x)
    {
        x=end.x;
        y=end.y;
        x_lim=start.x;
    }
    if(!(y==start.y && start.y<=end.y) && !(y==end.y && end.y<=start.y))
		incr = -1;
    p=2*dy-dx;

    while(x<=x_lim && y > 0)
    {
        PixelMatrix[y][x] = color;
	if(p>=0)
        {
			y += incr; 
            p = p + 2*dy - 2*dx;
        }
        else
			p=p + 2*dy;
	if(start.x != end.x)
		x++;
    }
}

void DrawRectangle(const Point_s& start, const Point_s& end, const unsigned long int color)
{
    unsigned int xmin = (start.x < end.x) ? start.x : end.x, 
        xmax = (start.x > end.x) ? start.x : end.x;
    
    for(unsigned int i=xmin;i<=xmax;++i)
    {
        Point_s p1 = {.x=i, .y=start.y}, p2 = {.x=i, .y=end.y};
        DrawLine(p1,p2,color);
    }
}

class Server
{
private:
    struct sockaddr_in address;
    int server_fd;
    int addrlen;

    std::map<int,bool> ID_busy = {  {1,false}, {2,false}, {3,false}, {4,false}  };
    std::map<int,int> ID_socket = {  {1,-1}, {2,-1}, {3,-1}, {4,-1}  };
    std::vector<std::mutex> mutex_ID;
    std::vector<std::condition_variable> cv_ID;

    bool ServerInit(void);
    int ReserveID(void);
    bool ReserveSocket(const int ID, const int socket);
    void FreeID(const int ID);
    int FindIDfromSocket(const int socket);

public:

    explicit Server();
    virtual ~Server() = default;

    void ConnectNewClient(void);
    void CommunicationWithClient(const int ID);
};

bool Server::ServerInit(void)
{
    int opt = 1; 

    // Creating socket file descriptor 
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) 
    { 
        perror("socket failed"); 
        return false; 
    }

    // Forcefully attaching socket to the port 8080 
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, 
                                                  &opt, sizeof(opt))) 
    { 
        perror("setsockopt"); 
        return false; 
    }
    address.sin_family = AF_INET; 
    address.sin_addr.s_addr = INADDR_ANY; 
    address.sin_port = htons( PORT );

    // Forcefully attaching socket to the port 8080 
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address))<0) 
    { 
        perror("bind failed"); 
        return false; 
    } 
    if (listen(server_fd, 3) < 0) 
    { 
        perror("listen"); 
        return false;
    }
    return true;     
}

int Server::ReserveID(void)
{
    for(int id=1; id<=4; ++id)
    {
        if(ID_busy[id] == false)
        {
            ID_busy[id] = true;
            return id;
        }
    }
    return -1;
}

bool Server::ReserveSocket(const int ID, const int socket)
{
    if(ID < 1 || ID > 4)
        return false;
    ID_socket[ID] = socket;
    return true;
}

void Server::FreeID(const int ID)
{
    if(ID >= 1 && ID <= 4)
    {
        ID_busy[ID] = false;
        ID_socket[ID] = -1;
    }
}

int Server::FindIDfromSocket(const int socket)
{
    for(int id=1; id<=4; ++id)
    {
        if(ID_socket[id] == socket) 
            return id;        
    }
    return -1;
}

Server::Server() :
	mutex_ID {std::vector<std::mutex>(4)},
	cv_ID {std::vector<std::condition_variable>(4) }
{
    addrlen = sizeof(address);
    auto ret = Server::ServerInit();
    if(!ret)
        std::exit(-1);
}

void Server::ConnectNewClient(void)
{
    while(true)
    {
        int new_socket;
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen))<0 ) 
        { 
            return;
        }
        int ID = ReserveID();
        std::stringstream stream;
        stream << "Hello from server, your ID:" << ID << " Good Luck! ;)";
        int ret = send(new_socket, stream.str().c_str(), stream.str().length(), 0); 
        if(!ret) // or ret < 0
            FreeID(ID);
        else
        {
            std::lock_guard<std::mutex> lk(mutex_ID[ID-1] );
            ReserveSocket(ID, new_socket);
            CreateCube(ID);
            cv_ID[ID-1].notify_one();
        }
    }
}

void Server::CommunicationWithClient(const int ID)
{
    char buffer = 0;
    while(true)
    {
        std::unique_lock<std::mutex> lk1(mutex_ID[ID-1]);
        cv_ID[ID-1].wait(lk1, [this,ID]{return (ID_socket[ID] != -1);});

        int ret = read( ID_socket[ID] , &buffer, 1);
        if(!buffer || !ret)
        {
            lk1.unlock();
            std::lock_guard<std::mutex> lk2(mutex_ID[ID-1] );
            std::cout << "Client with ID:" << ID << " disconnected!\n";
            Server::FreeID(ID);
            EraseCube(ID);
            RefreshCubes();
            cv_ID[ID-1].notify_one();
        }
        else
        { 
            printf("ID:%d , msg:%c\n",ID,buffer );
            MoveCube(ID, buffer);
            lk1.unlock();
            cv_ID[ID-1].notify_all();
        }
        buffer = 0;
    }
}

int main(int argc, char const *argv[]) 
{
    {
        Point_s left = {.x=0, .y=240}, right = {.x=639, .y=240};
        DrawLine(left,right,BLUE);
        Point_s bottom = {.x=320, .y=0}, top = {.x=320, .y=479};
        DrawLine(top,bottom,BLUE);
    	updateScreen();
    }

    char buffer = 0;

    std::unique_ptr<Server> server = std::unique_ptr<Server>(new Server); 

    std::thread client_thread[4];
    for(int i=0;i<4;i++)
    {
        client_thread[i] = std::thread(&Server::CommunicationWithClient, server.get(), i+1);
    }

    server->ConnectNewClient();

    for(int i=0;i<4;++i)
    {
        client_thread[i].join();
    }
    server.release();

    return 0; 
} 
