/************************************************
*												*
* 	MESSAGERIE INSTANTANEE - SERVEUR - V3		*
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
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <pthread.h>
#include <semaphore.h>

/* Definition de constantes */
#define PORT 2500
#define NB_MAX_CLIENTS 2
#define TAILLE_BUFFER 2048
#define TAILLE_PSEUDO 128

/* 
*	Declaration des Sockets. Ils doivent etre globaux pour que la fonction 
* 	"fermer" puisse y acceder (car elle peut etre declenchee par un CTRL + C) 
*/
int socketServeur; 
int socketClients[NB_MAX_CLIENTS];
char pseudoClients[NB_MAX_CLIENTS][TAILLE_PSEUDO];
pthread_t threadClients[NB_MAX_CLIENTS];

/* Structures contenant les donnees du serveur et des clients */
struct sockaddr_in adServ;
struct sockaddr_in adClient[NB_MAX_CLIENTS]; 

socklen_t lgA = sizeof(struct sockaddr_in);	

/* Mutex pour le numero du client dans le tableau : Cette valeur tres importante pour le thread du client 
* (position dans le tableau des sockets) etait remplacee avant d'arriver dans le thread, ce mutex resoud le probleme */
pthread_mutex_t mutex_numClient;

/* Ce semaphore permet de ne plus accepter de nouveaux clients lorsqu'il y a autant de clients que NB_MAX_CLIENTS */
sem_t semaphore_nb_clients; 


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

	/* Ecoute jusqu'a NB_MAX_CLIENTS clients*/
	res = listen (socketServeur, NB_MAX_CLIENTS);
	if (res < 0) {
		return -2;
	}

	return 0;
}

/*
*	Attend qu’un client se connecte.
*	Lorsqu'un client s'est connecte, ses donnees sont stockees dans le 2eme parametre
*	Et son socket dans le 1er parametre
*	param : int*		socClient 		La variable ou sera stockee le socket
*			sockaddr*	donneesClient	La variable ou seront stockees les donnees du client
*	return : -1 si echec, 0 sinon
*
*/
int attenteConnexion(int* socClient, struct sockaddr_in *donneesClient) {
	/* Accepte la connexion avec un client */
	int retour = accept(socketServeur, (struct sockaddr *) donneesClient, &lgA);

	if (retour > 0) {
		*socClient = retour;
		return 0;
	} else {
		return -1;
	}
}


/*
*	Envoie le message msg au client dont le socket est renseigne en parametre
*	param : 	int 	socClient 	Le socket du client ou envoyer les donnees
*				char*	msg 		La chaine a envoyer
*	return : 	le nombre d'octets envoyes en cas de reussite, -1 sinon
*/
int envoi (int socClient, char* msg) {
	return (int) send(socClient, msg, strlen(msg)+1, MSG_NOSIGNAL);
}

/*
*	Attend une reponse du client et la stocke dans le second parametre
*	param : 	int 	socClient 	Le socket du client qui envoie le message
*				char*	msg 		Le pointeur de la chaine de caracteres ou stocker le message
*				int 	taille		Le nombre d'octets max a lire, si taille == 0, la valeur par defaut est TAILLE_BUFFER
*
*	return : 	-1 si echec
*				0 si le client s'est deconnecte normalement
*				Le nombre d'octets lus sinon
*/
int reception (int socClient, char* msg, int taille) {
	if (taille == 0) {
		taille = TAILLE_BUFFER;
	}
	return (int) recv (socClient, msg, taille, 0);
}

/* 
*	Envoie le message a tout les clients
*	param : 	int 	numClient 	Le NUMERO du client qui envoie le message (et non le descripteur de socket)
*				char*	msg 		La chaine a envoyer

*	return : 	-1 si echec
*				1 si reussite
*/
int broadcast(int numClient, char* msg) {
	for (int i = 0; i<NB_MAX_CLIENTS; i++) {

		/* Envoie uniquement au clients connectes (socket != -1 sauf au client envoyeur) */
		if (i != numClient && socketClients[i] != -1) {
			if (envoi (socketClients[i], msg) == -1) {
				return -1;
			}
		}
	}
	return 1;
}

/*
*	Deconnecte le socket du client dont le numero du tableau des sockets et passé en parametre
*	Si le parametre est -1, alors on deconnecte tous les sockets
* 	
*	param : 	int 	numSocket	La position du socket dans le tableau
*/
void deconnecterSocket(int numSocket) {
	int debut;
	int fin;

	if (numSocket == -1) {
		debut = 0;
		fin = NB_MAX_CLIENTS;
	} else {
		debut = numSocket;
		fin = numSocket+1;
	}

	int i;
	for (i = debut; i < fin; i++) {
		if (socketClients[i] != -1) {
			printf ("Client n°%d %s deconnecte\n\n", i, pseudoClients[i]);

			/* Envoie "FIN" au client pour qu'il se deconnecte de son conte */
			envoi(socketClients[i], "FIN");
			shutdown (socketClients[i], 2);
			socketClients[i] = -1;

			/* Libere un jeton dans le semaphore */
			sem_post(&semaphore_nb_clients);
		}
	}

}

/*
* 	Ferme les sockets et quitte le programme
*/
void fermer() {
	printf("\n\n* -- FERMETURE DU SERVEUR -- *\n");

	/* Ferme tous les sockets clients */
	deconnecterSocket(-1);
	printf("Fermeture des sockets clients\n");

	/* Ferme le socket serveur */
	int res = close(socketServeur);
	if (res == 0) {
		printf("Fermeture du socket serveur\n");
	} else {
		perror("Erreur fermeture  du socket serveur\n");
	}

	/* Ferme les semaphores */
	sem_destroy(&semaphore_nb_clients);
	pthread_mutex_destroy(&mutex_numClient);

	printf("* -- FIN DU PROGRAMME -- *\n");

	exit(0);
}


/*
*	Transmet un message du client a tous les autres clients
*	Si ce client envoie fin ou se deconnecte, la fonction retourne directement -1
*	param : int 	numSocEnvoyeur		Le NUMERO du client envoyeur dans le tableau des sockets (pas le descripteur)
*	return : 
*			 -1 si erreur lors de la reception du message du client
			 1 Si le client se deconnecte ou envoie "FIN"
*			 0 sinon
*/
int envoi_reception (int numSocEnvoyeur) {
	/* Initialisation du buffer */
	char str[TAILLE_BUFFER];
	int res;
	int socEnvoyeur = socketClients[numSocEnvoyeur];

	
	/* ---- RECEPTION ---- */
	/* Attend le message du client envoyeur */
	res = reception (socEnvoyeur, str, 0);

	/* Erreur lors de la reception */
	if (res < 0) {
		return -1;
	} else if (res == 0 || (strlen(str) == 3 && strncmp ("FIN", str, 3) == 0)) { /* Le client s'est deconnecte */
		return 1;
	}

	/* Ajout du pseudo dans le message avant de l'envoyer aux autres clients "[pseudo] message" */
	char msgClient[strlen(pseudoClients[numSocEnvoyeur]) + strlen(str) + 4]; /* (+4) pour les 4 caracteres en plus ([] \0 ' ')*/
	strcpy(msgClient, "[");
	strcat(msgClient, pseudoClients[numSocEnvoyeur]);
	strcat(msgClient, "] ");
	strcat(msgClient, str);


	/* ---- ENVOI ---- */
	/* Envoie le message aux autres clients */
	broadcast(numSocEnvoyeur, msgClient);

	return 0;

}


/*
*	Effectue la transition des message entre ce client et les autres clients
*	Transmet ensuite les messages de ce client a tous les autres
*
*	param : 	int 	numcli 	Le NUMERO du client dans le tableau des sockets (pas le descripteur)
*
*/
void *conversation (int* numcli) {
	/* init de variable et liberation du mutex sur numClient */
	int numClient = *numcli;
	pthread_mutex_unlock(&mutex_numClient);

	int res = 0;
	char str[128];

	/* Reception du pseudo du client */
	int resPseudo = reception(socketClients[numClient], str, TAILLE_PSEUDO);
	strcpy(pseudoClients[numClient], str);

	if (resPseudo > 3) {
		envoi(socketClients[numClient], "BEGIN");
		/* Envoie un message de connexion a tous les clients */
		sprintf(str, ">>>> %s s'est connecte", pseudoClients[numClient]);
		broadcast(numClient, str);

		/* Boucle de la conversation */
		while(res == 0) {
			res = envoi_reception(numClient);
		}

		/* Envoie le message de deconnexion a tous les clients */
		sprintf(str, "<<<< %s s'est deconnecte", pseudoClients[numClient]);
		broadcast(numClient, str);
	}

	/* Deconnecte et quitte le thread */
	deconnecterSocket (numClient);
	pthread_exit(0);
}



/* 
*	Stocke l'IP du serveur dans la variable passee en parametre
*	prereq : ip possede au moins 50 caracteres alloues
*	
*	Reference :
* 	https://stackoverflow.com/questions/646241/c-run-a-system-command-and-get-output
*/
void ipServeur(char* ip) {
	FILE *fp;

	/* Open the command for reading. */
	fp = popen("hostname --all-ip-addresses", "r");
	if (fp == NULL) {
		printf("Failed to run command\n" );
		exit(1);
	}

	/* Read the output a line at a time - output it. */
	fgets(ip, 50, fp);
	char *pos = strchr(ip, '\n');
	*pos = '\0';

	/* Coupe pour prendre seulement la 1ere ip */
	ip = strtok(ip, " ");

	/* close */
	pclose(fp);
	
}

/*
*	./serveur [PORT]
*/
int main (int argc, char *argv[]) {
	/* Ferme proprement le socket si CTRL+C est execute */
	signal(SIGINT, fermer);

	/* --- INITIALISATION DU SERVEUR --- */

	/* Reccupere le port passe en argument, si aucun port n'est renseigne, le port est la constante PORT */
	int port;
	if (argc > 1) {
		port = atoi(argv[1]);
	} else {
		port = PORT;
	}

	printf("* -- INITIALISATION DU SERVEUR -- *\n");
	int resInit = initialisation(port);

	if (resInit == -1) {
		perror ("Erreur lors du nommage du Socket Serveur");
		fermer();
	} else if (resInit == -2) {
		perror ("Erreur lors de l'ecoute du Socket Serveur");
		fermer();
	} else {
		printf("* -- SERVEUR INITIALISE -- *\n");

		/* Reccupere l'IP et le port du serveur et les affiche*/
		char ipServ[50];
		ipServeur(ipServ);
		printf("IP : %s\nPort : %d\n", ipServ, port);
	}

	/* Initialise le tableau des sockets à -1 : Lorsqu'une case est egale a -1, on peut l'ulitilser pour l'attribuer au prochain client */
	int i = 0;
	for (i = 0; i < NB_MAX_CLIENTS; i++) {
		socketClients[i] = -1;
	}

	/* Init du mutex sur numClient */
	pthread_mutex_init(&mutex_numClient,0);
	sem_init(&semaphore_nb_clients, 0, NB_MAX_CLIENTS);

	/* --- BOUCLE PRINCIPALE --- */
	while (1) {
		sem_wait(&semaphore_nb_clients);
		
		/* Lock du mutex pour numClient */
		pthread_mutex_lock(&mutex_numClient);
		int numClient = -1;

		/* Recherche un slot disponible dans le tableau pour le prochain client */
		i = 0;
		while (i < NB_MAX_CLIENTS && numClient == -1) {
			if (socketClients[i] == -1) {
				numClient = i;
			}
			i++;
		}

		printf ("Attente de connexion d'un client\n");
		int resConnexion = attenteConnexion(&socketClients[numClient], &adClient[numClient]);

		if (resConnexion == 0) {
			/* Reccupere l'IP du client et l'affiche*/
			char ipclient[50];
			inet_ntop(AF_INET, &(adClient[numClient].sin_addr), ipclient, INET_ADDRSTRLEN);	

			printf ("Client de numero %d et d'ip %s connecte !\n\n", numClient, ipclient);
		} else {
			perror ("Erreur lors de la connexion au client\n");
			deconnecterSocket(numClient);
		}

		/* Cree un thread dedie pour ce client */
		pthread_create(&threadClients[numClient], 0, (void*) conversation, &numClient);

	}

	/* Ferme le serveur */
	fermer();
	return 0;
}
