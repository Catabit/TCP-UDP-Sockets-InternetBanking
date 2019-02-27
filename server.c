#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "util.h"

#define MAX_CLIENTS	5
#define BUFLEN 256


typedef enum {Offline, Online} conn_state_t;

typedef struct
{
    char nume[13];
    char prenume[13];
    unsigned int card_num;
    unsigned short pin;
    char parola[9];
    double sold;
    char failed_login_attempts;
} client_t;

typedef struct
{
    client_t* target;
    double sum;
} transfer_t;

typedef struct
{
    client_t* client;
    conn_state_t state;
    int sockfd;
    int lastSockfd;
    transfer_t* pendingTransfer;
} session_t;



void printClient(client_t client)
{
    printf("%s %s, %d %d %s %lf\n", client.nume, client.prenume, client.card_num, client.pin, client.parola, client.sold);
    fflush(stdout);
}

client_t* getClient(client_t* clients[], int client_count, unsigned int card)
{
    for (int i=0; i<client_count; i++)
    {
        if ((*clients[i]).card_num==card)
            return clients[i];
    }
    return NULL;
}

session_t* mkNewSession(client_t* client, int sockfd)
{
    session_t* result = malloc(sizeof(session_t));

    (*result).client = client;
    (*result).sockfd = sockfd;
    (*result).state = Online;
    (*result).lastSockfd = -1;
    (*result).pendingTransfer = NULL;

    return result;
}

session_t* getSession(session_t* open_sessions[], int session_count, client_t* client)
{
    if (session_count>0)
    {
        for (int i=0; i<session_count; i++)
        {
            //if ((*open_sessions[i]).state==Offline;) continue;

            if ((*(*open_sessions[i]).client).card_num == (*client).card_num)
                return open_sessions[i];
        }
    }
    return NULL;
}

session_t* getSessionBySocket(session_t* open_sessions[], int session_count, int client_socket)
{
    if (session_count>0)
    {
        for (int i=0; i<session_count; i++)
        {
            if ((*open_sessions[i]).state==Offline) continue;

            if ((*open_sessions[i]).sockfd == client_socket)
                return open_sessions[i];
        }
    }
    return NULL;
}

void closeSession(session_t* open_sessions[], int session_count, session_t* session)
{
    if (session_count>0 && session!=NULL)
    {
        for (int i=0; i<session_count; i++)
        {
            //if (open_sessions[i]==NULL) continue;

            if (open_sessions[i] == session)
            {
                (*open_sessions[i]).state=Offline;
                //(*(*open_sessions[i]).client).failed_login_attempts=0;
                printf("Sesiunea %d a fost inchisa\n", i);
                return;
            }
        }
    }
}

transfer_t* mkTransfer(client_t* target, double sum)
{
    transfer_t* result = malloc(sizeof(transfer_t));

    (*result).target = target;
    (*result).sum = sum;

    return result;
}

int sendTcpMsg(int dest, char code, char* message)
{
    char buf[strlen(message+1)];
    strcpy(buf+1, message);
    buf[0]=code;
    send(dest,buf,strlen(buf), 0);

    printf("TCP sent: %d : %s\n", code, message);
}

void sendUdpMsg(int sockfd, struct sockaddr_in client, char code, char* message)
{
    char msg[strlen(message)+1];
    msg[0]=code;
    strcpy(msg+1, message);
    sendto(sockfd, msg, strlen(msg), 0, (struct sockaddr*) &client, sizeof(client));

    printf("UDP sent: %d : %s\n", code, message);
}


int main(int argc, char *argv[])
{
    int sockfd_tcp, sockfd_udp, portno;
    char buffer[BUFLEN];
    struct sockaddr_in serv_addr, cli_addr, udp_client;
    int i;
    socklen_t udp_socklen;

    session_t* sessions[MAX_CLIENTS];
    int session_count=0;


    fd_set read_fds;	//multimea de citire folosita in select()
    fd_set tmp_fds;	//multime folosita temporar
    int fdmax;		//valoare maxima file descriptor din multimea read_fds

    if (argc < 3)
    {
        fprintf(stderr,"Usage : %s port users_file\n", argv[0]);
        exit(1);
    }


    //golim multimea de descriptori de citire (read_fds) si multimea tmp_fds
    FD_ZERO(&read_fds);
    FD_ZERO(&tmp_fds);

    sockfd_tcp = socket(AF_INET, SOCK_STREAM, 0);
    sockfd_udp = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd_tcp < 0 || sockfd_udp <0)
        error("ERROR opening socket");

    portno = atoi(argv[1]);

    memset((char *) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;	// foloseste adresa IP a masinii
    serv_addr.sin_port = htons(portno);

    if (bind(sockfd_tcp, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr)) < 0)
        error("ERROR on TCP binding");

    if (bind(sockfd_udp, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr)) < 0)
        error("ERROR on UDP binding");


    int yes = 1;
    if (setsockopt(sockfd_tcp,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int)) == -1)
    {
        printf("setsockopt error\n");
        return -1;
    }
    if (setsockopt(sockfd_udp,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int)) == -1)
    {
        printf("setsockopt error\n");
        return -1;
    }


    listen(sockfd_tcp, MAX_CLIENTS);

    //adaugam noul file descriptor (socketul pe care se asculta conexiuni) in multimea read_fds
    FD_SET(0, &read_fds);
    FD_SET(sockfd_tcp, &read_fds);
    FD_SET(sockfd_udp, &read_fds);
    fdmax = sockfd_tcp>sockfd_udp?sockfd_tcp:sockfd_udp;


    //citesc fisierul de configurare
    FILE* input_f = fopen(argv[2], "r");
    int num_clients;
    fscanf(input_f, "%d", &num_clients);
    client_t* clients[num_clients];

    for (int i=0; i<num_clients; i++)
    {
        client_t *result = malloc(sizeof(client_t));
        fscanf(input_f, "%12s %12s %6d %4hu %8s %lf",(*result).nume, (*result).prenume, &((*result).card_num), &((*result).pin),(*result).parola, &((*result).sold));

        (*result).failed_login_attempts = 0;
        clients[i]=result;
        //printClient(*clients[i]);
    }

    // main loop
    while (1)
    {
        tmp_fds = read_fds;
        if (select(fdmax + 1, &tmp_fds, NULL, NULL, NULL) == -1)
            error("ERROR in select");

        for(i = 0; i <= fdmax; i++)
        {
            if (FD_ISSET(i, &tmp_fds))
            {
                if (i==0) // citesc de la tastatura
                {
                    memset(buffer, 0, BUFLEN);
                    fgets(buffer, BUFLEN-1, stdin);

                    char **tokens = strsplit(buffer, " ");
                    int token_count = 0;
                    while (tokens[token_count]!=NULL)
                    {
                        token_count++;
                    }
                    tokens[token_count-1][strlen(tokens[token_count-1])-1]=0;

                    if (strcmp(tokens[0], QUIT)==0 && token_count==1)
                    {
                        for (int k=0; k<session_count; k++)
                        {
                            if ((*sessions[k]).state==Online)
                            {
                                sendTcpMsg((*sessions[k]).sockfd, OK, "Serverul inchide conexiunea");
                                close((*sessions[k]).sockfd);
                            }
                            free(sessions[k]);
                        }

                        for (int k=0; k<num_clients; k++)
                        {
                            free(clients[k]);
                        }
                        free(tokens);
                        close(sockfd_tcp);
                        close(sockfd_udp);
                        return 0;

                    }
                    free(tokens);



                }
                else if (i == sockfd_tcp)
                {
                    // a venit ceva pe socketul inactiv(cel cu listen) = o noua conexiune
                    // actiunea serverului: accept()
                    unsigned int clilen = sizeof(cli_addr);
                    int newsockfd;
                    if ((newsockfd = accept(sockfd_tcp, (struct sockaddr *)&cli_addr, &clilen)) == -1)
                    {
                        error("ERROR in accept");
                    }
                    else
                    {
                        //adaug noul socket intors de accept() la multimea descriptorilor de citire
                        FD_SET(newsockfd, &read_fds);
                        if (newsockfd>fdmax)
                        {
                            fdmax = newsockfd;
                        }

                    }
                    printf("Noua conexiune de la %s, port %d, socket_client %d\n ", inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port), newsockfd);
                }
                else if (i == sockfd_udp)
                {
                    memset(buffer, 0, BUFLEN);
                    memset(&udp_client, 0, sizeof(udp_client));
                    int numRead = recvfrom(sockfd_udp, buffer, BUFLEN, 0, (struct sockaddr*) &udp_client, &udp_socklen);

                    printf("UDP [%d] from %s : %d   message: %s \n",numRead, inet_ntoa(udp_client.sin_addr), ntohs(udp_client.sin_port), buffer);
                    //sendUdpMsg(sockfd_udp, udp_client, '>', buffer);

                    char **tokens = strsplit(buffer, " ");
                        int token_count = 0;
                        while (tokens[token_count]!=NULL)
                        {
                            token_count++;
                        }
                        if (tokens[token_count-1][strlen(tokens[token_count-1])-1]=='\n')
                            tokens[token_count-1][strlen(tokens[token_count-1])-1]=0;



                    fflush(stdout);
                    if (strcmp(tokens[0], UNLOCK)==0 && token_count==2)
                    {
                        client_t* client = getClient(clients, num_clients, atoi(tokens[1]));
                        if (client!=NULL)
                        {
                            sendUdpMsg(sockfd_udp, udp_client, PASSWORDNEEDED, PASSWORDNEEDED_MSG);
                        }
                        else
                        {
                            sendUdpMsg(sockfd_udp, udp_client, WRONGCARD, WRONGCARD_MSG);
                        }
                    }
                    if (strcmp(tokens[0], UNLOCK)==0 && token_count==3)
                    {
                        //tokens[2][strlen(tokens[2])-1]=0;
                        client_t* client = getClient(clients, num_clients, atoi(tokens[1]));
                        if (client!=NULL)
                        {
                            if ((*client).failed_login_attempts <3) //TODO change to 3
                            {
                                sendUdpMsg(sockfd_udp, udp_client, FAIL, FAIL_MSG);
                                continue;
                            }
                            printf("local [%s], remote [%s]: %d\n",(*client).parola, tokens[2], strcmp((*client).parola, tokens[2]));
                            if (strcmp((*client).parola, tokens[2])==0)
                            {
                                (*client).failed_login_attempts=0;
                                sendUdpMsg(sockfd_udp, udp_client, OK, "Card deblocat");
                            }
                            else
                            {
                                sendUdpMsg(sockfd_udp, udp_client, UNLOCKFAIL, UNLOCKFAIL_MSG);
                            }

                        }
                        else
                        {
                            sendUdpMsg(sockfd_udp, udp_client, WRONGCARD, WRONGCARD_MSG);
                        }
                    }
                    free(tokens);





                }
                else
                {
                    // am primit date pe unul din socketii cu care vorbesc cu clientii
                    //actiunea serverului: recv()
                    memset(buffer, 0, BUFLEN);
                    int numRead;
                    if ((numRead = recv(i, buffer, sizeof(buffer), 0)) <= 0)
                    {
                        if (numRead == 0)
                        {
                            //conexiunea s-a inchis
                            printf("selectserver: socket %d hung up\n", i);
                        }
                        else
                        {
                            error("ERROR in recv");
                        }
                        close(i);

                        FD_CLR(i, &read_fds); // scoatem din multimea de citire socketul pe care

                    }

                    else   //recv intoarce >0
                    {
                        printf ("Am primit de la clientul de pe socketul %d, mesajul: %s\n", i, buffer);

                        char **tokens = strsplit(buffer, " ");
                        int token_count = 0;
                        while (tokens[token_count]!=NULL)
                        {
                            token_count++;
                        }
                        if (tokens[token_count-1][strlen(tokens[token_count-1])-1]=='\n')
                            tokens[token_count-1][strlen(tokens[token_count-1])-1]=0;

                        if (tokens[0][0]==YESCHR)
                        {
                            session_t* active = getSessionBySocket(sessions, session_count, i);
                            if (active!=NULL)
                            {
                                if ((*active).pendingTransfer != NULL)
                                {
                                    double sum = (*(*active).pendingTransfer).sum;
                                    (*(*active).client).sold -=sum;
                                    (*(*(*active).pendingTransfer).target).sold +=sum;
                                    free((*active).pendingTransfer);
                                    (*active).pendingTransfer = NULL;

                                    char temp[50];
                                    sprintf(temp, "Transfer realizat cu succes");
                                    sendTcpMsg(i, OK, temp);
                                    continue;
                                }
                            }
                        }
                        else
                        {
                            session_t* active = getSessionBySocket(sessions, session_count, i);
                            if (active!=NULL)
                            {
                                if ((*active).pendingTransfer != NULL)
                                {
                                    free((*active).pendingTransfer);
                                    (*active).pendingTransfer = NULL;
                                    sendTcpMsg(i, CANCELED, CANCELED_MSG);
                                    continue;
                                }
                            }
                        }


                        if (strcmp(tokens[0], LOGIN)==0 && token_count==3)
                        {
                            int numar_card = atoi(tokens[1]);
                            int pin = atoi(tokens[2]);

                            client_t* client = getClient(clients, num_clients, numar_card);
                            if (client == NULL)
                            {
                                sendTcpMsg(i, WRONGCARD, WRONGCARD_MSG);
                            }
                            else
                            {
                                if (getSession(sessions, session_count, client) == NULL)
                                {
                                    if ((*client).pin==pin && (*client).failed_login_attempts<3)
                                    {
                                        (*client).failed_login_attempts=0;
                                        sessions[session_count++] = mkNewSession(client, i);
                                        printf("Clientul %s %s s-a logat [Sesiune %d]\n", (*client).nume, (*client).prenume, session_count-1);
                                        fflush(stdout);
                                        char temp[50];
                                        sprintf(temp, "Welcome %s %s", (*client).nume, (*client).prenume);
                                        sendTcpMsg(i, OK, temp);

                                    }
                                    else
                                    {
                                        (*client).failed_login_attempts++;
                                        if ((*client).failed_login_attempts <3 )
                                        {
                                            sendTcpMsg(i, WRONGPIN, WRONGPIN_MSG);

                                        }
                                        else
                                        {
                                            sendTcpMsg(i, CARDLOCKED, CARDLOCKED_MSG);
                                        }
                                    }
                                }
                                else if((*getSession(sessions, session_count, client)).state==Offline)
                                {
                                if ((*client).pin==pin && (*client).failed_login_attempts<3)
                                    {
                                        (*client).failed_login_attempts=0;
                                        (*getSession(sessions, session_count, client)).state=Online;
                                        (*getSession(sessions, session_count, client)).sockfd=i;
                                        printf("Clientul %s %s s-a relogat\n", (*client).nume, (*client).prenume);
                                        fflush(stdout);
                                        char temp[50];
                                        sprintf(temp, "Welcome %s %s", (*client).nume, (*client).prenume);
                                        sendTcpMsg(i, OK, temp);

                                    }
                                    else
                                    {
                                        (*client).failed_login_attempts++;
                                        if ((*client).failed_login_attempts <3 )
                                        {
                                            sendTcpMsg(i, WRONGPIN, WRONGPIN_MSG);

                                        }
                                        else
                                        {
                                            sendTcpMsg(i, CARDLOCKED, CARDLOCKED_MSG);
                                        }
                                    }
                                }
                                else
                                {
                                    sendTcpMsg(i, SESSIONACTIVE, SESSIONACTIVE_MSG);
                                }
                            }
                        }
                        else if (strcmp(tokens[0], LOGOUT)==0 && token_count==1)
                        {
                            closeSession(sessions, session_count, getSessionBySocket(sessions, session_count, i));
                            sendTcpMsg(i, OK, "Deconectare de la bancomat");

                        }
                        else if (strcmp(tokens[0], QUIT)==0  && token_count==1)
                        {
                            closeSession(sessions, session_count, getSessionBySocket(sessions, session_count, i));
                            printf("Clientul de pe socketul %d inchide conexiunea\n", i);
                            fflush(stdout);
                        }
                        else if (strcmp(tokens[0], TRANSF)==0 && token_count==3)
                        {
                            int numar_card = atoi(tokens[1]);
                            double suma = atof(tokens[2]);

                            client_t* client = getClient(clients, num_clients, numar_card);
                            if (client == NULL)
                            {
                                sendTcpMsg(i, WRONGCARD, WRONGCARD_MSG);
                            }
                            else
                            {
                                session_t* active = getSessionBySocket(sessions, session_count, i);
                                if (active != NULL && (*(*active).client).sold >= suma)
                                {
                                    (*active).pendingTransfer = mkTransfer(client, suma);
                                    char temp[100];
                                    sprintf(temp, "Transfer %.2lf catre %s %s? [y/n]", suma, (*client).nume, (*client).prenume);
                                    sendTcpMsg(i, OK, temp);
                                }
                                else
                                {
                                    sendTcpMsg(i, INSUFFICIENTFUNDS, INSUFFICIENTFUNDS_MSG);
                                }
                            }
                        }
                        else if (strcmp(tokens[0], SOLD)==0  && token_count==1)
                        {
                            session_t* active = getSessionBySocket(sessions, session_count, i);
                            if (active != NULL)
                            {
                                char temp[50];
                                sprintf(temp, "%.2lf", (*(*active).client).sold);
                                sendTcpMsg(i, OK, temp);
                            }
                        }
                        free(tokens);
                    }
                }
            }
        }
    }

    close(sockfd_udp);
    close(sockfd_tcp);

    return 0;
}


