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
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <pthread.h>

/* Definition de constantes */
#define PORT 2500
#define NB_MAX_CLIENTS 2
#define TAILLE_BUFFER 2048

/* 
*	Declaration des Sockets. Ils doivent etre globaux pour que la fonction 
* 	"fermer" puisse y acceder (car elle peut etre declenchee par un CTRL + C) 
*/
int socketServeur; 
int socketClients[NB_MAX_CLIENTS];

/* Structures contenant les donnees du serveur et des clients */
struct sockaddr_in adServ;
struct sockaddr_in adClient[NB_MAX_CLIENTS]; 

socklen_t lgA = sizeof(struct sockaddr_in);	

/* Le nombre de clients connectés actuellement */
int nb_client = 0;



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

	/* Ecoute jusqu'a NB_CLIENTS clients*/
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
* Ferme les sockets et quitte le programme
*/
void fermer() {
	printf("\n\n* -- FERMETURE DU SERVEUR -- *\n");

	int i;

	/* Ferme tous les sockets clients */
	for (i = 0; i < NB_MAX_CLIENTS; i++) {
		int res = 0;
		if (socketClients[i] > 0) {
			envoi(socketClients[i], "FIN");
			res = close (socketClients[i]);
		}
		if (res == 0) {
			printf("Fermeture du socket client numero %d\n", i);
		} else {
			printf("Erreur fermeture du socket client numero %d\n", i);
			perror(NULL);
		}
	}

	/* Ferme le socket serveur */
	int res = close(socketServeur);
	if (res == 0) {
		printf("Fermeture du socket serveur\n");
	} else {
		perror("Erreur fermeture  du socket serveur\n");
	}

	printf("* -- FIN DU PROGRAMME -- *\n");

	exit(0);
}


/*
*	Transmet le message du client envoyeur au client receveur
*	Si l'un des clients envoie "FIN" ou se deconnecte, Envoie "FIN" a l'autre client
*	param : int 	socEnvoyeur		Le descripteur du socket du client envoyeur
*			int 	socReceveur		Le descripteur du socket du client receveur
*	return : 
*			 -1 si erreur lors de la reception du message du client envoyeur
*			 -2 si erreur lors de l'envoi au client receveur
*			 -3 si les deux client se sont deconnectes durant cet echange
			 Si un client se deconnecte ou envoie "FIN", renvoie son descripteur de socket
*			 0 sinon
*/
int envoi_reception (int socEnvoyeur) {
	/* Initialisation du buffer */
	char str[TAILLE_BUFFER];
	int res;

	
	/* ---- RECEPTION ---- */
	/* Attend le message du client envoyeur */
	res = reception (socEnvoyeur, str, 0);

	/* Erreur lors de la reception */
	if (res < 0) {
		return -1;
	} else if (res == 0 || (strlen(str) == 3 && strncmp ("FIN", str, 3) == 0)) { /* Le client s'est deconnecte */
		return socEnvoyeur;
	}


	/* ---- ENVOI ---- */
	/* Envoie le message aux autres clients */
	for (int i = 0; i<nb_client; i++) {
		if (i != socEnvoyeur && socketClients[i] != -1) {
			res = 0;
			res = envoi (socketClients[i], str);
			printf("%d envoie %s à %d", socEnvoyeur, str, i);
		}
	}
	return 0;

}

/*
*	Deconnecte lee socket du client dont le numero du tableau des sockets et passé en parametre
*	Si le parametre est -1, alors on deconnecte tous les sockets
* 	
*	param : 	int 	numSocket	La position du socket
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
		/* Deconnecte */
		shutdown (socketClients[i], 2);
		socketClients[i] = -1;
	}

}

/*
*	Effectue la transition des message entre les deux clients
*	Envoie tout d'abord au client leur ordre :
*		- "NUM1" au client qui parlera en premier
*		- "NUM2" au client qui parlera en deuxieme
*	Transmet ensuite les messages d'un client a un autre
*	Si un client se deconnecte ou envoie "FIN", envoie "FIN" a l'autre client
*
*	param : 	int 	socClient1 	Le socket du client qui parlera en 1er
*
*	return : 	0 si le premier client s'est deconnecte
*				1 si le deuxieme client s'est deconnecte
*				2 si les deux clients se sont deconnectes
*				-1 si erreur
*
*/
void *conversation (int* numSoc) {
	int soc = *numSoc;
	printf("soc : %d\n", soc);
	
	while(1) {
		envoi_reception(soc);
	}
	printf ("%d s'est déconnecté\n", soc);
	deconnecterSocket (soc);

	
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

	int i = 0;
	for (i = 0; i < NB_MAX_CLIENTS; i++) {
		socketClients[i] = -1;
	}

	/* --- BOUCLE PRINCIPALE --- */
	while (1) {
		i = 0;
		int numClient = -1;

		/* Attend la connexion de tous les clients */
		while (i < NB_MAX_CLIENTS && numClient == -1) {
			if (socketClients[i] == -1) {
				numClient = i;
				printf("TROUVE\n");
			}
			i++;
		}

		printf ("Attente de connexion d'un client\n");
		int resConnexion = attenteConnexion(&socketClients[numClient], &adClient[numClient]);

		if (resConnexion == 0) {
			/* Reccupere l'IP du client et l'affiche*/
			char ipclient[50];
			inet_ntop(AF_INET, &(adClient[i].sin_addr), ipclient, INET_ADDRSTRLEN);	

			printf ("Client %s connecte !\n\n", ipclient);
		} else {
			perror ("Erreur lors de la connexion au client\n");
		}
		

		/* Lancement de la conversation */

		printf("\n* -- CONNEXION DU CLIENT -- *\n");

		pthread_t transfert;
		printf("%d\n", numClient);
		pthread_create(&transfert, 0, (void*) conversation, &numClient);

	}

	/* Ferme le serveur */
	fermer();
	return 0;
}
