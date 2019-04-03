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

/* Socket (globaux pour la fonction "fermer") */
int socketServeur; 
int socketClient;

/* Ferme les sockets et quitte le programme */
void fermer() {
	int res = close (socketClient);
	if (res == 0) {
		printf("\nFermeture du socket client\n");
	} else {
		perror("\nErreur fermeture du socket client\n");
	}

	int res2 = close(soc);
	if (res2 == 0) {
		printf("\nFermeture du socket serveur\n");
	} else {
		perror("\nErreur fermeture  du socket serveur\n");
	}
	exit(0);
}

int main (int argc, char *argv[]) {
	/* Ferme proprement le socket si CTRL+C est execute */
	signal(SIGINT, fermer);

	return 0;
}