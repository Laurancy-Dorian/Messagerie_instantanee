/************************************************
*												*
* 	MESSAGERIE INSTANTANEE - SERVEUR - v4		*
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

/* == Definition de constantes == */
#define PORT 2500
#define NB_MAX_CLIENTS 50
#define TAILLE_BUFFER 2048
#define TAILLE_PSEUDO 128
#define NB_MAX_SALONS 10
#define NB_COMMANDES 5
#define TRUE 1
#define FALSE 0

int NB_MAX_CLIENTS_SALONS_DEFAULT = NB_MAX_CLIENTS / NB_MAX_SALONS;
socklen_t lgA = sizeof(struct sockaddr_in);	

/* === Structures === */
typedef struct dataSocClients {
	int socket;		// Le socket du client
	struct sockaddr_in donneesClient;	// Les donnees (ip, port ...)
} dataSocClients;

typedef struct client {
	int id; 	// Position dans le tableau des clients du salon
	int salon; 	// id du salon
	dataSocClients dtCli;
	char* pseudo; // Le pseudo du client (length > 2)
} client;

typedef struct salon {
	int nbClients;
	int maxClients;
	char* nomSalon;
	pthread_mutex_t mutexSalon;
	client* tabClients; // Un tableau de client
} salon;

salon tabSalons[NB_MAX_SALONS];


salon EmptySalon;
client EmptyClient;


/* 
*	Declaration des Sockets. Ils doivent etre globaux pour que la fonction 
* 	"fermer" puisse y acceder (car elle peut etre declenchee par un CTRL + C) 
*/
int socketServeur; 

/* Structures contenant les donnees du serveur et des clients */
struct sockaddr_in adServ;


/* == Mutex == **/
/* Mutex pour le numero du client dans le tableau : Cette valeur tres importante pour le thread du client 
* (position dans le tableau des sockets) etait remplacee avant d'arriver dans le thread, ce mutex resoud le probleme */
pthread_mutex_t mutex_dataSocClient;

/* == Semaphores == **/
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
*	Envoie la liste des salons au client et attend qu'il en choissise un valide
*	param :		int Socket 		Le socket du client qui se connecte.
*
*	return : 	numSalon 		L'indice du salon dans le tableau de Salon
*
*/

int getSalon (int socket) {
	int numSalon = -1;
	char reponse[TAILLE_BUFFER];
	int res = 0;
	int i = 0;
	sleep(0.1);

	while (numSalon <= 0 || numSalon > NB_MAX_SALONS) {
		char str[TAILLE_BUFFER] = "\n[MSG SERVEUR] ";
		strcat(str, "Voici la liste des salons : \n");

		char str2[100];
		
		for (i = 0; i < NB_MAX_SALONS; i++) {
			sprintf(str2, " (%d/%d)\n", tabSalons[i].nbClients, tabSalons[i].maxClients);
			strcat(str, tabSalons[i].nomSalon);
			strcat(str, str2);
		}
		strcat (str, "[END MSG SERVEUR]\n");

	
		envoi(socket, str);
		res = reception(socket, reponse, TAILLE_BUFFER);
		if (res == 0) {
			return -1;
		} else {
			numSalon = atoi(reponse);
			if (numSalon > 0 && numSalon <= NB_MAX_SALONS) {
				if (tabSalons[numSalon-1].nbClients > tabSalons[numSalon-1].maxClients) {
					numSalon = -1;
					printf("RESET\n");
				}
			}
		}
	}
	
	return numSalon-1;
}


/* 
*	Envoie le message a tout les clients
*	param : 	cl		client 		Le client qui envoie le message
*				char*	msg 		La chaine a envoyer

*	return : 	-1 si echec
*				1 si reussite
*/
int broadcast(client cl, char* msg) {
	salon s = tabSalons[cl.salon];

	for (int i = 0; i<s.maxClients; i++) {

		/* Envoie uniquement au clients connectes (socket != -1 sauf au client envoyeur) */
		if (i != cl.id && s.tabClients[i].id != -1) {
			if (envoi (s.tabClients[i].dtCli.socket, msg) == -1) {
				return -1;
			}
		}
	}
	return 1;
}

/*
*	Deconnecte le socket du client passé en parametre
* 	
*	param : 	cl 	client	Le client qui se déconnecte.
*/
void deconnecterSocket(client cl) {		
	
	/* Envoie "FIN" au client pour qu'il se deconnecte de son compte */
	envoi(cl.dtCli.socket, "/fin");
	shutdown (cl.dtCli.socket, 2);

	pthread_mutex_lock(&tabSalons[cl.salon].mutexSalon);

	tabSalons[cl.salon].tabClients[cl.id].id = -1;
	tabSalons[cl.salon].nbClients--;

	pthread_mutex_unlock(&tabSalons[cl.salon].mutexSalon);
	

	/* Libere un jeton dans le semaphore */
	sem_post(&semaphore_nb_clients);
}

/*
* 	Ferme les sockets et quitte le programme
*/
void fermer() {
	printf("\n\n* -- FERMETURE DU SERVEUR -- *\n");

	/* Ferme le socket serveur */
	int res = close(socketServeur);
	if (res == 0) {
		printf("Fermeture du socket serveur\n");
	} else {
		perror("Erreur fermeture  du socket serveur\n");
	}

	/* Ferme les semaphores */
	sem_destroy(&semaphore_nb_clients);
	pthread_mutex_destroy(&mutex_dataSocClient);


	int i = 0;
	for (i = 0; i<NB_MAX_SALONS; i++) {
		if (tabSalons[i].maxClients != -1) {
			pthread_mutex_destroy(&tabSalons[i].mutexSalon);
		}
	}

	printf("* -- FIN DU PROGRAMME -- *\n");

	exit(0);
}



/*
*	Transmet un message du client a tous les autres clients
*	Si ce client envoie fin ou se deconnecte, la fonction retourne directement -1
*	param : client 	cl		La structure du client
*	return : 
*			 -1 si erreur lors de la reception du message du client
			 1 Si le client se deconnecte ou envoie "FIN"
*			 0 sinon
*/
int envoi_reception (client cl) {
	/* Initialisation du buffer */
	char str[TAILLE_BUFFER];
	int res;
	int socEnvoyeur = cl.dtCli.socket;

	
	/* ---- RECEPTION ---- */
	/* Attend le message du client envoyeur */
	res = reception (socEnvoyeur, str, 0);

	/* Erreur lors de la reception */
	if (res < 0) {
		return -1;
	} else if (res == 0 || (strlen(str) == 4 && strncmp ("/fin", str, 4) == 0)) {
		return 1;
	}


	/* Ajout du pseudo dans le message avant de l'envoyer aux autres clients "[pseudo] message" */
	char msgClient[strlen(cl.pseudo) + strlen(str) + 10]; /* (+4) pour les 4 caracteres en plus ([] \0 ' ')*/
	strcpy(msgClient, "[");
	strcat(msgClient, cl.pseudo);
	strcat(msgClient, "] ");
	strcat(msgClient, str);

	/* ---- ENVOI ---- */
	/* Envoie le message aux autres clients */
	broadcast(cl, msgClient);
	

	return 0;

}


/*
*	Effectue la transition des message entre ce client et les autres clients
*	Transmet ensuite les messages de ce client a tous les autres
*
*	param : 	dataSocClients* 	dsc 	Le pointeur vers la structure qui contient les donnees du client
*
*/
void *conversation (dataSocClients *dsc) {
	/* init de variable et liberation du mutex sur dataSocClient */
	dataSocClients dtCli = *dsc;
	pthread_mutex_unlock(&mutex_dataSocClient);

	client cl;

	cl.dtCli = dtCli;

	/* Reccupere l'IP du client et l'affiche*/
	char ipclient[50];
	inet_ntop(AF_INET, &(dtCli.donneesClient.sin_addr), ipclient, INET_ADDRSTRLEN);	


	/* Reception du pseudo du client */
	char pseudo[TAILLE_PSEUDO];
	int resPseudo = reception(cl.dtCli.socket, pseudo, TAILLE_PSEUDO);

	if (resPseudo > 3){
		printf ("Client de pseudo %s d'ip %s connecte !\n\n", pseudo, ipclient);

		cl.pseudo = pseudo;

		/* Creation des donnees du client */
		envoi(cl.dtCli.socket, "BEGIN");

		int salon = getSalon(cl.dtCli.socket);
		if (salon >= 0) {
			
			cl.salon = salon;
			/* Selectionne un slot vide dans le tableau des salons */
			int i = 0;
			int position_tableau_salon = -1;
			while (i < tabSalons[salon].maxClients && position_tableau_salon == -1) {
				if (tabSalons[salon].tabClients[i].id == -1) {
					position_tableau_salon = i;
				}
		 		i++;
		 	}
		 	if (position_tableau_salon != -1) {
		 		/* cree la structure du client */
		 		cl.id = position_tableau_salon;

		 		/* Prends un jeton*/


		 		pthread_mutex_lock(&tabSalons[salon].mutexSalon);
				/* Ajoute le client au salon */
				tabSalons[salon].tabClients[position_tableau_salon] = cl;
				tabSalons[salon].nbClients++;

				/* libère le jeton */
				pthread_mutex_unlock(&tabSalons[salon].mutexSalon);

				int res = 0;
				char str[TAILLE_BUFFER];


				/* Envoie un message de connexion a tous les clients */
				sprintf(str, ">>>> %s s'est connecte", cl.pseudo);
				broadcast(cl, str);


				sprintf(str, "Bienvenue dans %s", tabSalons[cl.salon].nomSalon);
				envoi(cl.dtCli.socket, str);

				/* Boucle de la conversation */
				while(res == 0) {
					res = envoi_reception(cl);
				}

				/* Envoie le message de deconnexion a tous les clients */
				sprintf(str, "<<<< %s s'est deconnecte", cl.pseudo);
				broadcast(cl, str);		
			}
		}
	}

	/* Deconnecte et quitte le thread */
	deconnecterSocket (cl);
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
	if (pos == NULL) {
		pos = &ip[49];
	}	
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

	EmptySalon.maxClients = -1;
	EmptyClient.id = -1;


	/* Initialise le tableau des sockets à -1 : Lorsqu'une case est egale a -1, on peut l'ulitilser pour l'attribuer au prochain client */
	int i = 0;
	int j = 0;

	for (i = 0; i<NB_MAX_SALONS; i++) {
		char* nomSalon = (char*)malloc(30 * sizeof(char)); 
		sprintf(nomSalon, "Salon n°%d", i+1);

		client* cl = (client*)malloc(NB_MAX_CLIENTS_SALONS_DEFAULT* sizeof(client));

		pthread_mutex_t mutexSalon;
		pthread_mutex_init(&mutexSalon,0);

		tabSalons[i] = (salon) {0, NB_MAX_CLIENTS_SALONS_DEFAULT, nomSalon, mutexSalon, cl};

		for (j=0; j<NB_MAX_CLIENTS_SALONS_DEFAULT; j++) {
			tabSalons[i].tabClients[j].id = -1;
		}

	}


	printf("Salons Initialises\n");


	/* Init des mutex et semaphores */
	pthread_mutex_init(&mutex_dataSocClient,0);
	sem_init(&semaphore_nb_clients, 0, NB_MAX_CLIENTS);

	printf("Debut\n");

	/* --- BOUCLE PRINCIPALE --- */
	while (1) {
		sem_wait(&semaphore_nb_clients);
		
		/* Lock du mutex pour numClient */
		pthread_mutex_lock(&mutex_dataSocClient);

		dataSocClients dtCli;
		
		printf ("Attente de connexion d'un client\n");
		int resConnexion = attenteConnexion(&dtCli.socket, &dtCli.donneesClient);


		if (resConnexion == 0) {
			/* Cree un thread dedie pour ce client */
			pthread_t thread;
			pthread_create(&thread, 0, (void*) conversation, &dtCli);
		} else {
			perror ("Erreur lors de la connexion au client\n");
			// deconnecterSocket(numClient);
		}


	}

	/* Ferme le serveur */
	fermer();
	return 0;
}
