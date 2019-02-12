#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fstream>
#include <vector>
#include <cstring>
#include <algorithm>


using namespace std;

#define MAX_CLIENTS	5
#define BUFLEN 256

typedef struct { 
	char nume[12];
	char prenume[12];
	int card;
	int pin;
	char parola[8];
	double sold;
} client;

void cod_eroare(int cod, char *eroare, char buffer[BUFLEN]) {
	if(cod == -2)
		sprintf(buffer, "IBANK> -2 : Sesiune deja deschisa\n");
	if(cod == -3)
		sprintf(buffer, "IBANK> -3 : Pin gresti\n");
	if(cod == -4) {
		if(strcmp(eroare, "unlock") == 0)
			sprintf(buffer, "UNLOCK> -4 : Numar card inexistent\n");
		else
			sprintf(buffer, "IBANK> -4 : Numar card inexistent\n");
	}
	if(cod == -5)
		sprintf(buffer, "IBANK> -5 : Card blocat\n");
	if(cod == -6) {
		if(strcmp(eroare, "unlock") == 0)
			sprintf(buffer, "UNLOCK> -6 : Operatie esuata\n");
		else
			sprintf(buffer, "IBANK> -6 : Operatie esuata\n");
	}
		
	if(cod == -7)
		sprintf(buffer, "UNLOCK> -7 : Deblocare esuata\n");
	if(cod == -8)
		sprintf(buffer, "IBANK> -8 : Fonduri insuficiente\n");
	if(cod == -9)
		sprintf(buffer, "IBANK> -9 : Operatie anulata\n");
	if(cod == -10) 
		sprintf(buffer, "IBANK> -10 : Eroare la apel %s\n", eroare);
}

int client_socket(int socket, vector<int> clienti_server) {
	for(int o = 0; o < clienti_server.size(); o++)
		if(clienti_server[o] == socket)
			return o; 
	return -1;
}

int client_card(int card, vector<client> clienti) {
	for(int o = 0; o < clienti.size(); o++)
		if(clienti[o].card == card)
			return o; 
	return -1;
}


int main(int argc, char *argv[])
{
     int sockfd, newsockfd, portno, fd, card, pin, unlock;
	 double suma;
	 bool transfer = false;
	 socklen_t clilen;
     char buffer[BUFLEN], comanda[9];
     struct sockaddr_in serv_addr, cli_addr, from_client;
	 FILE *file;
	 vector<client> clienti;
	 vector<int> sesiuni;
	 vector<int> clienti_server;
	 int client_server;
	 vector<bool> blocati;
	 vector<vector<int> > incercari;
	 vector<pair<int, double> > transferuri;
	 vector<int> deblocari;
	 client c;
     int m, n, o, i, j;

     fd_set read_fds;	//multimea de citire folosita in select()
     fd_set tmp_fds;	//multime folosita temporar 
     int fdmax;		//valoare maxima file descriptor din multimea read_fds

     if (argc < 3) {
		 cod_eroare(-10, argv[0], buffer);
		 puts(buffer);
         exit(1);
     }

     //golim multimea de descriptori de citire (read_fds) si multimea tmp_fds 
     FD_ZERO(&read_fds);
     FD_ZERO(&tmp_fds);
     
	 fd = socket(AF_INET, SOCK_DGRAM, 0);
     sockfd = socket(AF_INET, SOCK_STREAM, 0);
     if (sockfd < 0 || fd < 0) {
        cod_eroare(-10, "socket", buffer);
		puts(buffer);
     }
     portno = atoi(argv[1]);

     memset((char *) &serv_addr, 0, sizeof(serv_addr));
     serv_addr.sin_family = AF_INET;
     serv_addr.sin_addr.s_addr = INADDR_ANY;	// foloseste adresa IP a masinii
     serv_addr.sin_port = htons(portno);
     
     if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr)) < 0 || bind(fd, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr)) < 0) {
             cod_eroare(-10, "bind", buffer);
			 puts(buffer);
	 }
 
     listen(sockfd, MAX_CLIENTS);

     //adaugam noul file descriptor (socketul pe care se asculta conexiuni) in multimea read_fds
     FD_SET(sockfd, &read_fds);
	 FD_SET(fd, &read_fds);
	 FD_SET(0, &read_fds);
     fdmax = max(sockfd, fd);
	
	 file = fopen(argv[2], "r");
	 fscanf(file, "%d", &m);
	 for(o = 0; o < m; o++) {
		memset(&c, 0, sizeof(c));
		fscanf(file, "%s %s %d %d %s %lf", c.nume, c.prenume, &c.card, &c.pin, c.parola, &c.sold);
		clienti.push_back(c);
	}
	fclose(file);
	sesiuni.resize(m, -1);
	blocati.resize(m, false);
	deblocari.resize(m, -1);
    // main loop
	while (1) {
		tmp_fds = read_fds; 
		if (select(fdmax + 1, &tmp_fds, NULL, NULL, NULL) == -1) {
			cod_eroare(-10, "select", buffer);
			puts(buffer);
		}
		for(i = 0; i <= fdmax; i++) {
			if (FD_ISSET(i, &tmp_fds)) {
				if (i == sockfd) {
					// a venit ceva pe socketul inactiv(cel cu listen) = o noua conexiune
					// actiunea serverului: accept()
					clilen = sizeof(cli_addr);
					if ((newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen)) == -1) {
						cod_eroare(-10, "accept", buffer);
						send(i, buffer, strlen(buffer), 0);
						puts(buffer);
					} 
					else {
						//adaug noul socket intors de accept() la multimea descriptorilor de citire
						clienti_server.push_back(newsockfd);
						incercari.push_back(vector<int>(m,0));
						transferuri.push_back(make_pair(-1, 0));
						FD_SET(newsockfd, &read_fds);
						if (newsockfd > fdmax) { 
							fdmax = newsockfd;
						}
					}
					printf("Noua conexiune de la %s, port %d, socket_client %d\n", inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port), newsockfd);
				} else if(i == 0) {
					memset(buffer, 0, BUFLEN);
    				fgets(buffer, BUFLEN, stdin);
					memset(comanda, 0, sizeof(comanda));
					sscanf(buffer, "%s\n", comanda);
					if(strcmp(comanda, "quit") == 0) {
						for(int o = 0; o < clienti_server.size(); o++) {
							send(clienti_server[o], comanda, strlen(comanda), 0);
							close(clienti_server[o]);	
						}
						close(sockfd);
						exit(0);
					}
					else {
						cod_eroare(-10, comanda, buffer);
						puts(buffer);
					}
				} else if(i == fd) {
					// am primit date pe socketul pentru UDP cu care vorbesc cu clientii
					// actiunea serverului: recvfrom()
					memset(buffer, 0, BUFLEN);
					socklen_t clilen = sizeof(from_client);
					if((n = recvfrom(i, buffer, sizeof(buffer), 0, (struct sockaddr *) &(from_client), &clilen)) <= 0) { //conexiunea s-a inchis
						cod_eroare(-10, "recvfrom", buffer);
						puts(buffer);
						close(i); 
						FD_CLR(i, &read_fds); // scoatem din multimea de citire socketul pe care 
					}
					else { //recvfrom intoarce >0
					  memset(comanda, 0, sizeof(comanda));
					  sscanf(buffer, "%s %d", comanda, &card);
    				  if(strcmp(comanda, "unlock") == 0) {
							memset(buffer, 0, BUFLEN);
							o = client_card(card, clienti);
							if(o == -1)
								cod_eroare(-4, "unlock", buffer);
							else if(!blocati[o])
								cod_eroare(-6, "unlock", buffer);
							else {
								unlock++;
								sprintf(buffer, "UNLOCK> Trimite parola secreta\n");
							}
							sendto(i, buffer, strlen(buffer), 0, (struct sockaddr *) &(from_client), clilen);	
						} else if(unlock > 0) {
							unlock--;
							memset(buffer, 0, BUFLEN);
							o = client_card(card, clienti);
							if(strcmp(comanda, clienti[o].parola) == 0) {
								blocati[o] = false;
								for(int j = 0; j < incercari.size(); j++)
									incercari[j][o] = 0;
								sprintf(buffer, "UNLOCK> Card deblocat\n");
							} 
							else 
								cod_eroare(-7, "", buffer);
							sendto(i, buffer, strlen(buffer), 0, (struct sockaddr *) &(from_client), clilen);	
						}
					}
				} else {
					// am primit date pe unul din socketii pentru TCP cu care vorbesc cu clientii
					// actiunea serverului: recv()
					memset(buffer, 0, BUFLEN);
					if ((n = recv(i, buffer, sizeof(buffer), 0)) <= 0) { //conexiunea s-a inchis
						for(int o = 0; o < m; o++)
							if(sesiuni[o] == i)
								sesiuni[o] = -1;
						client_server = client_socket(i, clienti_server);
						incercari.erase(incercari.begin() + client_server);
					    transferuri.erase(transferuri.begin() + client_server);
						clienti_server.erase(find(clienti_server.begin(), clienti_server.end(), i));
						if(strcmp(comanda, "quit") != 0) {
							cod_eroare(-10, "recv", buffer);
							puts(buffer);
						}
						close(i); 
						FD_CLR(i, &read_fds); // scoatem din multimea de citire socketul pe care 
					} 
					else { //recv intoarce >0
						memset(comanda, 0, sizeof(comanda));
						sscanf(buffer, "%s", comanda);
						if(strcmp(comanda, "login") == 0) {
							sscanf(buffer, "%*s %d %d", &card, &pin);
							memset(buffer, 0, BUFLEN);
							o = client_card(card, clienti);
							if(o == -1) {
								cod_eroare(-4, "", buffer);
								send(i, buffer, strlen(buffer), 0);
							}
							else {
								client_server = client_socket(i, clienti_server);
								if(sesiuni[o] == -1) {
									if((clienti[o].pin == pin) && (!blocati[o])) {
										sesiuni[o] = i;	
										incercari[client_server][o] = 0;
										sprintf(buffer, "IBANK> Welcome %s %s\n", clienti[o].nume, clienti[o].prenume);
										send(i, buffer, strlen(buffer), 0);										
									}
									else {
										if((incercari[client_server][o] < 2) && (!blocati[o])) {
											incercari[client_server][o] += 1; 												
											cod_eroare(-3, "", buffer);
											send(i, buffer, strlen(buffer), 0);
										}
										else {
											if(!blocati[o])
												blocati[o] = true;
											cod_eroare(-5, "", buffer);
											send(i, buffer, strlen(buffer), 0);
										}
									}
								}
								else {
									cod_eroare(-2, "", buffer);
									send(i, buffer, strlen(buffer), 0);
								}
							}
						} else if(strcmp(comanda, "logout") == 0) {
							for(int o = 0; o < m; o++)
								if(sesiuni[o] == i)
									sesiuni[o] = -1;
							memset(buffer, 0, BUFLEN);
							sprintf(buffer, "IBANK> Clientul a fost deconectat\n");
							send(i, buffer, strlen(buffer), 0);	
						} else if(strcmp(comanda, "listsold") == 0) {
							memset(buffer, 0, BUFLEN);
							for(int o = 0; o < m; o++)
								if(sesiuni[o] == i)
									sprintf(buffer, "IBANK> %.2lf\n", clienti[o].sold);
							send(i, buffer, strlen(buffer), 0);	
						} else if(strcmp(comanda, "transfer") == 0) {
							sscanf(buffer, "%*s %d %lf", &card, &suma);
							memset(buffer, 0, BUFLEN);
							j = client_card(card, clienti);
							if(j == -1) 
								cod_eroare(-4, "", buffer);
							else {
								if(suma > 0) { 
								for(int o = 0; o < m; o++)
									if(sesiuni[o] == i) {
										if(clienti[o].sold < suma)
											cod_eroare(-8, "", buffer);	
										else {
											transfer = true;
											client_server = client_socket(i, clienti_server);
											transferuri[client_server] = make_pair(j, suma);
											sprintf(buffer, "IBANK> Transfer %.2lf catre %s %s? [ y/n ]\n", suma, clienti[j].nume, clienti[j].prenume);
										}
									}
								}
								else
									cod_eroare(-6, "", buffer);	
							}
							send(i, buffer, strlen(buffer), 0);	
						} else if(transfer) {
							transfer = false;
							client_server = client_socket(i, clienti_server);
							for(int o = 0; o < transferuri.size(); o++)
								if((o != client_server) && (transferuri[o].first != -1) && (transferuri[o].second != 0))
									transfer = true;
							memset(buffer, 0, BUFLEN);
							if(comanda[0] == 'y') {
								for(int o = 0; o < m; o++)
									if(sesiuni[o] == i) {
										j = transferuri[client_server].first;
										suma = transferuri[client_server].second; 
										clienti[o].sold -= suma;
										clienti[j].sold += suma;
										sprintf(buffer, "IBANK> Transfer realizat cu succes\n");
									}
							}
							else 
								cod_eroare(-9, "", buffer);			
							transferuri[client_server] = make_pair(-1, 0);
							send(i, buffer, strlen(buffer), 0);	
						} else if(strcmp(comanda, "") != 0) {
							memset(buffer, 0, BUFLEN);
							cod_eroare(-6, "", buffer);	
							send(i, buffer, strlen(buffer), 0);	
						}
					}
				}
			}
		fflush(stdout);
		}
     }
     close(sockfd);
   
     return 0; 
}


