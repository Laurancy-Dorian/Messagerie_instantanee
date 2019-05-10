########################################
#~ définitions
########################################

# nom de l'executable
#BIN=runPeriod

BIN=bin/client  bin/serveur
#BIN=bin/serveur

# liste des fichiers sources 
SRCCli=client.c
SRCServ=serveur.c

default: $(BIN)

########################################
#~ regles pour l'executable
########################################

obj/%.o: %.c
	gcc -Wall -Wcomment -Iinclude -c $< -o $@

bin/client: $(SRCCli:%.c=obj/%.o)
	gcc -o $@ $+ -lpthread 

bin/serveur: $(SRCServ:%.c=obj/%.o)
	gcc -o $@ $+ -lpthread 



clean:
	rm -f $(BIN) obj/*.o *~
