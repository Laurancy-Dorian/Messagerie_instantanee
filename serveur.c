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

/* Definition de constantes */
#define NB_CLIENTS 2

/* 
*	Declaration des Sockets. Ils doivent etre globaux pour que la fonction 
* 	"fermer" puisse y acceder (car elle est declenchee par un CTRL + C) 
*/
int socketServeur; 
int socketClients[NB_CLIENTS];

/* Structures contenant les donnees du serveur et des clients */
struct sockaddr_in adServ;
struct sockaddr_in adClient[NB_CLIENTS]; 

socklen_t lgA = sizeof(struct sockaddr_in);	


/*
* Ferme les sockets et quitte le programme
*/
void fermer() {
	int i;

	/* Ferme tous les sockets clients */
	for (i = 0; i < NB_CLIENTS; i++) {
		int res = close (socketClients[i]);
		if (res == 0) {
			printf("\nFermeture du socket client numero %d\n", i);
		} else {
			printf("\nErreur fermeture du socket client numero %d\n", i);
			perror(NULL);
		}
	}

	/* Ferme le socket serveur */
	int res = close(socketServeur);
	if (res == 0) {
		printf("\nFermeture du socket serveur\n");
	} else {
		perror("\nErreur fermeture  du socket serveur\n");
	}

	exit(0);
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

	/* Ecoute jusqu'a 10 clients*/
	res = listen (socketServeur, 10);
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
int attenteConnexion(int* socClient, struct sockaddr* donneesClient) {
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
*	Demande a l'utilisateur d'ecrire un message et l'envoie
*	param : 	int 	socClient 	Le socket du client ou envoyer les donnees
*	return : 	0 en cas de reussite, -1 sinon
*/
int envoi (int socClient) {

	return 0;
}

/*
*	Attend une reponse du client et la stocke dans le second parametre
*	param : 	int 	socClient 	Le socket du client qui envoie le message
*				char*	msg 		Le pointeur de la chaine de caracteres ou stocker le message
*				int 	taille		Le nombre d'octets a lire
*
*	return : 	-1 si echec
*				0 si le client s'est deconnecte normalement
*				Le nombre d'octets lus sinon
*/
int reception (int socClient, char* msg, int taille) {

	return 0;
}


/*
*	Attribue l'ordre des clients en envoyant "NUM1" au premier et "NUM2" au deuxieme
*	
*	param : 	int 	socClient1 	Le socket du client qui parlera en 1er
*				int 	socClient2 	Le socket du client qui parlera en 2eme
*
*	return :	0 si ok, -1 si ko
*/
int attribuerOrdre(int socClient1, int socClient2) {

	return 0;
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
*				int 	socClient2 	Le socket du client qui parlera en 2eme
*
*	return : 	0 si le premier client s'est deconnecte
*				1 si le deuxieme client s'est deconnecte
*				-1 si erreur
*
*/
int conversation (int socClient1, int socClient2) {

	return 0;
}


/* TODO : Prendre en compte la deconnexion du client pendant l'attente du 2eme client */
int main (int argc, char *argv[]) {
	/* Ferme proprement le socket si CTRL+C est execute */
	signal(SIGINT, fermer);

	return 0;
}