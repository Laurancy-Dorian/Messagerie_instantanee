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

/* Definition de constantes */
#define PORT 2500
char* IP = "127.0.0.1";

/*	
*	Declaration du Socket et de la structure. Ils doivent etre globaux pour que la fonction 
* 	"fermer" puisse y acceder (car elle peut etre declenchee par un CTRL + C) 
*/
int socketClient; 
struct sockaddr_in adServ;


/* 
* Ferme le socket et quitte le programme 
*/
void fermer() {
	printf("\n* -- FIN -- *\n");
	int res = close(socketClient);
	if (res == 0) {
		printf("\n* -- FERMETURE DU SOCKET -- *\n");
	} else {
		perror("\nErreur fermeture du socket\n");
		exit(-1);
	}
	exit(0);
}

/*
* 	Affiche une erreur et quitte le programme
*	param : char*	erreur 		Un message a afficher avant de quitter
*/
void erreur(char *erreur) {
	perror(erreur);
	fermer();
}

/*
*	Envoie le message au serveur
*	param : char*	msg 	Le message a envoyer
*	return : si reussi, renvoit le nombre de caractères émis. 
*			 echec, renvoit -1 et errno contient le code d'erreur.  
*/
int envoiMessage (char *msg) {
	return (int) send(socketClient, msg, strlen(msg)+1, 0);
}

/* 
* 	Demande a l'utilisateur de taper un message et l'envoie au serveur
*	param :	char*	msg 		Le message envoye sera stocke dans cette variable
*			int 	taillemsg 	La taille maximale qui peut etre tapee par l'utilisateur (taille max de msg)
*	return : 0 si ok, -1 si ko
*/
int envoi(char *msg, int taillemsg) {
	ssize_t envoi;

	/* Lecture du message */
	printf("==> ");
	fgets(msg, taillemsg, stdin);
	char *pos = strchr(msg, '\n');
	*pos = '\0';

	/* Envoi */
	envoi = envoiMessage(msg);

	if (envoi < 0) {
		return -1;
	} else {
		return 0;
	}
}

/* 
*	Recois un message du serveur
*	param: :	char* 	str 		La chaine de caracter ou sera stockee le message
*				int 	taillestr	La taille max de cette chaine
*	return :	0 si ok, -1 si ko
*/
int reception(char *str, int taillestr) {
	ssize_t reception = recv (socketClient, str, taillestr, 0);
	if (reception < 0) {
		return -1;
	} else {
		return 0;
	}
}

/* 
*	Initialise le serveur
*	param : char* 	ip 		L'adresse IP du serveur
*			int 	port 	Le port sur lequel se connecter
*/
void initialitation(char *ip, int port) {
	/* Creation de la socket IPV4 / TCP*/
	socketClient = socket(PF_INET, SOCK_STREAM, 0);

	/* Donnees sur le serveur */
	
	adServ.sin_family = AF_INET; /* IPV4 */
	adServ.sin_port = htons(port);	/* Port */
	inet_pton(AF_INET, ip, &(adServ.sin_addr)); /* Adresse IP */
}

/*
*	Connecte le client au serveur
* 	return : 0 si ok, -1 si ko 
*/
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


/*
*	Attend que le serveur confirme que l'autre client s'est bien connecte.
*	Cette fonction retournera l'ordre de parole de la conversation
*	return : 0 si ce client parlera en premier
*			 1 si ce client parlera en deuxieme
*			 -1 si erreur ou si le client distant s'est deconnecte 
*/
int attente() {
	char str[10];
	int ordre;

	/* reception du message */
	ssize_t res = reception(str, 10);
	
	/* Si erreur */
	if (res < 0) {
		envoiMessage("KO");
		return -1;
	} else {


		/* Analyse du message, envoi KO si ce message n'est ni NUM1 ni NUM2 */
		if (strcmp(str, "NUM1") == 0) {
			ordre = 0;
		}
		else if (strcmp(str, "NUM2") == 0) {
			ordre = 1;
		}
		else {
			envoiMessage("KO");
			return -1;
		}

		/* Envoie la confirmation au serveur */
		envoiMessage("OK");


		/* Attend que le serveur lance la conversation ou non*/
		res = reception(str, 10);
		if (strcmp(str, "BEGIN") == 0) {
			return ordre;
		}
		else {
			return -1;
		}

	}

}

/*
*	Conversation entre les deux pairs. En fonction de l'ordre, le client va recevoir puis envoyer un message ou 
*	envoyer puis recevoir un message. La conversation s'arrete si le client recoit ou envoie "FIN"
*	param:	int 	ordre		l'ordre de parole de la conversation  :
*								- 0 : Le cient parle en premier
*								- 1 : Le client parle en deuxieme
*	return:	0 si ce client termine la conversation, 1 si le client distant termine la conversation	
*/
int conversation(int ordre) {
	char msg[256];
	int res = 0;
	while(1){
		/* Si le client parle en premier */
		if (ordre == 0){
			/* Envoie le message. Si la chaine est "FIN", ou si l'envoi echoue, 
			*  retourne 0 (ce client quittera la conversation) */
			res = envoi(msg, 256);
			if (res == 0) {
				if (strlen(msg) == 3 && strncmp("FIN", msg, 3) == 0){
					return 0;
				}
			}
			else {
				return 0;
			}

			/* Recoit un message et l'affiche. Si la chaine est fin ou si la reception echoue, 
			*  retourne 1 (le client distant quittera la conversation) */
			res = reception(msg, 256);
			if (res == 0){
				if (strlen(msg) == 3 && strncmp("FIN", msg, 3) == 0) {
					return 1;
				}
				else{
					printf("~~~~~> %s\n", msg);
				}
			}
			else {
				return 1;
			}

		/* Si le client parle en deuxieme */
		} else {

			/* Recoit un message et l'affiche. Si la chaine est fin ou si la reception echoue, 
			*  retourne 1 (le client distant quittera la conversation) */
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


			/* Envoie le message. Si la chaine est "FIN", ou si l'envoi echoue, 
			*  retourne 0 (ce client quittera la conversation) */
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

/*
*	./client [PORT] [IP]
*/
int main (int argc, char *argv[]) {
	/* Ferme proprement le socket si CTRL+C est execute */
	signal(SIGINT, fermer);


	/* Reccupere le port passe en argument, si aucun port n'est renseigne, le port est la constante PORT */
	int port;
	char ip[50];
	if (argc >= 2) {
		port = atoi(argv[1]);
	} else {
		port = PORT;
	}

	/* Reccupere l'adresse IP passee en argument, si aucune IP n'est renseignee, l'ip est la constante IP */
	if (argc >= 3) {
		strcpy (ip, argv[2]);
	} else {
		strcpy (ip, IP);
	}


	int retourConv = 1;

	/* Le client se reconnectera a la fin de chaque conversation si ce n'est pas lui qui a termine la conversation precedente */
	while (retourConv > 0) {
		retourConv = 1;

		/* INITIALISATION */
		initialitation(ip, port);

		/* CONNEXION AU SERVEUR */
		int resConnexion = connexion();
		if (resConnexion < 0) {
			erreur("Erreur de connexion au serveur");
		}
		else {
			printf("* -- CONNEXION -- *\n");
		}

		/* Attend que l'autre client soit connecte */
		printf("* -- ATTENTE DE L'AUTRE CLIENT -- *\n");
		int ordre = attente();


		/* Si les deux pairs sont bien connectes, lance la conversation */
		if (ordre >= 0) {
			printf("\n* -- DEBUT DE LA CONVERSATION -- *\n");

			retourConv = conversation(ordre);

			printf("\n* -- FIN DE LA CONVERSATION -- *\n");
		}

		/* Ferme la connexion avec le serveur */
		close(socketClient);
	}

	/* Ferme le client */
	fermer();
	return 0;
}
