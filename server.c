#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>

/*definisanje enumeracije neophodne za implementaciju FSM koja
se koristi kako za slanje tako i za prijem datoteka*/
typedef enum { IDLE, LS, FILE_SIZE, SENDING } ftp_state;

/*definisanje strukture koja ce se koristiti za dinamicke strukture
podataka koje se kreiraju u toku izvrsavanja programa (runtime)*/
struct Files 
{
    int number;
    char * name;
    struct Files * next;
};

/*promenljive koje ce biti koriscene*/
struct Files * head = NULL;
DIR * d;
struct dirent * dir;
ftp_state stat = IDLE; 
int selection = 0;
int filesize;
int sent = 0;
FILE * file;

/* addtolist, getfromlist, printoulist i freelist su funkcije koje
se koriste za manipulisanje jednostruko povezanim listama kao
osnovom koriscenom u radu sa dinamickim strukturama*/
/*phead predstavlja “glavu” povezane liste, a num i str
podatke koje treba upisati u novo-kreirani element liste*/
void addtolist(struct Files ** phead, int num, char * str)
{
    struct Files * ptr;

    /*ukoliko je phead NULL to znaci da je lista prazna*/
    if(*phead == NULL)
    {
        *phead = (struct Files *) malloc(sizeof(struct Files));
        ptr = *phead;
        if(ptr == NULL)
        {
            printf("Error allocating memory..\n");
            exit(1);
        }
    }
    else
    {   /*lista nije prazna, dodaj samo jos jedan element na kraju liste*/
        ptr=*phead;
        while(ptr->next != NULL)
            ptr=ptr->next;
        
        ptr->next = (struct Files *) malloc(sizeof(struct Files));
        if(ptr->next == NULL)
        {
            printf("Error allocating memory..\n");
            exit(1);
        }
        ptr=ptr->next;
    }
    /*dodat je novi element, sada ga popuni
    potrebnim ulaznim podacima*/
    ptr->number = num;
    ptr->name = (char *) malloc (strlen(str));
    
    if (ptr->name == NULL)
    {
        printf("Could not allocate memory..\n");
        exit(1);
    }
    strcpy(ptr->name, str);
    ptr->next = NULL;
}

/*pronadji u listi element liste preko njegovog id-ja */
char * getfromlist(struct Files * head, int id)
{
    while(head != NULL)
    {
        if (head->number == id)
        return head->name;
        head=head->next;
    }
    /*ili vrati NULL ukoliko nema takvog*/
    return NULL;
}
    
/*ispisi sadrzaj liste jedan po jedan element*/
void printoutlist(struct Files * head)
{
    while (head!=NULL)
    {
        printf("(%d) %s\n",head->number,head->name);
        head = head->next;
    }
}

/*oslobodi prethodno zauzete memorijske resurse*/
void freelist(struct Files ** phead)
{
    struct Files * ptr = *phead;
    while(*phead != NULL)
    {
        free(ptr->name);
        ptr=ptr->next;
        free(*phead);
        *phead = ptr;
    }
} 

/*ovo je jezgro serverske funkcionalnosti-ova funkcija se poziva
nakon uspostavljanja veze sa klijentom kako bi klijentu bilo
omoguceno da preuzme sa servera zahtevane datoteke. */
void doprocessing (int sock)
{
    /*lokalne promenljive*/
    int n;
    char buffer[256];
    char sendBuf[256];
    char tempstr[256];
    bzero(buffer,256);
    int done = 0;
    int i = 0;
    int w = 30;
    while (!done)
    {
        switch(stat)
        {
        
        case IDLE:
            printf("[IDLE] Waiting for ls command..\n");
            n = read(sock,buffer,255);
            buffer[n] = 0;//terminiraj string primljen od strane klijenta
            printf("Received command: %s\n",buffer);
            if (strcmp(buffer,"ls") == 0)
                stat = LS; //predji u naredno stanje FSM
            else
                printf("Could not recognize command string...\n");
            break;

        case LS:
            printf("Content of FTP server download directory:\n");
            i = 1;
            /*sadrzaj trenutnog direktorijuma - FTP direktorijuma*/
            d = opendir(".");
            if (d)
            {
                sendBuf[0]=0;//neophodno zbog strcat od dole
                /*za svaki fajl unutar trenutnog direktorijuma*/
                
                while ((dir = readdir(d)))
                {/*ukoliko je to zaista fajl a na poddirektorijum*/
                    if(dir->d_type != DT_DIR){
            /*dodaj odgovarajuci element u listu*/
            addtolist(&head,i++,dir->d_name);
            /*i kreiraj string za slanje klijentu*/
            sprintf(tempstr, "(%d) %s\n", i-1, dir->d_name);
            strcat(sendBuf,tempstr);
            }
            }
            /*prikazi sadrzaj kreirane liste*/
            printoutlist(head);
            closedir(d);
            /*i posalji klijentu spisak trenutno dostupnih datoteka*/
            write(sock,sendBuf,strlen(sendBuf));
            }
            printf("waiting for id of file to be sent...\n");
            n = read(sock, buffer, 255);
            buffer[n] = 0;
            /*datoteka za slanje bice prepoznata preko svog id-ja*/
            selection = atoi(buffer);
            stat = FILE_SIZE;
        break;

        case FILE_SIZE:
            /*odredi velicinu fajla kako bi klijent znao sta
            da ocekuje prilikom slanja podataka*/
            file = fopen(getfromlist(head,selection),"rb");
            fseek(file,0L,SEEK_END);
            filesize=ftell(file);
            /*i vrati se na pocetak datoteke kako bi bio spreman za slanje*/
            fseek(file,0L,SEEK_SET);
            /*posalji podatak o velicini odabrane datoteke klijentu*/
            sprintf(sendBuf,"%d",filesize);
            write(sock,sendBuf,strlen(sendBuf));
            stat = SENDING;
        break;

        case SENDING:
            sent=0; 
            printf("Sending file %s. File size: %d bytes\n", \
            getfromlist(head, selection), filesize);
            /*sacekaj da dobijes potvrdu klijenta da je spreman za slanje*/
            n = read(sock,buffer,255);
            buffer[n] = 0;//terminiraj string
            printf("Received acknowlegde: %s\n",buffer);
            while(sent!=filesize){
            n = fread(sendBuf,1,256,file);
            sent += n;
            write(sock,sendBuf,n);
            /*progress bar u konzoli - trik*/
            float ratio = sent/(float)filesize;
            int c = ratio * w;
            printf("%3d%% [",(int)(ratio*100) );
            int x;
            for (x=0; x<c; x++)
            printf("=");
            for (x=c; x<w; x++)
            printf(" ");
            if (sent != filesize){
            printf("]\r");
            }else{
            printf("]\n%s succesfully sent! \n", \
            getfromlist(head, selection));
            }
            /*ovo je samo da bi se video transfer i za manje datoteke*/
            usleep(100000L);
            }
            fclose(file);
            /*sacekaj da vidis da li ovaj klijent zeli da preuzima jos datoteka,
            ili je zavrsio sa radom*/
            n = read(sock,buffer,255);
            buffer[n] = 0;
            if (buffer[0] == 'c')
            stat = IDLE;
            else
            done = 1;
            /*cak i ukoliko je klijent zavrsio, unisti listu datoteka
            jer je moguce da se u medjuvremenu azurirala pa treba
            napraviti novu strukturu koja ce prikazati novi sadrzaj*/
            freelist(&head);
        break;
        }
    }
}

/* glavni program serverske aplikacije */
int main( int argc, char **argv )
{
    int sockfd, newsockfd, portno, clilen;
    char buffer[256];
    struct sockaddr_in serv_addr, cli_addr;
    int n;
    /* najpre se poziva uvek socket() funkcija da se registruje socket:
    AF_INET je neophodan kada se zahteva komunikacija bilo koja
    dva host-a na Internetu;
    Drugi argument definise tip socket-a i moze biti SOCK_STREAM ili SOCK_DGRAM:
    SOCK_STREAM odgovara npr. TCP komunikaciji, dok SOCK_DGRAM kreira npr. UDP kanal
    Treci argument je zapravo protokol koji se koristi: najcesce se stavlja 0 sto znaci da
    OS sam odabere podrazumevane protokole za dati tip socket-a (TCP za SOCK_STREAM
    ili UDP za SOCK_DGRAM) */

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        perror("ERROR opening socket");
        return 1;
    }
    /* Inicijalizacija strukture socket-a */
    bzero((char *) &serv_addr, sizeof(serv_addr));
    portno = 5001;
    serv_addr.sin_family = AF_INET; //mora biti AF_INET
    /* ip adresa host-a. INADDR_ANY vraca ip adresu masine na kojoj se startovao server */
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    /* broj porta-ne sme se staviti kao broj vec se mora konvertovati u
    tzv. network byte order funkcijom htons*/
    serv_addr.sin_port = htons(portno); 
    /* Sada bind-ujemo adresu sa prethodno kreiranim socket-om */
    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr) ) < 0)
    {
        perror("ERROR on binding");
        exit(1);
    }
    printf("FTP server started.. waiting for clients ...\n");
    /* postavi prethodno kreirani socket kao pasivan socket
    koji ce prihvatati zahteve za konekcijom od klijenata
    koriscenjem accept funkcije */
    listen(sockfd,5); //maksimalno 5 klijenata moze da koristi moje usluge
    clilen = sizeof(cli_addr);
    
    while (1)
    {
        /*ovde ce cekati sve dok ne stigne zahtev za konekcijom od prvog klijenta*/
        newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
        printf("FTP client connected...\n");
        if (newsockfd < 0)
        {
            perror("ERROR on accept");
            exit(1);
        }
        /* Kreiraj child proces sa ciljem da mozes istovremeno da
        komuniciras sa vise klijenata */
        int pid = fork();
        if (pid < 0)
        {
            perror("ERROR on fork");
            exit(1);
        }
        if (pid == 0)
        {
            /* child proces ima pid 0 te tako mozemo znati da li
            se ovaj deo koda izvrsava u child ili parent procesu */
            close(sockfd);
            doprocessing(newsockfd);
            exit(0);
        }
        else
            close(newsockfd);
            /*ovo je parent proces koji je samo zaduzen da
            delegira poslove child procesima-stoga ne moras
            da radis nista vec samo nastavi da osluskujes
            nove klijente koji salju zahtev za konekcijom*/
    } 
}