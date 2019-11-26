#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <termios.h>

int main(int argc, char **argv)
{
    int sockfd = 0, n = 0, c;
    char recvBuff[1024];
    struct sockaddr_in serv_addr;
    
    static struct termios oldt, newt;
    tcgetattr( STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON);          
    tcsetattr( STDIN_FILENO, TCSANOW, &newt);
    
    if(argc != 2)
    {
        printf("\n Usage: %s <ip of server> \n",argv[0]);
        return -1;
    }
 
    memset(recvBuff, 0,sizeof(recvBuff));
    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("\n Error : Could not create socket \n");
        return -1;
    }
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(5001);
    
    if(inet_pton(AF_INET, argv[1], &serv_addr.sin_addr)<=0)
    {
        printf("\n inet_pton error occured\n");
        return -1;
    }

    if(connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        printf("\n Error : Connect Failed \n");
        return -1;
    }

    while(1)
    {
	    int c = getchar();
       	if(c)
	    	{
	      	write (sockfd, &c, 1);
				printf("\t sent\n\n");
    	    	if(c == 'q' || c == 'Q')
		        break;
	    	}
	 }
    tcsetattr( STDIN_FILENO, TCSANOW, &oldt);
    return 0;
}
