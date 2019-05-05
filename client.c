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
#include <dirent.h>
#include <time.h> 

/* Definition de constantes */
#define PORT 2500
#define TAILLE_BUFFER 2048
#define TRUE 1
#define FALSE 0
#define IN 1
#define OUT 0

char* IP = "127.0.0.1";
char* DOSSIER_ENVOI_FICHIERS = "./BoiteEnvoi";
char* DOSSIER_RECEPTION_FICHIERS = "./FichiersRecus";

/*	
*	Declaration du Socket et de la structure. Ils doivent etre globaux pour que la fonction 
* 	"fermer" puisse y acceder (car elle peut etre declenchee par un CTRL + C) 
*/
int socketClient; 
struct sockaddr_in adServ;

/* Derniere commande recue */
char lastCmd[TAILLE_BUFFER];

/* Cette variable assure la communication entre les deux threads */
int fin;
pthread_mutex_t mutex_fin;
pthread_mutex_t mutex_lastCmd;



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
int envoiUDP(int socket, char* msg, struct sockaddr_in *donneesReceveur);
int receptionUDP(int socket, char* msg, int tailleMsg, struct sockaddr_in *donneesEnvoyeur);
void *envoiFichier();
void *receptionFichier();
void initTransfertFichier(int socUDP, struct sockaddr_in *donneesCorrespondant);
int commande (char* msg, int in_Out);

/* 
* Ferme le socket et quitte le programme 
*/
void fermer() {
	printf("\n* -- FIN -- *\n");
	close(socketClient);
	pthread_mutex_destroy(&mutex_fin);
	pthread_mutex_destroy(&mutex_lastCmd);
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
int envoiUDP(int socket, char* msg, struct sockaddr_in *donneesReceveur) {
	socklen_t lgA = sizeof(struct sockaddr_in);
	return (int) sendto(socket, msg, strlen(msg)+1, 0, (struct sockaddr*) donneesReceveur, lgA);
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
int receptionUDP(int socket, char* msg, int tailleMsg, struct sockaddr_in *donneesEnvoyeur) {
	socklen_t lgA = sizeof(struct sockaddr_in);
	return (int) recvfrom (socket, msg, tailleMsg, 0, (struct sockaddr*) donneesEnvoyeur, &lgA);
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

	if (msg[0] != '/') {
		/* Envoi */
		envoi = envoiMessage(msg);
		if (envoi < 0) {
			return -1;
		}
	}

	return 0;


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
* 	Initialise le socket UDP pour le transfert du fichier avec le client correspondant et remplis la structure passee en parametre
*	de l'adresse ip et du port du correspondant.
*	Dans cette fonction, le client va envoye le port sur lequel il est connecte au serveur, et le serveur lui enverra le
*	port sur lequel son correspondant est connecte.
*	param :		int 				socUDP					Le socket initialise (PF_INET, SOCK_DGRAM, 0)
*				struct sockaddr_in	*donneesCorrespondant	La structure ou seront stockees les donnees du correspondant
*	prereq :	La commande /FILERECV [IP] ou /FILESEND [IP] a ete enregistree dans lastCmd
*/ 
void initTransfertFichier(int socUDP, struct sockaddr_in *donneesCorrespondant) {
	int res = -1;
	int port;
	struct sockaddr_in donneesLocal;


	/* Parametre un timeout de 2 secondes si pas de reponse */
	struct timeval tv;
	tv.tv_sec = 2;
	tv.tv_usec = 0;
	setsockopt(socUDP, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);

	/* Reception du message contenant l'IP du correspondant */
	pthread_mutex_lock(&mutex_lastCmd);
	char ipCorrespondant[strlen(&lastCmd[10])]; // Recupere l'ip du correspondant
	strcpy (ipCorrespondant, &lastCmd[10]);

	/* Donnees de l'envoyeur */
	donneesLocal.sin_family = AF_INET;	/* IPV4 */
	donneesLocal.sin_addr.s_addr = INADDR_ANY;
		
	srand(time(NULL));
	while (res == -1) {
		port = 10000 + (rand() % 15000); // Le port sera un random entre 10 000 et 25 000
		donneesLocal.sin_port = htons(port);	/* Port */
		res = bind (socUDP, (struct sockaddr*) &donneesLocal, sizeof(donneesLocal));
	}

	/* Envoie le port au serveur */
	char str[TAILLE_BUFFER];
	sprintf(str, "/FILEPORT %d", port);
	envoiMessage(str);

	/* Reception du message contenant le port du correspondant */
	pthread_mutex_lock(&mutex_lastCmd);
	char port_distant[25];
	strcpy(port_distant, &lastCmd[6]); // Recupere le port

	/* Remplis les donnees du Correspondant */
	(*donneesCorrespondant).sin_family = AF_INET;	/* IPV4 */
	inet_pton(AF_INET, ipCorrespondant, &((*donneesCorrespondant).sin_addr));
	(*donneesCorrespondant).sin_port = htons(atoi(port_distant));

}

/*
*	Fonction d'envoi de fichier.
*	Demande a l'utilisateur quel fichier il souhaite envoyer, puis le transfert au client correspondant
*	prereq :	La commande /FILERECV [IP] ou /FILESEND [IP] a ete enregistree dans lastCmd
*/
void *envoiFichier() {
	/* Init du socket en UDP */
	int socUDP = socket(PF_INET, SOCK_DGRAM, 0);
	struct sockaddr_in adCorrespondant;


	/* Reccupere les donnees du correspondant */
	initTransfertFichier(socUDP, &adCorrespondant);

	/* == Selection du fichier == */
	int fileSelected = FALSE;
	FILE *fichier;
	char fileName[TAILLE_BUFFER];
	char filepath[TAILLE_BUFFER];
	while (fileSelected == FALSE) {
		/* Affiche la liste des fichiers */
		DIR *dp;
		struct dirent *ep;     
		dp = opendir (DOSSIER_ENVOI_FICHIERS);
		if (dp != NULL) {
			printf("[ENVOI FICHIER] Voilà la liste de fichiers :\n");
			while ((ep = readdir (dp))) {
				if(strcmp(ep->d_name,".")!=0 && strcmp(ep->d_name,"..")!=0) {
					printf("%s\n",ep->d_name);
				}
			}    
			(void) closedir (dp);
		}else {
			perror ("Ne peux pas ouvrir le répertoire");
		}

		/* Demande le fichier a envoyer */
		printf("[ENVOI FICHIER] Indiquer le nom du fichier en tapant \"/FILESELECT nomDuFichier\": \n");
	  	pthread_mutex_lock(&mutex_lastCmd);

	  	strcpy(fileName, &lastCmd[12]); // Recupere le nom du fichier tape par l'utilisateur

	  	strcpy(filepath, DOSSIER_ENVOI_FICHIERS);
	  	strcat(filepath, "/");
	  	strcat(filepath, fileName);		

		fichier = fopen(filepath, "r");
		if (fichier == NULL){
			printf("[ENVOI FICHIER] Ce fichier n'existe pas : %s\n", fileName);
		} else {
			fileSelected = TRUE;
		}
	}

	/* == Envoi du fichier =*/

	printf("\n[ENVOI FICHIER] Envoi du fichier : %s\n", fileName);
	int ack = 0;
	char contenu[TAILLE_BUFFER];
	char message[TAILLE_BUFFER + 15];
	char retourACK[5] = "ACK1";

	/* Envoi du nom du fichier */
	while (atoi(&retourACK[3]) != ack) {
		sprintf(contenu, "%d%s", ack, fileName);
		envoiUDP(socUDP, contenu, &adCorrespondant);
		receptionUDP(socUDP, retourACK, 4, &adCorrespondant); // Message de confirmation de reception du message
	}

	ack = (ack + 1) % 10;

	/* Lecture et envoi du fichier */
	// Lire et afficher le contenu du fichier
	while (fgets(contenu, TAILLE_BUFFER, fichier) != NULL) {
		/* Envoi du contenu du fichier */
		while (atoi(&retourACK[3]) != ack) {
			sprintf(message, "%d%s", ack, contenu);
			envoiUDP(socUDP, message, &adCorrespondant);
			receptionUDP(socUDP, retourACK, 4, &adCorrespondant);
		}
		ack = (ack + 1) % 10;
	}

	envoiUDP(socUDP, "/#FINDUFICHIER", &adCorrespondant);


	printf("[SYSTEME] Le fichier a bien ete envoye\n");
	/* Ferme le fichier */
	fclose(fichier);

	/* Quitte le thread */
	pthread_exit(0);
}


/*
*	Fonction de reception de fichier.
*	Recoit un fichier depuis le correspondant et l'enregistre
*	La commande /FILERECV [IP] ou /FILESEND [IP] a ete enregistree dans lastCmd
*/
void *receptionFichier() {
	/* Init d'un socket en UDP */
	int socUDP = socket(PF_INET, SOCK_DGRAM, 0);
	struct sockaddr_in adCorrespondant;

	/* Reccupere les donnees du correspondant */
	initTransfertFichier(socUDP, &adCorrespondant);

	int ack = 0;
	char contenu[TAILLE_BUFFER + 10] = "2";
	char retourACK[5];

	/* Reception du nom du fichier */
	while (atoi(&contenu[0]) != ack) {
		receptionUDP(socUDP, contenu, TAILLE_BUFFER+10, &adCorrespondant); // Message de confirmation de reception du message
	}
	sprintf(retourACK, "ACK%d", ack);
	envoiUDP(socUDP, retourACK, &adCorrespondant);
	ack = (ack + 1) % 10;

	/* Ouverture du fichier */
	FILE *fichier;

	char fileName[strlen(&contenu[1]) + strlen(DOSSIER_RECEPTION_FICHIERS) + 1];
	strcpy(fileName, DOSSIER_RECEPTION_FICHIERS);
	strcat(fileName, "/");
	strcat(fileName, &contenu[1]);

	fichier = fopen(fileName, "w+");

	if (fichier == NULL){
		printf("Erreur dans la creation du fichier %s\n", fileName);
	} else {

		/* Reception et ecriture du fichier */
		// Lire et afficher le contenu du fichier
		while (receptionUDP(socUDP, contenu, TAILLE_BUFFER+1, &adCorrespondant) > 0 && strcmp("/#FINDUFICHIER", contenu) != 0) {
			if (atoi(&contenu[0]) == ack) {
				fputs(&contenu[1], fichier);
				sprintf(retourACK, "ACK%d", ack);
				envoiUDP(socUDP, retourACK, &adCorrespondant);
				ack = (ack + 1) % 10;
			} else {
				sprintf(retourACK, "ACK%d", (ack-1 %10));
				envoiUDP(socUDP, retourACK, &adCorrespondant);
			}

		}

		printf("[SYSTEME] Vous avez recu un fichier : %s\n", fileName);
	}
	/* Ferme le fichier */
	fclose(fichier);

	/* Quitte le thread */
	pthread_exit(0);
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

/*
*	Analyse et traite ou envoie la commande :
*		/fin 		Termine la connexion
*		/FILESEND	Initialise la
*/
int commande (char* msg, int in_Out) {
	strcpy(lastCmd, msg);
	pthread_mutex_unlock(&mutex_lastCmd);

	char message[strlen(msg)];
	strcpy (message, msg);
	char *cmd = strtok(message, " ");

	if (in_Out == OUT) {
		if (strlen(cmd) != strlen("/FILESELECT") && strncmp("/FILESELECT", cmd, strlen("/FILESELECT")) != 0) {
			envoiMessage(msg);
		}
	}

	if (strlen(msg) == 4 && strncmp("/fin", msg, 4) == 0){
		decoThread(0);
	} else if (strlen(cmd) == strlen("/FILESEND") && strncmp("/FILESEND", cmd, strlen("/FILESEND")) == 0) {
		pthread_t sendFile;
		pthread_create(&sendFile, 0, envoiFichier, 0);

	} else if (strlen(cmd) == strlen("/FILERECV") && strncmp("/FILERECV", cmd, strlen("/FILERECV")) == 0) {		
		pthread_t recvFile;
		pthread_create(&recvFile, 0, receptionFichier, 0);
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
				commande(msg, IN);
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
				commande(msg, OUT);
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
		pthread_mutex_init(&mutex_lastCmd, 0);
		pthread_mutex_lock(&mutex_lastCmd);

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
