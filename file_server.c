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

static void kidhandler(int signum);
int logPrint(char * path, char * logMessage);

int main(int argc, char ** argv)
{
	/*argv[1] is the port number
	  argv[2] is the path to files to be served
	  argv[3] is the path to the log file*/
	
	if (argc != 4)
	{
		printf("Incorrect number of parameters.\n");
		printf("Usage: file_server port_number /path/to/files /path/to/logfile\n");
		return(3);
	}

/*daemonize server, continues to run in current directory, no IO changes*/

   int d;
	if ( ( d=daemon(0,1) ) == 0)
	{
   		printf("Successfully Daemonized! :)\n");
	}
	else{
		printf("Did not successfully daemononize\n");
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
	si_me.sin_port = htons(atoi(argv[1]));
	si_me.sin_addr.s_addr = htonl(INADDR_ANY);

	if ( bind(s, (struct sockaddr*)&si_me, sizeof(si_me)) == -1)
	{
		printf("Error in binding the socket\n");
		return 2;
	}

	/*this will catch bad children via SIGCHLD*/
	sa.sa_handler = kidhandler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1)
		err(1, "sigaction failed");

	int len_inet = sizeof(adr_clnt);
	while(1) {

		/*fork on client request*/
		if (recvfrom(s, buf, BUFLEN, 0, &adr_clnt, &len_inet)!=-1){
			printf("Forking! We got a reequest!\n");
			/*fork child to handle connection*/
			pid = fork();
			if (pid == -1)
				err(1, "fork failed");

			if(pid == 0)
			{
				/*put filepath and file together*/
				char getFile[sizeof(argv[2])+BUFLEN];
				strcpy(getFile, argv[2]);
				strcat(getFile, buf);

				client_port = ntohs(adr_clnt.sin_port);
				client_address = inet_ntoa(adr_clnt.sin_addr);
		
				curtime = time(NULL);
				local_time = localtime(curtime);

				char recTime[80];
				strftime(recTime, sizeof(recTime), "%F %T", local_time);
				
				/*if requested file not found*/
				if (access(getFile, 0)!=0){
					printf("Requested client file not found!\n");
					char * myMessage; 
					sprintf(myMessage,"<%s> <%d> <%s> <%s> <File not Found>\n"\
							, client_address, client_port, buf, recTime);
					do {
						/*Attempt to write to log file until file
						  becomes free. This way there are no missing
						  entries to the log file*/
					}while(logPrint(argv[3],myMessage)==-1);
				}	

				/*File exists*/
				else{
					/*attempt sending file in 1024 chunks*/
					char * buffer[BUFLEN];
					int success = 1;
					FILE * fin = fopen(getFile,"r");
					int size = fread(buffer, fstat(fileno(fin))\
						, sizeof(buffer), fin);
					do {
						if (size < 0)
						{
							printf("Error ocurred reading file!");
							/*write error to log*/
							char myMessage[80];
							sprintf(myMessage, "<%s> <%d> <%s> <%s>"
									"<Transmission not Complete>\n"\
							, client_address, client_port, buf, recTime);
							logPrint(argv[3],myMessage);
							success = 0;
							break;
						}
						sendto(s, buffer, sizeof(buffer),0, \
								(struct sockaddr*)&adr_clnt, slen);
					} while (size == sizeof(buffer));
					
					if (success == 1){
						/*send terminating character*/
						sendto(s, "$", sizeof(buffer),0, \
								(struct sockaddr*)&adr_clnt, slen);
						/*write time of success to log*/
						char doneTime[80];
						curtime = time(NULL);
						local_time = localtime(curtime);
						strftime(doneTime, sizeof(doneTime),\
								"%F %T", local_time);
						char * myMessage;
						printf(myMessage, "<%s> <%d> <%s> <%s> <%s>\n",\
								client_address, client_port, buf, recTime,\
								doneTime);
						logPrint(argv[3],myMessage);
					}
					fclose(fin);
				}

				ssize_t written, w;
			}
		}
	}

}
static void kidhandler(int signum){
	waitpid(WAIT_ANY, NULL, WNOHANG);
}

int logPrint(char * path, char * logMessage){
 /*determine if file exists*/
	int a = access(path, 0);
  	FILE * logfile;
  	if ( a == 0){
  		logfile = fopen(path, "a");
		int fd = fileno(logfile);	
		/*Use file descriptor to place exclusive lock on file*/
		int f = flock(fd, LOCK_EX);
		
		if (f == -1)
		{
			printf("log file is currently in use");
			return -1;
		}
		else{
			fprintf(logfile,"%s", logMessage);
			fclose(logfile);
			f = flock(fd, LOCK_UN);
			return 0;
		}
	}
	
	else{
		printf("Logfile does not exist, creating log file");
		logfile = fopen(path, "w");
		int fd = fileno(logfile);

		/*Use file descriptor to place exclusive lock on file*/
		int f = flock(fd, LOCK_EX);
		
		if (f == -1)
		{
			printf("log file is currently in use");
			return -1;
		}
		else{
			fprintf(logfile,"%s", logMessage);
			fclose(logfile);
			f = flock(fd, LOCK_UN);
			return 0;
		}
	}
}



/*OUTLINE:
  Listen on port
  Get request from client
  	fork()
  	flock()
  	Get IP/Port number from client
  	Use access() to determine if file exists
  		File doesn't exist-> write error to log
  	Begin file transfer
  		Error sending full file? Write error to log
  	Transmission complete -> write time of completion to log
  	*/
