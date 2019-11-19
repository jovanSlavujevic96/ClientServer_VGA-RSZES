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

/* delimiter ce se koristiti prilikom prijema spiska datoteka od FTP servera */
char delimiter [2] = "\n";
/* isti enum, jednostruko povezana lista i funkcije za
manipulisanje istom kao u slucaju FTP servera*/

typedef enum {IDLE, LS, FILE_SIZE, RECEIVING } ftp_states;

struct Files 
{
    int number;
    char * name;
    struct Files * next;
};

void addtolist(struct Files ** phead, int num, char * str)
{
    struct Files * ptr;
    if (*phead == NULL)
    {
    *phead = (struct Files *) malloc(sizeof(struct Files));
    ptr = *phead;
        if (ptr == NULL)
        {
            printf("Could not allocate memory for struct Files\n");
            exit(1);
        }
    }
    else
    {
        ptr = *phead;
        while (ptr->next != NULL)
            ptr = ptr->next;
        ptr->next = (struct Files *) malloc(sizeof(struct Files));
        if (ptr->next == NULL)
        {
            printf("Could not allocate memory for struct Files\n");
            exit(1);
        }
        ptr=ptr->next;
    }
    ptr->number = num;
    ptr->name = (char *) malloc(strlen(str)+1);
    if (ptr->name == NULL)
    {
        printf("Could not allocate memory for field name\n");
        exit(1);
    }
    strcpy(ptr->name, str);
    ptr->next = NULL;
}

void printoutlist (struct Files * head)
{
    while (head != NULL)
    {
        printf("(%d)..... %s\n",head->number,head->name);
        head = head->next;
    }
}

char * getfromlist(struct Files * head,int id)
{
    struct Files * ptr = head;
    while(ptr != NULL)
    {
        if (ptr->number == id)
            return ptr->name;
        ptr = ptr->next;
    }
    return NULL;
}

void freelist(struct Files ** phead)
{
    struct Files * ptr = *phead;
    while (*phead != NULL)
    {
        free(ptr->name);
        ptr=ptr->next;
        free(*phead);
        *phead = ptr;
    }
}

/* definisanje globalnih promenljivih */
struct Files * head = NULL;
int selected = 0;
FILE * file;
int filesize = 0;

int main(int argc, char **argv)
{
    int sockfd = 0, n = 0;
    char recvBuff[1024];
    struct sockaddr_in serv_addr;
    char message [100];
    int done;
    int number;
    char tmpstr [30];
    int received;
    char * token;
    char ans;
    ftp_states stat;
    char path[30];

     /* klijentska aplikacija se poziva sa ./ime_aplikacija ip_adresa_servera */
    if(argc != 2)
    {
        printf("\n Usage: %s <ip of server> \n",argv[0]);
        return 1;
    }
 
    memset(recvBuff, 0,sizeof(recvBuff));
    /* kreiraj socket za komunikaciju sa serverom */
    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("\n Error : Could not create socket \n");
        return 1;
    }
    memset(&serv_addr, 0, sizeof(serv_addr));

    /* kao i podatke neophodne za komunikaciju sa serverom */
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(5001);
    /* inet_pton konvertuje ip adresu iz stringa u format
    neophodan za serv_addr strukturu */
    if(inet_pton(AF_INET, argv[1], &serv_addr.sin_addr)<=0)
    {
        printf("\n inet_pton error occured\n");
        return 1;
    }

    /* povezi se sa serverom definisanim preko ip adrese i porta */
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        printf("\n Error : Connect Failed \n");
        return 1;
    }

    /* udji u FSM u kome ces preuzimati datoteke (jednu ili vise) */
    done = 0;
    stat = IDLE;
 
    while (!done)
    {
        switch(stat)
        {
        case IDLE:
            printf("type 'ls' for listing FTP download directory\n> ");
            gets(message);
            /* trenutno jedino ls komanda je podrzana od strane naseg klijenta */
            while (strcmp(message,"ls") != 0)
            {
                printf("Currently supported FTP commands:\n \
                ls - print FTP server download directory content\n> ");
                gets(message);
            }
            /* posalji komandu serveru kako bi nam
            poslao sadrzaj download direktorijuma*/
            write (sockfd, message, strlen(message));
            stat = LS;
        break;

        case LS:
            n = read (sockfd, recvBuff, 1024);
            recvBuff[n] = 0; //terminiraj primljeni string kako bi ga mogao ispisati
            /* parsiraj primljen sadrzaj direktorijuma od servera
            u kome se nalaze u svakom redu ime datoteke sa svojim id-jem*/
            token = strtok(recvBuff, delimiter);
            /* ne znamo koliko ima redova u primljenoj poruci
            pa pretrazuj primljeni string red po red */
            while (token != NULL){
            sscanf(token,"(%d) %s",&number, tmpstr);
            /* dodaj u listu prepoznate podatke iz primljenog stringa */
            addtolist(&head,number,tmpstr);
            token =strtok(NULL,delimiter);
            }
            printoutlist(head);
            printf("Please enter id of a file you want to download and press ENTER:\n> ");
            gets(message);
            /* konvertuj string u broj kako bi mogao da pretrazujes listu*/
            selected = atoi(message);
            /* takodje, posalji serveru id datoteke koja je odabrana */
            write(sockfd, message, strlen(message));
            /* otvori datoteku sa istim imenom kako bi u nju mogli da se
            upisuju podaci koji se primaju od strane FTP servera.
            NAPOMENA: na LPC ploci jedino u /tmp direktorijumu mogu da se kreiraju
            datoteke, ostatak je read-only */
            strcpy(path,"/tmp/");
            strcat(path,getfromlist(head,selected));
            file = fopen(path,"w");
            if (file == NULL){
            printf("Error opening file:%s", getfromlist(head,selected));
            exit(1);
            }
            stat = FILE_SIZE;
        break;

        case FILE_SIZE:
            /* server najpre salje duzinu odabrane datoteke
            kako bismo mogli da znamo sta da ocekujemo prilikom
            prijema sadrzaja datoteke i da prepoznamo kraj datoteke */
            n = read (sockfd, recvBuff, 1024);
            recvBuff[n] = 0;
            filesize = atoi(recvBuff);
            printf("%s file size: %d bytes.\n",getfromlist(head,selected),filesize);
            /* vrati potvrdu serveru i predji u stanje prijema datoteke */
            strcpy(message,"OK");
            write(sockfd, message, strlen(message));
            stat = RECEIVING;
        break;

        case RECEIVING:
            received = 0;
            /* sve dok ne primis unapred definisani broj bajtova, nastavi
            da primas podatke sa servera i da upisujes u datoteku u /tmp direktorijumu */
            while (received != filesize){
            n = read (sockfd, recvBuff, 256);
            received += n;
            fwrite(recvBuff, 1, n, file);
            }
            fclose(file);
            printf("\nFile %s successfuly received.\n \
            (c)ontinue or (q)uit? \n>", getfromlist(head, selected)); 
            ans = getchar();
            getchar(); //getchar uvek prihvati prvi karakter ali ceka na newline '\n'
            if (ans == 'c')
            stat = IDLE;
            else
            done = 1;
            /* svakako nam ne treba vise prethodna lista jer je moguce da se sadrzaj
            FTP direktorijuma menjao u medjuvremenu dok smo download-ovali prethodnu
            datoteku */
            freelist(&head);
            /* i posalji odluku serveru kako bi znao
            da li da terminira trenutno aktivnu konekciju */
            write(sockfd,&ans,1);
            printf("\n");
        break;
        }
    }
    return 0;
}