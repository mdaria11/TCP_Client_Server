#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "helpers.h"

struct stdinmessage //structura comanda primita de la tastatura
{
	char command[12];
	char topic[51];
	int sf;
};

struct topicmessage //structura mesaj primit de la server/client UDP
{
	char ip_client_udp[16];
	unsigned short port_client_udp;
	char topic[51];
	char tip_date[20];
	char payload[1500];
};

void usage(char *file) //in caz de argumente invalide
{
	fprintf(stderr, "Usage: %s id_client server_address server_port\n", file);
	exit(0);
}

int main(int argc, char *argv[])
{
	int sockfd, n, ret;
	struct sockaddr_in serv_addr; //contine portul si adresa ip a serverului
	char buffer[BUFLEN]; //bufferul clientului
	int enable=1;

	if (argc < 4) { //caz nr argmumente invalid
		usage(argv[0]);
	}

	if (strlen(argv[1])>10) //caz id invalid
	{
		fprintf(stderr, "ID must have max 10 characters.\n");
		exit(0);
	}
	//dezactivare buffering pt stdout
	setvbuf(stdout, NULL, _IONBF, BUFSIZ);
	fflush(stdout);

	sockfd = socket(AF_INET, SOCK_STREAM, 0); //socketul care leaga clientul de server
	DIE(sockfd < 0, "socket");

	//adresa si portul serverului
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(atoi(argv[3]));
	ret = inet_aton(argv[2], &serv_addr.sin_addr);
	DIE(ret == 0, "inet_aton");

	ret = connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)); ///ne conectam la server
	DIE(ret < 0, "connect");

	ret=setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(int));
	DIE(ret < 0, "setsockopt");

	n = send(sockfd, argv[1], strlen(argv[1]), 0); //trimitem id-ul serverului
	DIE(n < 0, "send");

	while (1) {
		//setul de socketi de pe care primim/trimitem mesaje (socketul de server + socketul de stdin)
		fd_set set;
		FD_ZERO(&set);
		FD_SET(STDIN_FILENO, &set); //socketul de la tastatura
		FD_SET(sockfd, &set); //socketul de la server

		memset(buffer, 0, BUFLEN); //reinitializam bufferul

		int fdmax= STDIN_FILENO > sockfd ? STDIN_FILENO : sockfd; //socketul de valoare maxima
		int rc =select(fdmax+1, &set, NULL, NULL, NULL); //asteptam date de la server/tastatura
		DIE(rc<0, "select");

		if (FD_ISSET(STDIN_FILENO, &set)) //avem date de la stdin/tastatura
		{
			n = read(0, buffer, sizeof(struct stdinmessage)); //citim comanda
			DIE(n < 0, "read");

			struct stdinmessage msj;
			memset(&msj, 0, sizeof(struct stdinmessage));

			int check= sscanf(buffer, "%s %s %d", msj.command, msj.topic, &msj.sf); //impartim bufferul in comanda, topic si sf
			DIE(check <=0, "sscanf");

			if (strncmp(msj.command, "exit", 4) == 0) { //primim comanda exit de la tastatura

				close(sockfd); //inchidem socketul
				exit(0);
			}

			if (strncmp(msj.command, "subscribe", 9) ==0) //primim comanda de subscribe
			{
				if(check != 3) //caz comanda invalida de subscribe
				{
					int wc = write(STDERR_FILENO, "Invalid command.\n", 17 );
			   	    DIE(wc < 0, "write");
					continue;
				}

				n = send(sockfd, buffer, sizeof(struct stdinmessage), 0); //trimitem comanda mai departe la server
			    DIE(n < 0, "send");

			    int wc = write(STDOUT_FILENO, "Subscribed to topic.\n", 21 ); //afisam linia de feedback
			    DIE(wc < 0, "write");
				
			}
			else if (strncmp(msj.command, "unsubscribe", 11) ==0) //comanda unsubscribe
			{
				if(check != 2) //caz comanda invalida de unsubscribe
				{
					int wc = write(STDERR_FILENO, "Invalid command.\n", 17 );
			   	    DIE(wc < 0, "write");
					continue;
				}

				n = send(sockfd, buffer, sizeof(struct stdinmessage), 0); //trimitem cele 2 campuri din comanda
			    DIE(n < 0, "send");

			    int wc = write(STDOUT_FILENO, "Unsubscribed from topic.\n", 25 ); //afisam linia de feedback
			    DIE(wc < 0, "write");
			}
			else //comanda invalida
			{
				int wc = write(STDERR_FILENO, "Invalid command.\n", 17 );
			    DIE(wc < 0, "write");
			}
		}
		else //primire mesaje de la server
		{
			int rc = recv(sockfd, buffer, sizeof(struct topicmessage), 0); //luam in buffer mesajul primit de la server
			DIE(rc < 0, "recv");

			if(rc == 0) //serverul a inchis comunicarea
			{
				break;
			}
			else //mesaj de pe un topic 
			{
				struct topicmessage *m = (struct topicmessage *)buffer;

				//afisare informatii topic
				printf("%s:%d - ", m->ip_client_udp, m->port_client_udp);
				printf("%s - ", m->topic);
				printf("%s - ", m->tip_date);
				char sign =0;

				//afisarea payload-ului un functie de tip
				if(strcmp(m->tip_date, "INT") == 0)
				{
					if(memcmp(&sign, m->payload, 1) !=0) //verificam semnul
					{
						printf("-");
					}

					printf("%u\n", htonl(*((unsigned int *)(m->payload+1))));
				}
				if(strcmp(m->tip_date, "SHORT_REAL") == 0)
				{
					printf("%.2f\n",(double)htons((*((unsigned short *)m->payload)))/(double)100);
				}
				if(strcmp(m->tip_date, "FLOAT") == 0)
				{
					if(memcmp(&sign, m->payload, 1) !=0) //verificam semnul
					{
						printf("-");
					}

					int pow;

					memcpy(&pow, m->payload+5, 1);

					unsigned int dec=1;

					for(int j=0; j<pow; j++) //calculam puterea de 10
					{
						dec=dec*10;
					}

					char retouch[50], *end; //stergem 0-urile de la finalul numarului
					memset(retouch, 0, 50);
					snprintf(retouch, 50, "%f", (double)htonl((*((unsigned int*)(m->payload+1))))/(double)dec);
					end=retouch+strlen(retouch)-1;
					while(*end=='0')
					{
						end=end-1;
					}

					if(*end=='.')
					{
						*end='\0';
					}
					else
					{
						*(end+1)='\0';
					}
					
					printf("%s\n", retouch);
				}
				if(strcmp(m->tip_date, "STRING") == 0)
				{
					printf("%s\n", m->payload);
				}
			}
		}
	}

	close(sockfd); //inchidem socketul
	return 0;
}
