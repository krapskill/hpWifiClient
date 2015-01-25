hpWifiClient
============

Voii le code source de notre projet de haut-parleur wifi partie client.
Un client représente un Haut-parleur avec ampli intégré et une Raspberry-pi.

Avant de pouvoir compiler le code :
- installer linux raspbian sur la rapsberry
- installer les paquets nécessaires (cités ci-dessous)
- configurer le fichier /etc/ntp.conf 
- executer les lignes de compilations
- executer la ligne d'execution

VERSION DE LINUX UTILISÉE POUR LA RASPBERRY :
- http://downloads.raspberrypi.org/raspbian_latest

CONFIGURATION DU FICHIER DE CONF NTP :
Voilà quelques liens utiles pour apprendre à configurer le fichier en question.
- http://www.linux-france.org/prj/edu/archinet/systeme/ch58s03.html
- http://www.it-connect.fr/configurer-un-client-ntp-sous-linux/
- http://www.tldp.org/LDP/sag/html/basic-ntp-config.html
- http://doc.ubuntu-fr.org/ntp


LIGNES DE COMPILATION DU CODE : 

>g++ -std==gnu++0x -c easywsclient.cpp -o easywsclient.o

>g++ -std==gnu++0x -c main.cpp -o main.o

>g++ main.o -pthread -lasound easywsclient.o -o monProgramme


LIGNE D'EXECUTION :
>./monProgramme


LIBRAIRIES ET PAQUETS UTILISÉES :
- pthread pour la gestion multithread
>sudo apt-get install pthread

- lasound pour l'ultilisation d'ALSA sur linux
>sudo apt-get install libasound2-dev

- easywsclient pour le webScoket en C++
dispo dans la repository, il faut juste avoir les sources dans le même dossier
