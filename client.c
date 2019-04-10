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

int socketClient; /* Socket (global pour la fonction "fermer" */
struct sockaddr_in adServ;

/*Connecte le client au serveur, retourne 0 si ok, -1 sinon */
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

/*Début */
/*Attente du message du serveur pour l'ordre */
int attente() {
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

/* Affiche une erreur et quitte le programme */
void erreur(char *erreur) {
	perror(erreur);
	fermer();
}

/* Envoi du message et Message de confirmation de l'envoi */
int envoi(char *msg, int taillemsg) {
	ssize_t envoi;
	printf("Entrez un message :\n");
	fgets(msg, taillemsg, stdin);
	char *pos = strchr(msg, '\n');
	*pos = '\0';

	envoi = send(socketClient, msg, strlen(msg)+1, 0);

	if (envoi < 0) {
		return -1;
	} else {
		return 0;
	}
}

/* Réception du message */
int reception(char *str,int taillestr) {
	ssize_t reception = recv (socketClient, str, taillestr, 0);
	if (reception < 0) {
		return -1;
	} else {
		return 0;
	}
}

int conversation(int ordre) {
	char msg[256];
	int res;
	while(1){
		if (ordre == 0){
			res = envoi(msg, 256);
			if (res == 0) {
				if (strncmp("FIN", msg, 3)){
					return 0;
				}
				else{
					printf("Message envoyé \n");
				}
			}
			else {
				return 0;
			}
			res = reception(msg, 256);
			if (res == 0){
				if (strncmp("FIN", msg, 3)) {
					return 1;
				}
				else{
					printf("Message reçu : %s\n", msg);
				}
			}
			else{
				return 1;
			}
		} else {
			res = reception(msg, 256);
			if (res == 0){
				if (strncmp("FIN", msg, 3)) {
					return 1;
				}
				else{
					printf("Message reçu : %s\n", msg);
				}
			}
			else{
				return 1;
			}
			res = envoi(msg, 256);
			if (res == 0) {
				if (strncmp("FIN", msg, 3)){
					return 0;
				}
				else{
					printf("Message envoyé \n");
				}
			}
			else {
				return 0;
			}
		}
	}
}

int main (int argc, char *argv[]) {
	/* Ferme proprement le socket si CTRL+C est execute */
	signal(SIGINT, fermer);

	return 0;
}
