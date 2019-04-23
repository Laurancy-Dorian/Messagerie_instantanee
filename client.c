/************************************************
*												*
* 	MESSAGERIE INSTANTANEE - CLIENT - V3		*
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
#define TAILLE_BUFFER 2048


char* IP = "127.0.0.1";

/*	
*	Declaration du Socket et de la structure. Ils doivent etre globaux pour que la fonction 
* 	"fermer" puisse y acceder (car elle peut etre declenchee par un CTRL + C) 
*/
int socketClient; 
struct sockaddr_in adServ;

/* Dernier message recu */
char lastMsg[TAILLE_BUFFER];

/* Cette variable assure la communication entre les deux threads */
int fin;
pthread_mutex_t mutex_fin;
pthread_mutex_t mutex_lastMsg;



/* Declaration des fonctions */
void fermer();
void erreur(char *erreur);
int envoiMessage (char *msg);
int getMsgTerminal(char* msg, int taille);
int envoi(char *msg, int taillemsg);
int reception(char *str, int taillestr);
void initialitation(char *ip, int port);
int connexion();
void decoThread (int val);
void *lire();
void *ecrire();
int conversation();
int envoiUDP(int socket, char* msg, struct sockaddr *donneesReceveur);
int receptionUDP(int socket, char* msg, int tailleMsg, struct sockaddr *donneesEnvoyeur);
void *envoiFichier();
void *receptionFichier();

/* 
* Ferme le socket et quitte le programme 
*/
void fermer() {
	printf("\n* -- FIN -- *\n");
	close(socketClient);
	printf("\n* -- FERMETURE DU SOCKET -- *\n");
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
*	Envoie le message au tiers (UDP)
*	param : int 				socket 				Le socket 
*			char*				msg 				Le message a envoyer
*			(struct sockaddr*) 	donneesReceveur		Les donnees du tiers (IP, port ...)
*	return : si reussi, renvoit le nombre de caractères emis. 
*			 echec, renvoit -1 et errno contient le code d'erreur.  
*/
int envoiUDP(int socket, char* msg, struct sockaddr *donneesReceveur) {
	return (int) sendto(socket, msg, strlen(msg)+1, 0, (struct sockaddr*) &donneesReceveur, sizeof(struct sockaddr_in));
}

/* 
*	Recois un message du tiers
*	param : int 				socket 				Le socket 
*			char*				msg 				Le message a envoyer
*			int 				tailleMsg 			La taille max dans msg
*			(struct sockaddr*) 	donneesEnvoyeur		Les donnees du tiers (IP, port ...)
*	return : si reussi, renvoit le nombre de caractères recus. 
*			 echec, renvoit -1 et errno contient le code d'erreur.  
*/
int receptionUDP(int socket, char* msg, int tailleMsg, struct sockaddr *donneesEnvoyeur) {
	socklen_t lgA = sizeof(struct sockaddr_in);
	return (int) recvfrom (socket, msg, tailleMsg, 0, (struct sockaddr*) &donneesEnvoyeur, &lgA);
}

/*
*	Envoie le message au serveur
*	param : char*	msg 	Le message a envoyer
*	return : si reussi, renvoit le nombre de caractères emis. 
*			 echec, renvoit -1 et errno contient le code d'erreur.  
*/
int envoiMessage (char *msg) {
	return (int) send(socketClient, msg, strlen(msg)+1, 0);
}

/*
*	Enregistre dans msg ce qui est tape dans le terminal
*	param :		char* 	msg 	Le buffer ou sera enregistre la chaine
*				int 	taille 	La taille max de ce buffer
*	return : La taille de la chaine de caracteres lue
*/
int getMsgTerminal(char* msg, int taille) {
	/* Lecture du pseudo */
	fgets(msg, taille, stdin);
	char *pos = strchr(msg, '\n');
	*pos = '\0';

	return strlen(msg);
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
	int res = getMsgTerminal(msg, taillemsg);

	if (res == -1) {
		return 0;
	}

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

void *envoiFichier() {
	int res = -1;
	int socUDP = socket(PF_INET, SOCK_DGRAM, 0);
	int port;

	/* Reception du message contenant l'IP du correspondant */
	pthread_mutex_lock(&mutex_lastMsg);
	char* ip = &lastMsg[6]; // Recupere l'ip

	/* Donnees de l'envoyeur */
	struct sockaddr_in ad;
	ad.sin_family = AF_INET;	/* IPV4 */
	ad.sin_addr.s_addr = inet_pton(AF_INET, ip, &(ad.sin_addr));

	while (res == -1) {
		port = 10000 + (rand() % 15000); // Random entre 10 000 et 25 000
		ad.sin_port = htons(port);	/* Port */
		res = bind (socUDP, (struct sockaddr*) &ad, sizeof(ad));
	}


	/* Envoie le port au serveur */
	char str[TAILLE_BUFFER];
	sprintf(str, "%d", port);
	envoiMessage(str);


	/* Reception du message contenant le port du correspondant */
	pthread_mutex_lock(&mutex_lastMsg);
	char* port_distant = &lastMsg[6]; // Recupere le port

	/* Donnees du receveur */
	struct sockaddr_in adRec;
	adRec.sin_family = AF_INET;	/* IPV4 */
	adRec.sin_addr.s_addr = inet_pton(AF_INET, ip, &(ad.sin_addr));
	adRec.sin_port = atoi(port_distant);

	/* Selection du fichier */
	// TODO

	return NULL;
}

// TODO
void *receptionFichier() {
	return NULL;

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
	return connect(socketClient, (struct sockaddr *) &adServ, lgA) ;
}

/* 
* 	Assigne une valeur a la variable globale fin et temine le thread
*	param : 	int 	val 	La valeur a assigner dans fin
*/
void decoThread (int val) {
	pthread_mutex_lock(&mutex_fin);
	fin = val;
	pthread_mutex_unlock(&mutex_fin);
	pthread_exit(0);
}

int commande (char* msg) {
	if (strlen(msg) == 3 && strncmp("/fin", msg, 3) == 0){
		decoThread(0);
	} else if (strcmp("/FILESEND", msg) <= 0) {
		pthread_t sendFile;
		pthread_create(&sendFile, 0, envoiFichier, 0);

	} else if (strcmp("/FILERECV", msg) <= 0) {
		pthread_t recvFile;
		pthread_create(&recvFile, 0, receptionFichier, 0);
	} else {
		strcpy(lastMsg, msg);
		pthread_mutex_unlock(&mutex_lastMsg);
	}
	return 0;
}

/* 	
*	Fonction *lire 
*	lis les messages envoyés par l'autre client en boucle.
*	Si la lecture echoue ou que le message reçu est FIN, ferme le thread et met la variable globale fin à 1.
*/
void *lire() {
	char msg[256];
	int res = 0;

	while (fin < 0) {
		res = reception(msg, 256);
		if (res == 0){

			if (strncmp ("/", msg, 1) == 0) {
				commande(msg);
			}

			else{
				printf("%s\n", msg);
			}
		}
		else {
			decoThread(1);
		}
	}
	return NULL;
}



/* 	
*	Fonction ecrire
*	Envoie les messages du client en boucle.
*	Si l'envoi echoue : ferme le thread et met la variable globale fin à 0.
*/
void *ecrire() {
	char msg[256];
	int res = 0;

	while(fin < 0){
		/* Envoie le message. Si la chaine est "FIN", ou si l'envoi echoue, deconnecte */
		
		res = envoi(msg, 256);
		if (res == 0) {
			if (strncmp ("/", msg, 1) == 0) {
				commande(msg);
			}
		}
		else {
			decoThread(0);
		}

	}

	return NULL;
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

	//creation des 2 threads 
	pthread_t lecture;
	pthread_t ecriture;
	pthread_create(&lecture, 0, lire, 0);
	pthread_create(&ecriture, 0, ecrire, 0);

	pthread_join(lecture, NULL);
	pthread_join(ecriture, NULL);

	return fin;
}

/*
*	./client [PORT "IP" Pseudo]
*
*	Exemple : ./client 2051 "192.168.1.100" Toto
*/
int main (int argc, char *argv[]) {
	/* Ferme proprement le socket si CTRL+C est execute */
	signal(SIGINT, fermer);

	/* --- RECCUPERATION DES PARAMETRES --- */
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

	/* Reccupere le pseudo passe en parametre*/
	char pseudo[128];
	int resPseudo = 0;
	if (argc >= 4) {
		strcpy (pseudo, argv[3]);
		resPseudo = strlen(pseudo);
	} 
	

	/* --- SECTION PRINCIPALE --- */
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

	/* Demande au client de taper son pseudo */
	while(resPseudo < 3) {
		printf("Veuillez entrer un pseudo de taille 3 minimum\n==> ");
		resPseudo = getMsgTerminal(pseudo, 128);
	}
	printf("Votre pseudo est : %s\n", pseudo);
	envoiMessage(pseudo);

	/* Attend le message "BEGIN" du serveur. Si le client attend longtemps ici, cela veut dire qu'il n'y a plus de
	*  place dans la conversation. Le client sera alors connecte lorsqu'un autre client quittera 
	*/
	printf("Veuillez patienter, vous etes en file d'attente...\n");
	char str[128];
	int resDebut = reception(str, 128);
	if (resDebut == 0 && strlen(str) == 5 && strncmp("BEGIN", str, 5) == 0) {

		/* Init du mutex sur fin */
		pthread_mutex_init(&mutex_fin,0);
		pthread_mutex_init(&mutex_lastMsg, 0);
		pthread_mutex_lock(&mutex_lastMsg);

		/* Si les deux pairs sont bien connectes, lance la conversation */
		printf("\n* -- DEBUT DE LA CONVERSATION -- *\n");

		conversation();

		printf("\n* -- FIN DE LA CONVERSATION -- *\n");
	}

	/* Ferme la connexion avec le serveur */
	close(socketClient);

	/* Ferme le client */
	fermer();
	return 0;
}
