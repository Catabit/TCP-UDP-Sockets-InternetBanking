#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include "util.h"

#define BUFLEN 256

int main(int argc, char *argv[])
{
    int sockfd_tcp, sockfd_udp, n;
    struct sockaddr_in serv_addr;
    struct hostent *server;

    fd_set read_fds;    //multimea de citire folosita in select()
    fd_set tmp_fds;    //multime folosita temporar
    int fdmax;     //valoare maxima file descriptor din multimea read_fds

    char logfile_name[25];
    sprintf(logfile_name, "client-%d.log", getpid());
    char lastCommand[100];


    unsigned int lastcard = -1;
    char isLoggedIn = 0;
    char isWaitingForPwd = 0;
    char isWaitingForLogin = 0;
    char isWaitingForTransfer = 0;

    char buffer[BUFLEN];
    if (argc < 3)
    {
        fprintf(stderr,"Usage %s server_address server_port\n", argv[0]);
        exit(0);
    }

    FD_ZERO(&read_fds);
    FD_ZERO(&tmp_fds);

    sockfd_tcp = socket(AF_INET, SOCK_STREAM, 0);
    sockfd_udp = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd_tcp < 0)
        error("ERROR opening TCP socket");
    if (sockfd_udp < 0)
        error("ERROR opening UDP socket");


    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[2]));
    inet_aton(argv[1], &serv_addr.sin_addr);


    if (connect(sockfd_tcp,(struct sockaddr*) &serv_addr,sizeof(serv_addr)) < 0)
        error("ERROR connecting");

    FD_SET(0, &read_fds); // fd 0
    FD_SET(sockfd_tcp, &read_fds);
    FD_SET(sockfd_udp, &read_fds);
    fdmax = sockfd_tcp>sockfd_udp?sockfd_tcp:sockfd_udp;

    FILE* logfile = fopen(logfile_name, "wt");

    while(1)
    {
        tmp_fds = read_fds;
        if (select(fdmax + 1, &tmp_fds, NULL, NULL, NULL) == -1)
        {
            error("ERROR in select");
            break;
        }

        for(int i = 0; i <= fdmax; i++)
        {
            if (FD_ISSET(i, &tmp_fds))
            {
                if (i==0)
                {
                    //citesc de la tastatura
                    memset(buffer, 0, BUFLEN);
                    fgets(buffer, BUFLEN-1, stdin);

                    char **tokens = strsplit(buffer, " ");
                    int token_count = 0;
                    while (tokens[token_count]!=NULL)
                    {
                        token_count++;
                    }
                    tokens[token_count-1][strlen(tokens[token_count-1])-1]=0;
                    strcpy(lastCommand, buffer);



                    fflush(stdout);
                    if (isWaitingForPwd)
                    {

                        char buf[50];
                        sprintf(buf, "unlock %d %s", lastcard, tokens[0]);
                        strcpy(lastCommand, tokens[0]);
                        strcat(lastCommand, "\n");

                        n = sendto(sockfd_udp, buf, strlen(buf), 0, (struct sockaddr*) &serv_addr, sizeof(serv_addr));
                        isWaitingForPwd = 0;
                        continue;
                    }

                    if (isWaitingForTransfer)
                    {
                        n = send(sockfd_tcp, buffer, strlen(buffer), 0);
                        strcpy(lastCommand, buffer);
                        isWaitingForTransfer = 0;
                        continue;
                    }

                    if (strcmp(tokens[0], UNLOCK)==0 && token_count==1)
                    {
                        char buf[25];
                        sprintf(buf, "unlock %d", lastcard);
                        strcpy(lastCommand, "unlock\n");

                        n = sendto(sockfd_udp, buf, strlen(buf), 0, (struct sockaddr*) &serv_addr, sizeof(serv_addr));
                        continue;
                    }

                    if (strcmp(tokens[0], LOGIN)==0 && token_count==3)
                    {
                        if (isLoggedIn)
                        {
                            printf("IBANK> %d : %s\n", SESSIONACTIVE, SESSIONACTIVE_MSG);
                            fprintf(logfile, "%s", buffer);
                            fprintf(logfile, "IBANK> %d : %s\n", SESSIONACTIVE, SESSIONACTIVE_MSG);
                            continue;
                        }

                        strcpy(lastCommand, buffer);
                        n = send(sockfd_tcp, buffer, strlen(buffer), 0);
                        isWaitingForLogin = 1;
                        lastcard = atoi(tokens[1]);
                        continue;
                    }

                    if (strcmp(tokens[0], QUIT)==0 && token_count==1)
                    {
                        n = send(sockfd_tcp, buffer, strlen(buffer), 0);
                        fprintf(logfile, "quit\n");
                        close(sockfd_tcp);
                        close(sockfd_udp);
                        close(logfile);
                        return 0;
                    }

                    if (isLoggedIn)
                    {
                        if (strcmp(tokens[0], LOGOUT)==0 && token_count==1)
                        {
                            n = send(sockfd_tcp, buffer, strlen(buffer), 0);
                            strcpy(lastCommand, buffer);
                            isLoggedIn = 0;
                            lastcard = -1;
                            isWaitingForPwd = 0;
                            isWaitingForLogin = 0;
                        }

                        if (strcmp(tokens[0], SOLD)==0 && token_count==1)
                        {
                            n = send(sockfd_tcp, buffer, strlen(buffer), 0);
                            strcpy(lastCommand, buffer);
                        }

                        if (strcmp(tokens[0], TRANSF)==0 && token_count==3)
                        {
                            n = send(sockfd_tcp, buffer, strlen(buffer), 0);
                            strcpy(lastCommand, buffer);
                            isWaitingForTransfer = 1;
                        }
                    }
                    else
                    {
                        printf("%d : %s\n", NOLOGIN, NOLOGIN_MSG);
                        fprintf(logfile, "%s", lastCommand);
                        fprintf(logfile, "%d : %s\n", NOLOGIN, NOLOGIN_MSG);
                        continue;
                    }

                    if (n < 0)
                    {
                        error("ERROR writing to socket");
                        break;
                    }
                    free(tokens);

                    //trimit mesaj la server
                    //n = send(sockfd_tcp,buffer,strlen(buffer), 0);

                }
                else
                {
                    //got data from the server
                    memset(buffer, 0, BUFLEN);
                    if ((n = recv(i, buffer, BUFLEN, 0)) <= 0)
                    {
                        if (n == 0)
                        {
                            //conexiunea s-a inchis
                            printf("Serverul a inchis conexiunea\n");
                            close(sockfd_tcp);
                            close(sockfd_udp);
                            close(logfile);
                            return 0;
                        }
                        else
                        {
                            error("ERROR in recv");
                        }
                        close(i);
                        FD_CLR(i, &read_fds); // scoatem din multimea de citire socketul pe care
                        return 0;

                    }
                    else
                    {
                        //printf("Got from server on socket %d data:\n%s\n",i, buffer);

                        fprintf(logfile, "%s", lastCommand);
                        if (i==sockfd_udp)
                        {
                            printf("UNLOCK> ");
                            fprintf(logfile, "UNLOCK> ");
                        }
                        else
                        {
                            printf("IBANK> ");
                            fprintf(logfile, "IBANK> ");
                        }

                        printf("%d : %s\n", buffer[0], buffer+1);
                        fprintf(logfile, "%d : %s\n", buffer[0], buffer+1);



                        if (buffer[0]==PASSWORDNEEDED)
                        {
                            isWaitingForPwd = 1;
                        }

                        if (isWaitingForLogin)
                        {
                            isWaitingForLogin=0;
                            if (buffer[0]==OK)
                                isLoggedIn = 1;
                        }

                        if (isWaitingForTransfer && buffer[0] != OK)
                            isWaitingForTransfer = 0;

                        fflush(stdout);
                    }

                }
            }
        }
    }
    return 0;
}


