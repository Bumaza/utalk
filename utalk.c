/*
 * utalk.c - Jednoduchy UDP talker
 * author: Jozef Budac 
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <termios.h> 
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdbool.h>

#define BUFFER_SIZE 1024
#define REC_MESS_SIZE 1088 //1024 + 64 
#define STDIN 0
#define ENTER 10
#define BACKSPACE  0x7f
#define ETX 4


/* Moje nastavenia*/
char buffer[BUFFER_SIZE]; // pre nacitavanie sprav
char message[BUFFER_SIZE]; // pre ulozenie rozpsinej spravy
char recMessage[BUFFER_SIZE][REC_MESS_SIZE]; // pre ulozenie prijatych sprav
int index_message = 0;
int index_recieve;

//funkcie
void sendMessage();
void receiveMessage();
char *getTime();
void turnOffCannon();
void errorMessage();


void errorMessage(char *s){
	printf("%s\n", s);
	exit(1);
}

void turnOffCannon(){
	struct termios termattr;
	if (tcgetattr(0, &termattr) < 0) perror("Chyba: tcgetattr");
	 
	termattr.c_lflag &= ~ICANON;
	if (tcsetattr(0, TCSANOW, &termattr) < 0) perror("Chyba: tcsetattr");
}

char *getTime(){

	time_t mytime = time(NULL);
    char * time_str = ctime(&mytime);
	time_str[strlen(time_str)-1] = '\0';
    return time_str;
}


void sendMessage(int socket_fd){
	int precitane, posli;
	precitane = read(STDIN, buffer, BUFFER_SIZE-1);

	if(precitane < 0) errorMessage("Chyba pri citani.");

	//hadnle input
	int index_citaj = 0;
	while(index_citaj < precitane){
		int asciiChar = (int) buffer[index_citaj];   //prvy znak zo vstupu - citame po znakoch lebo kanonicky mod je off

		if(asciiChar == ENTER || asciiChar == ETX){ //enter alebo ctrl-d bolo stalcene tak odosli spravu
			message[++index_message] = 0;
			posli = send(socket_fd, message, index_message-1, 0);
			if(posli == -1) errorMessage("Chyba pri posielani.");
			//vynulujeme index
			index_message = 0; //vynuluj index_message ..

			//pozri sa ci medzi casom nerpisli spravicky
			for(int i = 0; i < index_recieve; i++){
				for(int j = 0;j<REC_MESS_SIZE;j++){
					if(recMessage[i][j]==0){
						printf("\n"); //skonci (lebo bol enter )
						break;
					}else{
						printf("%c",recMessage[i][j]);
					}
				}
			}
			index_recieve = 0; //vypisal si vsetky spravy vynuluj index prijatych sprav

		}else if(asciiChar == BACKSPACE){ //bol stalceny backspace
			if(index_message == 0) return; //nemame co mazat uz 
			index_message--;
			write(1, "\b\b\b   \b\b\b",9); //vymaze backspace(^?) a dalsi znak pred vypiseme pradzne a vratime sa znova spat
		}else if(asciiChar >= 32 && asciiChar <= 126){ // 32 - 126 normalne znaky ... ASCI tabulka
			message[index_message++] = buffer[index_citaj];
		}else{ //TOOD... vsetko ostanne ohedlovat podla potreby

		}

		index_citaj++;
	}

	
	
}

void receiveMessage(int socket_fd){

	int precitane;

	struct sockaddr_in odkoho;
	socklen_t adrlen = sizeof(odkoho);

	precitane = recvfrom(socket_fd, buffer, BUFFER_SIZE-1, 0, (struct sockaddr*)&odkoho, &adrlen);
	if(precitane < 0){errorMessage("Chyba pri citani.");}

	//do buffra este vlozime novy riadok a ukoncenie stringu
	buffer[precitane] = 0;

	char *adresa = inet_ntoa(odkoho.sin_addr);
	char *tstr = getTime();
	int len = strlen(tstr);

	if(index_message > 0){ //uzivatel nieco pise nevyrusuj ho, vypis potom
		if(index_recieve >= BUFFER_SIZE) errorMessage("Privela prijatych sprav");

		//naformatujeme do spravy cas
		recMessage[index_recieve][0] = '[';
		for(int i = 1; i <= len; i++){
			recMessage[index_recieve][i] = tstr[i-1];
		}
		recMessage[index_recieve][++len] = ']';
		recMessage[index_recieve][++len] = ':';
		recMessage[index_recieve][++len] = ' ';
		len++;

		//pridame spravu do buffra prijatych sprav
		for(int i = len; i < len+precitane; i++){
			recMessage[index_recieve][i] = buffer[i-len];
		}
		index_recieve++; //incrementuj pocet prijaych sprav

	}else{ //uzivtalen nic nepise tak mu to mozes rovno poslat na STDOUT
		printf("[%s]: ",tstr); //vypis casu
		for(int i = 0; i < precitane; i++){ //vypis spravy
			printf("%c",buffer[i]);
		}
		printf("\n");
	}

}

/**  priklad zavolania...
*   ./utalk 127.0.0.1 1195 1196 
*   ./utalk 127.0.0.1 1196 1195
**/
int main(int argc, char ** args){

	//premenen
	int socket_fd, his_port = 2041, my_port = 2042;
	char *adress;
	fd_set check_fd;

	/* 
	 *Osterenie vstupnych arguentov
	*/

	//ocakavame partnerovu adresu
	if (argc > 1) if ( strcmp(args[1],"-") != 0){
    	adress = args[1];
    	
  	}

  	//ocakavame partnerov port
  	if(argc > 2){
  		int port = atoi(args[2]);
  		his_port = port != 0 ? port : his_port;
  	}

  	//ocakavame nas port
  	if(argc > 3){
  		int port = atoi(args[3]);
  		my_port = port != 0 ? port : my_port;
  	}

  	//vypneme kanoncky mod
  	turnOffCannon();

  	//vytvorime socket
  	socket_fd = socket(PF_INET, SOCK_DGRAM, 0);
  	if(socket_fd < 0) errorMessage("Vytvaranie socketu zlyhalo.");

  	//nastavime seba
  	struct sockaddr_in me;
  	me.sin_family = AF_INET;
  	me.sin_addr.s_addr = INADDR_ANY;
  	me.sin_port = htons(my_port);
  	int my_bind = bind(socket_fd, (struct sockaddr*)&me, sizeof(me));
  	if(my_bind == -1) errorMessage("Nastavenie adresyy zlyhalo.");

  	//nastavime partnera
	struct sockaddr_in his;
  	his.sin_family = AF_INET;
  	his.sin_addr.s_addr = inet_addr(adress);
  	if(his.sin_addr.s_addr == INADDR_NONE) errorMessage("Neplatna adresa");
  	his.sin_port = htons(his_port);
  	int my_connect = connect(socket_fd, (struct sockaddr*)&his, sizeof(his));
  	if(my_connect == -1) errorMessage("Pripojenie zlyhalo.");


  	//nekonecny cyklus pre komnukaciu
  	while(1){

  		FD_ZERO(&check_fd);
  		FD_SET(0, &check_fd);
  		FD_SET(socket_fd, &check_fd);

  		int read = select(socket_fd + 1, &check_fd, NULL, NULL, NULL);
  		if(read <= 0) errorMessage("Chyba pri selecte.");

  		if(FD_ISSET(0, &check_fd)){
  			sendMessage(socket_fd);
  		}
  		if(FD_ISSET(socket_fd, &check_fd)){
  			receiveMessage(socket_fd);
  		}
  	}

	return 0;
}
