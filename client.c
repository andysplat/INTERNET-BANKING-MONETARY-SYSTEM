#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <arpa/inet.h>
#include <unistd.h>

#define BUFLEN 256

void cod_eroare(int cod, char *eroare, char buffer[BUFLEN]) {
	if(cod == -1)
		sprintf(buffer, "-1 : Clientul nu este autentificat\n");
	if(cod == -2)
		sprintf(buffer, "-2 : Sesiune deja deschisa\n");
	if(cod == -10) 
		sprintf(buffer, "-10 : Eroare la apel %s\n", eroare);
}

int main(int argc, char *argv[])
{
    int sockfd, n, portno, fd, id, logat = 0, card = -1, unlock = 0;
	char buffer[BUFLEN], comanda[9], fisier[20];
    struct sockaddr_in serv_addr, from_server;
    FILE *file;

    fd_set read_fds;	//multimea de citire folosita in select()
    fd_set tmp_fds;	//multime folosita temporar 
    int fdmax;	//valoare maxima file descriptor din multimea read_fds
    
    id = getpid();
    sprintf(fisier, "client-%d.log", id);

    //golim multimea de descriptori de citire (read_fds) si multimea tmp_fds
    FD_ZERO(&read_fds);
    FD_ZERO(&tmp_fds);

	if (argc < 3) {
		cod_eroare(-10, argv[0], buffer);
		puts(buffer);
		exit(1);
    }  
    fd = socket(AF_INET, SOCK_DGRAM, 0);
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0 || fd < 0) {
        cod_eroare(-10, "socket", buffer);
		puts(buffer);
    }
	portno = atoi(argv[2]);
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(portno);
    inet_aton(argv[1], &serv_addr.sin_addr);
    
    if (connect(sockfd,(struct sockaddr*) &serv_addr,sizeof(serv_addr)) < 0) {
        cod_eroare(-10, "connect", buffer);
		puts(buffer);
    }    
    
    FD_SET(sockfd, &read_fds);
	FD_SET(fd, &read_fds);
    FD_SET(0, &read_fds);
    if(sockfd > fd)
		fdmax = sockfd;
	else
		fdmax = fd;

	while (1) {
		tmp_fds = read_fds; 
		if (select(fdmax + 1, &tmp_fds, NULL, NULL, NULL) == -1) {
			cod_eroare(-10, "select", buffer);
			puts(buffer);
		}
		for(int i = 0; i <= fdmax; i++) {
			if (FD_ISSET(i, &tmp_fds)) {
				if (i != 0 && i!= fd) {
					// am primit date pe unul din socketi pentru TCP
					// actiunea clinetului: recv()
					memset(buffer, 0, BUFLEN);
					if ((n = recv(i, buffer, sizeof(buffer), 0)) <= 0) { //conexiunea s-a inchis
						cod_eroare(-10, "recv", buffer);
						puts(buffer);
						close(i); 
						FD_CLR(i, &read_fds); // scoatem din multimea de citire socketul pe care 
					} 
					else { //recv intoarce >0
 						if(strcmp(buffer, "quit") == 0) {
							 close(sockfd);
							 exit(0);
						} 
						else {
							file = fopen(fisier, "a+");
							fprintf(file, "%s", buffer);
							fclose(file); 
							if(strcmp(buffer, "IBANK> Clientul a fost deconectat\n") == 0)
								logat = 0;
							else {
								memset(comanda, 0, sizeof(comanda));
								sscanf(buffer, "%*s %s", comanda);
								if(strcmp(comanda, "Welcome") == 0) 
									logat = 1;
							}
			      		}
						puts(buffer);
					}
				} else if(i == fd) { 
					// am primit date pe socketul pentru UDP
					// actiunea clinetului: recvfrom()
					socklen_t clilen = sizeof(from_server);
					if((n = recvfrom(i, buffer, sizeof(buffer), 0, (struct sockaddr *) &(from_server), &clilen)) <= 0) { //conexiunea s-a inchis
						cod_eroare(-10, "recvfrom", buffer);
						puts(buffer);
						close(i); 
						FD_CLR(i, &read_fds); // scoatem din multimea de citire socketul pe care 
					}
					else { //recvfrom intoarce >0	
						if(strcmp(buffer, "UNLOCK> Trimite parola secreta\n") == 0)
							unlock = 1;
						file = fopen(fisier, "a+");
						fprintf(file, "%s", buffer);
						fclose(file);
						puts(buffer);
					} 
				}
				else {	 
  					//citesc de la tastatura
    				memset(buffer, 0, BUFLEN);
    				fgets(buffer, BUFLEN, stdin);
					memset(comanda, 0, sizeof(comanda));
					sscanf(buffer, "%s", comanda);	
					if(strcmp(comanda, "login") == 0)
						sscanf(buffer, "%*s %d", &card);
					if((strcmp(comanda, "login") == 0) && logat) {
						memset(buffer, 0, BUFLEN);	
						cod_eroare(-2, "", buffer);					
						file = fopen(fisier, "a+");
						fprintf(file, "%s", buffer);  
						fclose(file);
					} else if((strcmp(comanda, "login") != 0) && (strcmp(comanda, "quit") != 0) && (strcmp(comanda, "unlock") != 0) && (unlock == 0) && (!logat)) {
						if(strcmp(comanda, "") != 0) {
							memset(buffer, 0, BUFLEN);
							cod_eroare(-1, "", buffer);	
						}				
							file = fopen(fisier, "a+");
							fprintf(file, "%s", buffer);  
							fclose(file);
					} else if((strcmp(comanda, "unlock") == 0) || (unlock == 1)) {	
						if(unlock == 1)
							unlock = 0;
						memset(buffer, 0, BUFLEN);
						sprintf(buffer, "%s %d\n", comanda, card);
						file = fopen(fisier, "a+");
						fprintf(file, "%s\n", comanda);  
						fclose(file);
						//trimit mesaj la server UDP
						n = sendto(fd, buffer, strlen(buffer), 0, (struct sockaddr*) &(serv_addr), sizeof(serv_addr)); 
    					if (n < 0) {
     						cod_eroare(-10, "sendto", buffer);
							puts(buffer);
						}
					} else {
						file = fopen(fisier, "a+");
						fprintf(file, "%s", buffer);  
						fclose(file);
    					//trimit mesaj la server TCP
						n = send(sockfd, buffer, strlen(buffer), 0); 
    					if (n < 0) {
     						cod_eroare(-10, "send", buffer);
							puts(buffer);
						}
						if(strcmp(comanda, "quit") == 0) {
							close(sockfd);
							exit(0);
						}
					}
				}
			}
		fflush(stdout);
		}
	} 
    return 0;
}


