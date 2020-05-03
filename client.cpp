#include <stdio.h> 
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <unistd.h> 
#include <string.h>
#define PORT 8080 
#include <termios.h>
#include <unistd.h>
#include <iostream>
#include <string>
#include <sstream>

typedef enum {KeyboardOld_,KeyboardNew_} KeyboardState_t;

void SetKeyboard(const KeyboardState_t state)
{
    static struct termios oldt, newt;
    tcgetattr( STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON);
    auto keyboard = (state == KeyboardOld_) ? oldt : newt;      
    tcsetattr( STDIN_FILENO, TCSANOW, &keyboard);
}
   
bool IsCharSomeOfAllowed(const char character, const char* letters="aAwWsSdD")
{
    for(int i=0; i<strlen(letters); ++i)
    {
        if(character == letters[i])
            return true;
    }
    return false;
}

int ParseID(const char* message)
{
    int length;
    if(message == nullptr || (length=strlen(message))-4 < 0)
        return -1;
    for(int i=0; i<length-3; ++i)
    {
        std::stringstream stream;
        stream << message[i] << message[i+1] << message[i+2];
        if(!strcmp(stream.str().c_str(),"ID:") )
            return (message[i+3]-48);
    }
    return -1;
}

bool ConclusionFromID(const int ID)
{
    std::string screen_side;
    if(ID == 1)
        screen_side = "top left";
    else if(ID == 2)
        screen_side = "top right";
    else if(ID == 3)
        screen_side = "bottom right";
    else if(ID == 4)
        screen_side = "bottom left";
    else
        return false;
    std::cout << "You are moving cube on " << screen_side << " side of the screen\n";
    return true;
}

bool ProtocolInit(int& socket_, const char* IP="127.0.0.1")
{
    struct sockaddr_in serv_addr;
    if ((socket_ = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
    { 
        printf("\n Socket creation error \n"); 
        return false; 
    }
    serv_addr.sin_family = AF_INET; 
    serv_addr.sin_port = htons(PORT);

    // Convert IPv4 and IPv6 addresses from text to binary form 
    if(inet_pton(AF_INET, IP, &serv_addr.sin_addr)<=0)  
    { 
        printf("\nInvalid address/ Address not supported \n"); 
        return false; 
    }

    std::cout << "Pending connection with server..\n";
    while (connect(socket_, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) ;

    int ID;
    {
        char buffer[1024] = {0};
        read(socket_, buffer, 1024); 
        std::cout << "MESSAGE: " << buffer << std::endl;
        ID = ParseID(buffer);
    }
    bool connection_succ = ConclusionFromID(ID);
    if(!connection_succ)
    {
        std::cout << "Connection is not succesfull.\nExit.\n";
        return false;
    }
    std::cout << "Succesfully connected.\n";
    return true;    
}

int main(int argc, char const *argv[]) 
{ 
    SetKeyboard(KeyboardNew_);

    int sock = 0; 
  
    if(!ProtocolInit(sock))
        return -1; 

    while(true)
    {
        int c = getchar();
        if(IsCharSomeOfAllowed(c) == true)
            send(sock, &c , 1, 0) , std::cout << " succesfully sent\n"; 
        else if(c == 'q' || c == 'Q')
        {
            std::cout << " pressed\n"; 
            break;
        }
    }
    SetKeyboard(KeyboardOld_);
    return 0; 
} 