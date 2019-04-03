/************************************************
*												*
* 	MESSAGERIE INSTANTANEE - SERVEUR - V1.1		*
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

/* Definition de constantes */
#define NB_CLIENTS 2

/* Socket (globaux pour la fonction "fermer") */
int socketServeur; 
int socketClients[NB_CLIENTS];

struct sockaddr_in adServ;

/* Ferme les sockets et quitte le programme */
void fermer() {
	int i;

	/* Ferme tous les sockets clients */
	for (i = 0; i < NB_CLIENTS; i++) {
		int res = close (socketClients[i]);
		if (res == 0) {
			printf("\nFermeture du socket client numero %d\n", i);
		} else {
			printf("\nErreur fermeture du socket client numero %d\n", i);
			perror(NULL);
		}
	}

	/* Ferme le socket serveur */
	int res = close(socketServeur);
	if (res == 0) {
		printf("\nFermeture du socket serveur\n");
	} else {
		perror("\nErreur fermeture  du socket serveur\n");
	}
	exit(0);
}

/*
*	Initialise le serveur : prepare le socket, bind et listen
*	param : int port 	Le port sur lequel le socket doit ecouter
*	Return :
*		-1  :  Erreur lors du nommage du socket
*		-2  :  Erreur lors de l'ecoute
*		0 si l'initialisation s'est bien passee
*/
int initialisation (int port) {
	/* Creation de la socket IPV4 / TCP*/
	socketServeur = socket(PF_INET, SOCK_STREAM, 0);

	/* Description des donnees */
	adServ.sin_family = AF_INET;	/* IPV4 */
	adServ.sin_addr.s_addr = INADDR_ANY;	/* "N'importe quelle IP" */
	adServ.sin_port = htons(port);	/* PORT */

	/* Nommage du socket */
	int res = bind (socketServeur, (struct sockaddr*) &adServ, sizeof(adServ));
	if (res < 0) {
		return -1;
	}

	/* Ecoute jusqu'a 10 clients*/
	res = listen (socketServeur, 10);
	if (res < 0) {
		return -2;
	}

	return 0;
}

int main (int argc, char *argv[]) {
	/* Ferme proprement le socket si CTRL+C est execute */
	signal(SIGINT, fermer);

	return 0;
}