/*
Usage: file_server <port number> /path/to/files /path/to/logfile
Daemonize on startup
if logfile does not exist, create it
fork() new child process when new request from client

  */
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/file.h>
#include <netinet/in.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <err.h>
#include <errno.h>
#include <limits.h>

#define BUFLEN 1024

int main(int argc, char ** argv)
{
	/*argv[1] is the server IP
	  argv[2] is the port number to listen on
	  argv[3] is the file we want*/
	
	if (argc != 4)
	{
		printf("Incorrect number of parameters.\n");
		printf("Usage: file_client IP portNumber fileWanted\n");
	}


	/*create socket and listen*/
	struct sockaddr_in si_me, adr_clnt;
	struct sigaction sa;
	int s, i, slen=sizeof(adr_clnt);
	char buf[BUFLEN];
	pid_t pid;
	u_long p;
	unsigned short client_port;
	char * client_address;
	int receive;
	time_t curtime;
	struct tm * local_time;

	if ( ( s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP) ) == -1)
	{
		printf("Error creating socket\n");
		return 1;
	}

	memset((char *) &si_me, 0, sizeof(si_me));
	si_me.sin_family = AF_INET;
	si_me.sin_port = htons(atoi(argv[2]));
	si_me.sin_addr.s_addr = htonl(inet_addr(argv[1]));
	
	adr_clnt.sin_family = AF_INET;
	adr_clnt.sin_port = htons(INADDR_ANY);
	adr_clnt.sin_addr.s_addr = htonl(0);

	/*request file from server*/
	sendto(s, argv[3], sizeof(argv[3]),0, \
		(struct sockaddr*)&si_me, sizeof(si_me));
	printf("Waiting on %s\n", argv[3]);

	/*create file to be copied*/
	FILE * fp = fopen(argv[3], "w");
	
	int len_inet = sizeof(si_me);
	while(1) {
		
		/*run while receiving packet*/
		while (recvfrom(s, buf, BUFLEN, 0, (struct sockaddr*)&si_me, &len_inet)!=-1){
			printf("We have received a response from the server!\n");
			for (i = 0; i < sizeof(buf); i++){
			
				if(buf[i] == '$'){
					/*file is done being sent*/
					fclose(fp);
					close(s);
					exit(0);
				}

				/*write file 8 bits at a time to fp*/
				fputc(buf[i], fp);
			}


		}
		/*
		char * buffer[BUFLEN];
		FILE * fin = fopen(getFile,"r");
		int size = fread(buffer, fstat(fileno(fin))\
			, sizeof(buffer), fin);
		do {
			if (size < 0)
			{
				printf("Error ocurred reading file!");
				char myMessage[80];
			
			}
			sendto(s, buffer, sizeof(buffer),0, \
				(struct sockaddr*)&adr_clnt, slen);
					} while (size == sizeof(buffer));
		}*/
	}

	exit(0);
}
