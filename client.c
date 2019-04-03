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

int socket; /* Socket (global pour la fonction "fermer")

/* Ferme le socket et quitte le programme */
void fermer() {
	printf("\nINTERRUPTION DU PROGRAMME (CTRL+C)\n");
	int res = close(socket);
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