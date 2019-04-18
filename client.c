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
#include <pthread.h>

/* Definition de constantes */
#define PORT 2500

char* IP = "127.0.0.1";

/*	
*	Declaration du Socket et de la structure. Ils doivent etre globaux pour que la fonction 
* 	"fermer" puisse y acceder (car elle peut etre declenchee par un CTRL + C) 
*/
int socketClient; 
struct sockaddr_in adServ;
int fin;


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


/* 	Fonction lire 
*	lis les messages envoyés par l'autre client en boucle.
*	Si la lecture echoue ou que le message reçu est FIN ferme le thread et met fin à 1.
*/
void *lire() {
	while(1){
		char msg[256];
		int res = 0;

		res = reception(msg, 256);
			if (res == 0){
				if (strlen(msg) == 3 && strncmp("FIN", msg, 3) == 0) {
					fin = 1;
					pthread_exit(0);
				}
				else{
					printf("~~~~~> %s\n", msg);
				}
			}
			else {
				fin = 1;
				pthread_exit(0);
			}
	}
}

/* 	Fonction ecrire
*	Envoie les messages du client en boucle.
*	Si l'envoi echoue ferme le thread et met fin à 0.
*/
void *ecrire() {
	while(1){
		/* Envoie le message. Si la chaine est "FIN", ou si l'envoi echoue, 
		*  retourne 0 (ce client quittera la conversation) */
		char msg[256];
		int res = 0;


		res = envoi(msg, 256);
		if (res == 0) {
			if (strlen(msg) == 3 && strncmp("FIN", msg, 3) == 0){
				fin = 0;
				pthread_exit(0);
			}
		}
		else {
			fin = 0;
			pthread_exit(0);
		}
	}
}


/*
*	Conversation entre les deux pairs. 
*	Création de 2 threads :
*		-un thread lecture qui utilise la fonction lire (qui reçoit le message et l'affiche).
*		-un thread ecriturte qui utilise la fonction ecrire (qui envoie le message).
*	return:	0 si ce client termine la conversation, 1 si le client distant termine la conversation	
*/
int conversation() {
	fin = -1;

	//création des 2 threads 
	pthread_t lecture;
	pthread_t ecriture;
	pthread_create(&lecture, 0, lire, 0);
	pthread_create(&ecriture, 0, ecrire, 0);

	pthread_join(lecture, NULL);
	pthread_join(ecriture, NULL);

	return fin;
}

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


		/* Si les deux pairs sont bien connectes, lance la conversation */
		printf("\n* -- DEBUT DE LA CONVERSATION -- *\n");

		retourConv = conversation();

		printf("\n* -- FIN DE LA CONVERSATION -- *\n");
	
		/* Ferme la connexion avec le serveur */
		close(socketClient);
	}

	/* Ferme le client */
	fermer();
	return 0;
}
