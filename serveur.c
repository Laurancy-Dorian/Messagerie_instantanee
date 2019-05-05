/************************************************
*												*
* 	MESSAGERIE INSTANTANEE - SERVEUR - v3		*
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
#define NB_MAX_CLIENTS 15
#define TAILLE_BUFFER 2048
#define TAILLE_PSEUDO 128
#define NB_COMMANDES 5
#define TRUE 1
#define FALSE 0


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


/* == Mutex == **/
/* Mutex pour le numero du client dans le tableau : Cette valeur tres importante pour le thread du client 
* (position dans le tableau des sockets) etait remplacee avant d'arriver dans le thread, ce mutex resoud le probleme */
pthread_mutex_t mutex_numClient;

/* == Semaphores == **/
/* Ce semaphore permet de ne plus accepter de nouveaux clients lorsqu'il y a autant de clients que NB_MAX_CLIENTS */
sem_t semaphore_nb_clients; 

/* == Commandes == */
/* Structure contenant les commandes */
typedef struct st_cmd {
	char* cmd;		// La chaine de caracteres representant la commande
	void *fonction;	// Le pointeur vers la fonction associee a la commande
} st_cmd;

/* Commandes lors d'echanges d'informations entre le serveur et le client. L'utilisateur n'est pas cense les utiliser */
char* commandes_systeme[] = {"/FILESEND", "/FILERECV", "/FILEPORT", "/FILE"};

/* Enregistre la derniere commande de chaque client et protege la lecture par un mutex */
char lastCmd[NB_MAX_CLIENTS][TAILLE_BUFFER];
pthread_mutex_t mutexCmd[NB_MAX_CLIENTS];


/* == Declaration des fonctions == */
void help(int numSocket);
void fin(int numSocket);
void file(int numSocket);
void file_traitements(int* numSoc);
void friends(int numSocket);
int initialisation (int port);
int attenteConnexion(int* socClient, struct sockaddr_in *donneesClient);
int envoi (int socClient, char* msg);
int reception (int socClient, char* msg, int taille);
int broadcast(int numClient, char* msg);
void deconnecterSocket(int numSocket);
void fermer();
int envoi_reception (int numSocEnvoyeur);
void *conversation (int* numcli);
void ipServeur(char* ip);

/* Liste des commandes */
st_cmd commandes[NB_COMMANDES] = {
	{"/help", &help},
	{"/fin", &fin},
	{"/file", &file},
	{"/friends", &friends},
	{"/", &help}
};


/* 
*	Deconnecte ce client du salon
*	param : 	int 	numSocket	Le numero dans le tableau des sockets du client qui effectue cette commande
*/ 
void fin (int numSocket) {
	char str[TAILLE_BUFFER];

	/* Envoie le message de deconnexion a tous les clients */
	sprintf(str, "<<<< %s s'est deconnecte", pseudoClients[numSocket]);
	broadcast(numSocket, str);

	/* Deconnecte et quitte le thread */
	deconnecterSocket (numSocket);

	lastCmd[numSocket][0] = '\0';
	pthread_exit(0);
}

/* 
*	Envoie la liste des commandes disponibles au client
*	param : 	int 	numSocket	Le numero dans le tableau des sockets du client qui effectue cette commande
*/ 
void help (int numSocket) {
	char str[TAILLE_BUFFER] = "\n[MSG SERVEUR] ";
	strcat(str, "Voici la liste des commandes : \n");

	int i;
	for (i = 0; i < NB_COMMANDES-1; i++) {
		strcat(str, commandes[i].cmd);
		strcat(str, "\n");
	}
	strcat (str, "[END MSG SERVEUR]\n");

	envoi(socketClients[numSocket], str);
	lastCmd[numSocket][0] = '\0';
}


/* 
*	Envoie la liste des pseudos des correspondants au client
*	param : 	int 	numSocket	Le numero dans le tableau des sockets du client qui effectue cette commande
*/ 
void friends (int numSocket) {
	char str[TAILLE_BUFFER] = "\n[MSG SERVEUR] Voici la liste des amis :\n";

	int i;
	for (i = 0; i < NB_MAX_CLIENTS; i++) {
		if (socketClients[i] != -1 && i != numSocket) {
			strcat(str, pseudoClients[i]);
			strcat(str, "\n");
		}
	}
	strcat (str, "[END MSG SERVEUR]\n");

	envoi(socketClients[numSocket], str);
	lastCmd[numSocket][0] = '\0';

}


/* 
*	Execute les traitements de la commande /file
*	Permet de communiquer aux clients leurs ports et adresse ip
*	param : int*	numSocket 	Le numero dans le tableau des sockets du client emeteur
*	
*	prereq : Cette fonction doit etre lancee par la fonction file
*			 La case lastCmd[numSoc] doit etre remplie par la commande /file... du client
*/
void file_traitements(int* numSoc) {
	int numSocket = numSoc[0];
	int numSocket2 = numSoc[1];
	/* Attend la reponse du client */
	int ok = FALSE;
	while (ok == FALSE) {
		pthread_mutex_lock(&mutexCmd[numSocket]);
		if (strncmp(lastCmd[numSocket], "/FILEPORT", 9) != 0) {
			pthread_mutex_unlock(&mutexCmd[numSocket]);
		} else {
			ok = TRUE;
		}
	}

	/* On reccupere le port */
	char cmd[TAILLE_BUFFER];
	strcpy(cmd, lastCmd[numSocket]);
	char* port = &cmd[10];

	/* Envoi du message */
	char str[TAILLE_BUFFER];
	strcpy (str, "/FILE ");
	strcat (str, port);
	envoi(socketClients[numSocket2], str);

	/* Le client correspondant a toutes les informations qu'il faut pour echanger un fichier, ce thread se termine */
	lastCmd[numSocket][0] = '\0';
	pthread_exit(0);
}

/* 
*	Commande File : cree un thread par client pour le traitement de cette commande (voir file_traitement)
*	Cette fonction est une commande, il faut donc qu'elle ait ete appelee par la fonction commande
* 	param :		int 	numSocket	Le numero dans le tableau des sockets du client quilance cette commande
*	prereq : 	Le tableau lastCmd[numSocket] a ete rempli par la commande de l'utilisateur
*/
void file (int numSocket) {
	pthread_mutex_lock(&mutexCmd[numSocket]);

	char cmd[TAILLE_BUFFER];
	strcpy(cmd, lastCmd[numSocket]);
	

	/* Reccupere le nom du client */
	char* nomClient = strtok(cmd, " ");
	nomClient = strtok(NULL, " ");


	/* Verifie si le pseudo correspond a un de ceux dans la liste */
	int i = 0;
	int found = FALSE;
	if (nomClient != NULL) {
		while (found == FALSE && i < NB_MAX_CLIENTS) {
			if (socketClients[i] != -1 && i != numSocket && strcmp(nomClient, pseudoClients[i]) == 0) {
				found = TRUE;
			} else {
				i++;
			}
			
		}
	}

	/* Si le pseudo ne correspond pas : Affiche la liste des amis au client */
	if (found == FALSE) {
		envoi(socketClients[numSocket], "[MSG SERVEUR] Ce pseudo ne correspond a aucun client connecte [END MSG SERVEUR]\n");
		sleep(0.5);
		friends(numSocket);
		lastCmd[numSocket][0] = '\0';
	} else {
		int numReceveur = i; 
		printf("Mise en relation de %s et %s pour un transfert de fichiers\n",pseudoClients[numSocket], pseudoClients[numReceveur]);

		/* Initie le lancement du transfert de fichier */

		/* Reccuperation des IP */
		char ipEnvoyeur[50];
		inet_ntop (AF_INET, &(adClient[numSocket].sin_addr), ipEnvoyeur, INET_ADDRSTRLEN);	

		char ipReceveur[50];
		inet_ntop (AF_INET, &(adClient[numReceveur].sin_addr), ipReceveur, INET_ADDRSTRLEN);	

		/* Envoie /FILESEND [IP] au client envoyeur */
		char str[TAILLE_BUFFER] = "/FILESEND ";
		strcat(str, ipReceveur);
		envoi(socketClients[numSocket], str);

		/* Envoie /FILERECV [IP] au client recepteur */
		strcpy(str, "/FILERECV ");
		strcat(str, ipEnvoyeur);
		envoi(socketClients[numReceveur], str);


		/* Cree un thread par client pour envoyer les informations sur les ports au clients */
		pthread_t t1; 
		int tab[] = {numSocket, numReceveur};
		pthread_create(&t1, 0, (void*) &file_traitements, &tab);

		pthread_t t2; 
		int tab2[] = {numReceveur, numSocket};
		pthread_create(&t2, 0, (void*) &file_traitements, &tab2);
	}

}


/*
*	Effectue la commande pour ce client
*	param : char* 	str 	 			La commande a effectuer
*			int 	numSocEnvoyeur		Le NUMERO du client envoyeur dans le tableau des sockets (pas le descripteur)
*	return : 
*			 -1 si erreur
*			 0 si ok
*/
int commande(char* str, int numSocEnvoyeur) {	
	/* Enregistre la commande dans le tableau des commandes */
	strcpy(lastCmd[numSocEnvoyeur], str);
	pthread_mutex_unlock(&mutexCmd[numSocEnvoyeur]);

	char commande[strlen(str)];
	strcpy(commande, str);
	char* cmd = strtok(commande, " ");

	/* Execute la commande si elle existe */
	int i = 0;
	int commande_executee = FALSE;
	while (i < sizeof(commandes)/sizeof(commandes[0]) && commande_executee == FALSE) {
		
		if (strlen(cmd) == strlen(commandes[i].cmd) && strncmp(commandes[i].cmd, cmd, strlen(commandes[i].cmd)) == 0) {
			void (*f)(int) = commandes[i].fonction;
			(*f)(numSocEnvoyeur);
			commande_executee = TRUE;
		}
		i++;
	}

	/* Verifie si cette "commande" n'est pas une commande systeme */
	i = 0;
	int est_cmd_sys = FALSE;
	if (commande_executee == FALSE) {
		while (i < sizeof(commandes_systeme)/sizeof(commandes_systeme[0]) && est_cmd_sys == FALSE) {
			if (strlen(cmd) == strlen(commandes_systeme[i]) && strncmp(commandes_systeme[i], cmd, strlen(commandes_systeme[i])) == 0) {
				est_cmd_sys = TRUE;
			}
			i++;
		}
	}
	
	/* Si cette commande n'est pas referencee dans les commandes disponibles, alors affiche la liste des commandes a l'utilisateur */
	if (commande_executee == FALSE && est_cmd_sys == FALSE) {
		help(numSocEnvoyeur);
	}
	
	return 0;
}

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
			envoi(socketClients[i], "/fin");
			shutdown (socketClients[i], 2);
			socketClients[i] = -1;
			pseudoClients[i][0] = '\0';
			pthread_mutex_unlock(&mutexCmd[numSocket]);
			lastCmd[i][0] = '\0';

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

	int i = 0;
	for (i = 0; i<NB_MAX_CLIENTS; i++) {
		pthread_mutex_destroy(&mutexCmd[i]);
	}

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
	} else if (res == 0) {
		strcpy(str, "/fin");
	}

	if (strncmp ("/", str, 1) == 0) {
		commande(str, numSocEnvoyeur);
	} else {
		/* Ajout du pseudo dans le message avant de l'envoyer aux autres clients "[pseudo] message" */
		char msgClient[strlen(pseudoClients[numSocEnvoyeur]) + strlen(str) + 4]; /* (+4) pour les 4 caracteres en plus ([] \0 ' ')*/
		strcpy(msgClient, "[");
		strcat(msgClient, pseudoClients[numSocEnvoyeur]);
		strcat(msgClient, "] ");
		strcat(msgClient, str);


		/* ---- ENVOI ---- */
		/* Envoie le message aux autres clients */
		broadcast(numSocEnvoyeur, msgClient);
	}

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


	/* Initialise le tableau des sockets à -1 : Lorsqu'une case est egale a -1, on peut l'ulitilser pour l'attribuer au prochain client */
	int i = 0;
	for (i = 0; i < NB_MAX_CLIENTS; i++) {
		socketClients[i] = -1;
	}

	/* Init des mutex et semaphores */
	pthread_mutex_init(&mutex_numClient,0);
	sem_init(&semaphore_nb_clients, 0, NB_MAX_CLIENTS);
	for (i = 0; i<NB_MAX_CLIENTS; i++) {

		pthread_mutex_init(&mutexCmd[i], 0);
		pthread_mutex_lock(&mutexCmd[i]);
	}

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
