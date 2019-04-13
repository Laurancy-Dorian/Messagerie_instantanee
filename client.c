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

#define PORT 2500

char* IP = "127.0.0.1";

int socketClient; /* Socket (global pour la fonction "fermer" */
struct sockaddr_in adServ;

/* Ferme le socket et quitte le programme */
void fermer() {
	printf("\n* -- INTERRUPTION DU PROGRAMME (CTRL+C) -- *\n");
	int res = close(socketClient);
	if (res == 0) {
		printf("\n* -- FERMETURE DU SOCKET -- *\n");
	} else {
		perror("\nErreur fermeture du socket\n");
		exit(-1);
	}
	exit(0);
}

/* Affiche une erreur et quitte le programme */
void erreur(char *erreur) {
	perror(erreur);
	fermer();
}

int envoiMessage (char *msg) {
	return (int) send(socketClient, msg, strlen(msg)+1, 0);
}

/* Envoi du message et Message de confirmation de l'envoi */
int envoi(char *msg, int taillemsg) {
	ssize_t envoi;
	printf("==> ");
	fgets(msg, taillemsg, stdin);
	char *pos = strchr(msg, '\n');
	*pos = '\0';

	envoi = envoiMessage(msg);

	if (envoi < 0) {
		return -1;
	} else {
		return 0;
	}
}

/* Réception du message */
int reception(char *str, int taillestr) {
	ssize_t reception = recv (socketClient, str, taillestr, 0);
	if (reception < 0) {
		return -1;
	} else {
		return 0;
	}
}

/* Initialise le serveur */
void initialitation(char *ip, int port) {
	/* Creation de la socket IPV4 / TCP*/
	socketClient = socket(PF_INET, SOCK_STREAM, 0);

	/* Donnees sur le serveur */
	
	adServ.sin_family = AF_INET; /* IPV4 */
	adServ.sin_port = htons(port);	/* Port */
	inet_pton(AF_INET, ip, &(adServ.sin_addr)); /* Adresse IP */
}

/*Connecte le client au serveur, retourne 0 si ok, -1 sinon */
int connexion() {

	/* -- Taille de la socket -- */
	socklen_t lgA = sizeof(struct sockaddr_in);

	/* Connexion au serveur */
	int res = connect(socketClient, (struct sockaddr *) &adServ, lgA) ;
	/* Message de confirmation de connexion */
	if (res < 0) {
		return -1;
	} else {
		return 0;
	}

}


/*Attente du message du serveur pour l'ordre */
int attente() {
	char str[10];
	ssize_t res = reception(str, 10);
	int ordre;

	if (res < 0) {
		envoiMessage("KO");
		return -1;
	} else {
		envoiMessage("OK");

		if (strcmp(str, "NUM1") == 0) {
			ordre = 0;
		}
		else if (strcmp(str, "NUM2") == 0) {
			ordre = 1;
		}
		else {
			return -1;
		}

		res = reception(str, 10);
		if (strcmp(str, "BEGIN") == 0) {
			return ordre;
		}
		else {
			return -1;
		}

	}

}


int conversation(int ordre) {
	char msg[256];
	int res = 0;
	while(1){
		if (ordre == 0){
			res = envoi(msg, 256);
			if (res == 0) {
				if (strlen(msg) == 3 && strncmp("FIN", msg, 3) == 0){
					return 0;
				}
			}
			else {
				return 0;
			}
			res = 0;
			res = reception(msg, 256);
			if (res == 0){
				if (strlen(msg) == 3 && strncmp("FIN", msg, 3) == 0) {
					return 1;
				}
				else{
					printf("~~~~~> %s\n", msg);
				}
			}
			else{
				return 1;
			}
		} else {
			res = reception(msg, 256);
			if (res == 0){
				if (strlen(msg) == 3 && strncmp("FIN", msg, 3) == 0) {
					return 1;
				}
				else{
					printf("~~~~~> %s\n", msg);
				}
			}
			else{
				return 1;
			}
			res = 0;
			res = envoi(msg, 256);
			if (res == 0) {
				if (strlen(msg) == 3 && strncmp("FIN", msg, 3) == 0){
					return 0;
				}
			}
			else {
				return 0;
			}
		}
	}
}

int main (int argc, char *argv[]) {
	/* Reccupere le port passe en argument, si aucun port n'est renseigne, le port est la constante PORT */
	int port;
	char ip[50];
	if (argc >= 2) {
		port = atoi(argv[1]);
	} else {
		port = PORT;
	}

	if (argc >= 3) {
		strcpy (ip, argv[2]);
	} else {
		strcpy (ip, IP);
	}

	/* Ferme proprement le socket si CTRL+C est execute */
	signal(SIGINT, fermer);


	int retour = 1;
	int res;

	while (retour > 0) {
		retour = 1;

		/* INITIALISATION ET CONNEXION*/
		initialitation(ip, port);
		res = connexion();
		if (res < 0) {
			erreur("Erreur de connexion au serveur");
		}
		else {
			printf("* -- CONNEXION -- *\n");
		}


		printf("* -- ATTENTE DE L'AUTRE CLIENT -- *\n");
		int ordre = attente();

		if (ordre >= 0) {
			printf("\n* -- DEBUT DE LA CONVERSATION -- *\n");

			retour = conversation(ordre);

			printf("\n* -- FIN DE LA CONVERSATION -- *\n");
		}

		close(socketClient);
		
	}

	fermer();
	return 0;
}
