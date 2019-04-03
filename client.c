/************************************************
*												*
* 	MESSAGERIE INSTANTANEE - CLIENT - V1.1		*
*												*
*	AUTEURS : CABARET Emma - LAURANCY Dorian	*
*												*
************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>

int socketClient; /* Socket (global pour la fonction "fermer")*/
struct sockaddr_in adServ;

int connexion(char *ip, int port) {
	/* -- Donnee preliminaire : Taille de la socket -- */
	socklen_t lgA = sizeof(struct sockaddr_in);


	/* -- Debut -- */
	/* Creation de la socket IPV4 / TCP*/
	socketClient = socket(PF_INET, SOCK_STREAM, 0);

	/* Donnees sur le serveur */
	
	adServ.sin_family = AF_INET; /* IPV4 */
	adServ.sin_port = htons(port);	/* Port */
	inet_pton(AF_INET, ip, &(adServ.sin_addr)); /* Adresse IP */


	/* Connexion au serveur */
	int res = connect(socketClient, (struct sockaddr *) &adServ, lgA) ;
	/* Message de confirmation de connexion */
	if (res < 0) {
		return -1;
	} else {
		return 0;
	}

}

int attente() {
	/*Début */
	/*Attente du message du serv eur pour l'ordre */
	char str[10];
	ssize_t reception = recv (socketClient, str, 256, 0);

	if (reception < 0) {
		return -1;
	}
	else if (strcmp(str, "NUM1") == 0) {
		return 0;
	}
	else if (strcmp(str, "NUM2") == 0) {
		return 1;
	}
	else {
		return -1;
	}

}

/* Ferme le socket et quitte le programme */
void fermer() {
	printf("\nINTERRUPTION DU PROGRAMME (CTRL+C)\n");
	int res = close(socketClient);
	if (res == 0) {
		printf("\nFermeture du socket\n");
	} else {
		perror("\nErreur fermeture du socket\n");
	}
	exit(0);
}

int main (int argc, char *argv[]) {
	/* Ferme proprement le socket si CTRL+C est execute */
	signal(SIGINT, fermer);

	return 0;
}

