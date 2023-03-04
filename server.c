#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include "helpers.h"

struct topicmessage //structura mesaj trimis catre client TCP
{
	char ip_client_udp[16];
	unsigned short port_client_udp;
	char topic[51];
	char tip_date[20];
	char payload[1500];
};

struct node //nod pentru coada de mesaje
{
	struct topicmessage message;
	struct node *next;
};

struct queue //coada
{
	struct node *head, *tail;
};

void init(struct queue *coada) //initializare coada
{
	coada->tail=NULL;
	coada->head=NULL;
}

void freequeue(struct queue *coada) //eliberare memorie coada
{
	struct node *flag, *flag1;

	flag=coada->head;
	flag1=flag;

	while(flag!=NULL)
	{
		flag1=flag->next;
		free(flag);
		flag=flag1;
	}
}

void addqueue(struct queue *coada, struct topicmessage *m) //adaugam un mesaj in coada
{
	struct node *nou, *flag;

	nou=malloc(sizeof(struct node));
	memset(&nou->message, 0, sizeof(struct topicmessage));
	flag=coada->tail;

	memcpy(nou->message.ip_client_udp, m->ip_client_udp, strlen(m->ip_client_udp));
	nou->message.port_client_udp=m->port_client_udp;
	memcpy(nou->message.topic, m->topic, strlen(m->topic));
	memcpy(nou->message.tip_date, m->tip_date, strlen(m->tip_date));

	if(strcmp(nou->message.tip_date, "INT") == 0)
	{
		memcpy(nou->message.payload, m->payload, 5);
	}
	if(strcmp(nou->message.tip_date, "SHORT_REAL") == 0)
	{
		memcpy(nou->message.payload, m->payload, 2);
	}
	if(strcmp(nou->message.tip_date, "FLOAT") == 0)
	{
		memcpy(nou->message.payload, m->payload, 6);
	}
	if(strcmp(nou->message.tip_date, "STRING") == 0)
	{
		memcpy(nou->message.payload, m->payload, strlen(m->payload)+1);
	}

	nou->next=NULL;

	if(flag ==NULL)
	{
		coada->head=nou;
		coada->tail=nou;
		return;
	}

	flag->next=nou;
	coada->tail=nou;
}

void popqueue(struct queue *coada, struct topicmessage *m) //scoatem in *m mesajul din coada
{
	struct node *flag, *flag1;
	struct topicmessage *mnode;
	flag=coada->head;
	mnode=&(flag->message);

	memcpy(m->ip_client_udp, mnode->ip_client_udp, strlen(mnode->ip_client_udp));
	m->port_client_udp=mnode->port_client_udp;
	memcpy(m->topic, mnode->topic, strlen(mnode->topic));
	memcpy(m->tip_date, mnode->tip_date, strlen(mnode->tip_date));

	if(strcmp(m->tip_date, "INT") == 0)
	{
		memcpy(m->payload, mnode->payload, 5);
	}
	if(strcmp(m->tip_date, "SHORT_REAL") == 0)
	{
		memcpy(m->payload, mnode->payload, 2);
	}
	if(strcmp(m->tip_date, "FLOAT") == 0)
	{
		memcpy(m->payload, mnode->payload, 6);
	}
	if(strcmp(m->tip_date, "STRING") == 0)
	{
		memcpy(m->payload, mnode->payload, strlen(mnode->payload)+1);
	}

	flag1=flag->next;
	free(flag);
	coada->head=flag1;
}

struct stdinmessage //structura comanda primita de la un client TCP
{
	char command[12];
	char topic[51];
	int sf;
};

struct udpmessage //structura mesaj primit de la un client UDP
{
	char topic[51];
	char type_data;
	char payload[1500];
};

struct client_info //structura in care avem informatiile despre un client TCP
{
	int socket;
	char id[11];
	char *topics[100];
	int nrtopics;
	int topicsf[100];
	int state;
	struct queue coada;
};

void usage(char *file) //eroare in caz de argumente invalide
{
	fprintf(stderr, "Usage: %s server_port\n", file);
	exit(0);
}

void freetopics (char **topic, int nrtopics) //eliberare memorie topics
{
	for(int i=0; i<nrtopics; i++)
	{
		free(topic[i]);
	}
}

int main(int argc, char *argv[])
{
	int sockfd, newsockfd, portno, udp_sockfd;
	char buffer[BUFLEN], idclient[11];
	struct sockaddr_in serv_addr, cli_addr;
	int n, i, ret, enable=1, check, wc;
	socklen_t clilen;
	struct client_info *clients;
	int CLMAX=1000, times=50;
	int nrclientitcp=0;

	clients=malloc(sizeof(struct client_info)*CLMAX); //vectorul de clienti TCP (initial alocat pentru 1000 clienti)

	fd_set read_fds;	// setul de socketi conectati la server
	fd_set tmp_fds;		// setul de socketi folosit pentru select()
	int fdmax;			// socketul de valoare maxima

	if (argc < 2) { //caz nr argmumente invalid
		usage(argv[0]);
	}

	setvbuf(stdout, NULL, _IONBF, BUFSIZ); //dezactivare buffering la afisare
	fflush(stdout);

	//golim cele 2 seturi de socketi
	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	udp_sockfd = socket(AF_INET, SOCK_DGRAM, 0); //creare socket pentru UDP clients
	DIE(udp_sockfd < 0, "socket");

	sockfd = socket(AF_INET, SOCK_STREAM, 0); // creare socket de listen pentru TCP clients
	DIE(sockfd < 0, "socket");

	portno = atoi(argv[1]); //portul de pe care deschidem socketii
	DIE(portno == 0, "atoi");//verificam daca portul este valid

	//setam socketii de listen pentru TCP si UDP ca sa fie reutilizabili + dezactivare algoritm Nagle
	ret=setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
	DIE(ret < 0, "setsockopt");
	ret=setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(int));
	DIE(ret < 0, "setsockopt");
	ret=setsockopt(udp_sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
	DIE(ret < 0, "setsockopt");

	//completam adresa+port server
	memset((char *) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(portno);
	serv_addr.sin_addr.s_addr = INADDR_ANY;

	ret = bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr)); //bind socketul de TCP
	DIE(ret < 0, "bind");

	ret = bind(udp_sockfd, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr)); //bind socketul de UDP
	DIE(ret < 0, "bind");

	ret = listen(sockfd, MAX_CLIENTS); //sockfd - socketul de conectari/listen pentru clientii TCP
	DIE(ret < 0, "listen");

	//adaugam cei 2 socketi pentru TCP/UDP + file descriptorul pentru stdin/tastatura
	FD_SET(sockfd, &read_fds);
	FD_SET(udp_sockfd, &read_fds);
	FD_SET(STDIN_FILENO, &read_fds); //socketul de la tastatura

	//luam socketul cu valoarea ce mai mare ca sa putem sa identificam socketii
	if(sockfd > udp_sockfd)
	{
		fdmax = sockfd;
	}
	else
	{
		fdmax=udp_sockfd;
	}
	if(STDIN_FILENO > fdmax)
	{
		fdmax=STDIN_FILENO;
	}

firstwhile:
	while (1) {
		tmp_fds = read_fds; 

		memset(buffer, 0, BUFLEN); //reinitializam bufferul
		
		ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL); //asteptam mesaje de la clienti/tastatura
		DIE(ret < 0, "select");

		//in setul tmp_fds avem un socket de la care am primit un mesaj
		for (i = 0; i <= fdmax; i++) {
			if (FD_ISSET(i, &tmp_fds)) {
				if (i == sockfd) { //cerere conexiune de la un client TCP

					clilen = sizeof(cli_addr);
					newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen); //acceptam conexiunea pe socketul newsockfd
					DIE(newsockfd < 0, "accept");

					ret=setsockopt(newsockfd, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(int));
					DIE(ret < 0, "setsockopt");

					// adaugam socketul clientului nou in multimea de socketi
					FD_SET(newsockfd, &read_fds);
					if (newsockfd > fdmax) { 
						fdmax = newsockfd;
					}

					//primit de la client un mesaj ce contine id-ul acestuia
					memset(idclient, 0, 11);
					n = recv(newsockfd, idclient, 11, 0);
					DIE(n<0, "id failed");
					idclient[n]='\0';

					//luam fiecare client care e conectat/s-a conectat la un moment dat la server
					for(int j=0; j<nrclientitcp; j++)
					{
						if(strncmp(clients[j].id, idclient, strlen(idclient))==0) //mai exista un client cu acelasi ID
						{
							if (clients[j].state == 1) //clientul cu acelasi ID este conectat 
							{
								printf("Client %s already connected.\n", idclient);
								close(newsockfd); //inchidem socketul clientului
								FD_CLR(newsockfd, &read_fds); //il scoatem din setul de socketi
								goto firstwhile;
							}
							else //clientul nu este conectat
							{
								printf("New client %s connected from %s:%d.\n", idclient, inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));

								clients[j].socket= newsockfd; //reinitializam socketul clientului
								clients[j].state=1;

								struct topicmessage mtosend;

								while(clients[j].coada.head !=NULL) //trimitem eventualele mesaje din coada de SF
								{
									memset(&mtosend, 0, sizeof(struct topicmessage));
									popqueue(&clients[j].coada, &mtosend);
									n = send(clients[j].socket, &mtosend, sizeof(struct topicmessage), 0);
			   						DIE(n < 0, "send");
							    }
								init(&clients[j].coada);
								goto firstwhile;
							}
						}
					}

					printf("New client %s connected from %s:%d.\n", idclient, inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));

					//cazul in care nu mai avem loc in vectorul de clienti
					if(nrclientitcp+1 >CLMAX)
					{
						clients=realloc(clients, CLMAX+times);
						CLMAX=CLMAX+times;
					}

					//initializam campurile pentru noul client conectat
					clients[nrclientitcp].socket= newsockfd; //socket
					memcpy(clients[nrclientitcp].id, idclient, n+1); //id
					clients[nrclientitcp].nrtopics=0; //nr topicuri la care este abonat clientul
					clients[nrclientitcp].state=1; //starea clientului (0-deconectat  1-conectat)
					init(&clients[nrclientitcp].coada); //coada SF pentru mesaje

					nrclientitcp++;

				} else if (i == udp_sockfd) { //avem un mesaj de la un client udp

					//adresa+port client UDP
					struct sockaddr_in udp_cli_addr;
					unsigned int udp_clilen;

					//luam mesajul de la clientul UDPS
					udp_clilen=sizeof(udp_cli_addr);
					n= recvfrom(udp_sockfd, buffer, BUFLEN, 0, (struct sockaddr *) &udp_cli_addr, &udp_clilen);
					DIE(n<0, "recvfrom");

					struct topicmessage mtosend; //mesajul de trimis catre clientii TCP abonati la topicul respectiv
					struct udpmessage mreceived; //mesajul primit de la clientul UDP
					int maxtopic=1;

					memset(&mtosend, 0, sizeof(struct topicmessage));
					memset(&mreceived, 0, sizeof(struct udpmessage));

					//luam primul camp de topic din buffer si adaugam '\0' in caz de 50 char in topic
					memcpy(mreceived.topic, buffer, 50);
					for(int j=0; j<50; j++)
					{
						if(mreceived.topic[j] == '\0')
						{
							maxtopic=0 ;
							break;
						}
					}

					if (maxtopic == 1)
					{
						mreceived.topic[50]='\0';
					}

					//copiem topicul in mesajul de trimis
					memcpy(mtosend.topic, mreceived.topic, strlen(mreceived.topic)+1);

					//copiem adresa ip si portul clientului UDP in mesajul de trimis
					int sizeip= strlen(inet_ntoa(udp_cli_addr.sin_addr));
					memset(mtosend.ip_client_udp, 0, 16);
					memcpy(mtosend.ip_client_udp, inet_ntoa(udp_cli_addr.sin_addr), sizeip); //ip
					mtosend.port_client_udp=ntohs(udp_cli_addr.sin_port); //port

					//tipul datelor din payload
					memcpy(&mreceived.type_data, buffer+50, 1);
					
					switch(mreceived.type_data)
					{
						case 0:
							memcpy(mtosend.tip_date, "INT\0", 4);
							memcpy(mtosend.payload, buffer+51, 5); //1 octet de semn + 4 octeti numarul
							break;
						case 1:
							memcpy(mtosend.tip_date, "SHORT_REAL\0", 11);
							memcpy(mtosend.payload, buffer+51, 2); //2 octeti numarul
							break;
						case 2:
							memcpy(mtosend.tip_date, "FLOAT\0", 6);
							memcpy(mtosend.payload, buffer+51, 6); //1 octet semn + 4 octeti nr + 1 octet puterea lui 10
							break;
						case 3:
							memcpy(mtosend.tip_date, "STRING\0", 7);
							memcpy(mreceived.payload, buffer+51, n-51); // n-51 octeti string
							int lenpayload= n-51;
							if(lenpayload == 1500) //caz payload maxim
							{
								memcpy(mtosend.payload, mreceived.payload, lenpayload);
							}
							else
							{
								mreceived.payload[lenpayload]='\0'; //adaugam '\0' la finalul stringului
								memcpy(mtosend.payload, mreceived.payload, lenpayload+1);
							}
							
							break;

						default: //caz tip_date invalid
							wc = write(STDERR_FILENO, "Invalid command.\n", 17 );
			   		   	    DIE(wc < 0, "write");
							goto firstwhile;
					}

					for(int k = 0; k<nrclientitcp; k++) //luam toti clientii
					{
						struct client_info *curent;
						curent= &clients[k];

						for(int t =0; t < curent->nrtopics; t++) //fiecare topic la care este abonat clientul
						{
							if (strcmp(curent->topics[t], mtosend.topic) == 0) //client abonat la topicul mesajului de la clientul UDP
							{
								if (curent->state ==1) //client conectat => trimitem mesajul
								{
									n = send(curent->socket, &mtosend, sizeof(struct topicmessage), 0);
			   						DIE(n < 0, "send");
									break;
								}
								else //client deconectat
								{
									if (curent->topicsf[t] == 1) //SF activ => punem in coada clientului mesajul
									{
										addqueue(&curent->coada, &mtosend);
									}

									break;
								}
							}
						}
					}

				} else if (i == STDIN_FILENO) { //comanda de la tastatura

					n = read(0, buffer, 10);
					DIE(n < 0, "read");

					if (strncmp(buffer, "exit", 4)==0) //primim comanda de exit
					{
						for(int j=0; j<nrclientitcp; j++)
						{
							if(clients[j].state == 1)
							{
								close(clients[j].socket); //inchidem toti socketii clientilor
								FD_CLR(clients[j].socket, &read_fds);
							}
						}

						//free la topics + coada pt fiecare client
						for(int j=0; j<nrclientitcp; j++)
						{
							freetopics(clients[j].topics, clients[j].nrtopics);
							freequeue(&clients[j].coada);
						}

						free(clients);
						close(sockfd); //inchidem socketul de listen pt clientii TCP
						close(udp_sockfd); //inchidem socketul pt mesajele UDP
						exit(0);
					}
					else //comanda invalida
					{
						wc = write(STDERR_FILENO, "Invalid command.\n", 17 );
			   		    DIE(wc < 0, "write");
					}
				} else { // primim mesaje de la un client TCP

					n = recv(i, buffer, sizeof(struct stdinmessage), 0);
					DIE(n < 0, "recv");

					if (n == 0) { //clientul a inchis conexiunea

						for(int j=0; j<nrclientitcp; j++)
						{
							if (clients[j].socket == i)
							{
								clients[j].state=0; //deconectat
								printf("Client %s disconnected.\n", clients[j].id);
							}
						}

						close(i); //inchidem socketul clientului
						FD_CLR(i, &read_fds); //il scoatem din setul de socketi

					} else { //mesaj de subscribe/unsubscribe de la client tcp

						if (strncmp(((struct stdinmessage *)buffer)->command, "subscribe", 9) ==0) //primim comanda de subscribe
						{
							struct client_info *cl;
							struct stdinmessage m;

							memset(&m, 0, sizeof(struct stdinmessage));

							//luam cele 3 campuri din comanda
							check= sscanf(buffer, "%s %s %d", m.command, m.topic, &m.sf);
							DIE(check<3, "sscanf");

							for(int j=0; j< nrclientitcp; j++)
							{
								if(clients[j].socket == i)
								{
									cl= &clients[j];
									break;
								}
							}

							//adaugam topicul+sf in vectorul de topics al clientului
							cl->topics[cl->nrtopics]=malloc(sizeof(char)* (strlen(m.topic)+1)); 
							memcpy(cl->topics[cl->nrtopics], m.topic, strlen(m.topic)+1);
							cl->topicsf[cl->nrtopics]=m.sf;
							cl->nrtopics++;
						}
						else if (strncmp(((struct stdinmessage *)buffer)->command, "unsubscribe", 11) ==0) //comanda unsubscribe
						{
							struct client_info *cl;
							struct stdinmessage m;

							memset(&m, 0, sizeof(struct stdinmessage));

							//luam cele 2 campuri din comanda
							check= sscanf(buffer, "%s %s", m.command, m.topic);
							DIE(check<2, "sscanf");

							for(int j=0; j< nrclientitcp; j++)
							{
								if(clients[j].socket == i)
								{
									cl= &clients[j];
									break;
								}
							}

							for(int j=0; j<cl->nrtopics; j++)
							{
								if (strncmp(m.topic, cl->topics[j], strlen(m.topic)) == 0)
								{
									//scoatem topicul din vectorul de topics
									cl->topics[j]=realloc(cl->topics[j], sizeof(char));
									(cl->topics[j])[0]='\0';
									break;
								}
							}
						}
					}
				}
			}
		}
	}

	//free la clienti topics
	for(int i=0; i<nrclientitcp; i++)
	{
		freetopics(clients[i].topics, clients[i].nrtopics);
		freequeue(&clients[i].coada);
	}
	free(clients);
	close(sockfd);
	close(udp_sockfd);

	return 0;
}
