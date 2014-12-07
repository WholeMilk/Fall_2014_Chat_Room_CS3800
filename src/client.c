/////////////////////////////////////////////////////////////////////
/// @file client.c
/// @authors Matthew Lindner & Kyle Fagan
/// @brief client implementation - multithreaded, socket communication
/////////////////////////////////////////////////////////////////////
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>  
#include <netinet/in.h>  
#include <netdb.h>       
#include <pthread.h>
#include <signal.h>

/* gcc client.c -o client -lnsl -pthread */

#define SERVER_PORT 63122

//program uses two threads, one for read, one for write
pthread_t t_read, t_write;

//socket used to connect to server
int sd;

//when set to 1, aids in closing program
int quit = 0;

//forward declarations of all functions used in program
void signalhandler(int sig);
void *read_handler(void *arg);
void *write_handler(void *arg);


int main()
{
	struct sockaddr_in server_addr = { AF_INET, htons( SERVER_PORT ) }; 
	char buf[512]; 
	struct hostent *hp; 
	
	
	printf("Please Enter the Server You Would Like to Connect To\n");
	scanf("%s", buf); //read the server address from the keyboard
	
	/* get the host */
	if((hp = gethostbyname(buf)) == NULL)
	{
		printf("Error: Unkown Host");
		exit(1);
	}
	bcopy( hp->h_addr_list[0], (char*)&server_addr.sin_addr, hp->h_length ); 
	
	/* create a socket */
	if( ( sd = socket( AF_INET, SOCK_STREAM, 0 ) ) == -1 )  //internet stream socket, TCP
  { 
		perror( "Error: socket failed" ); 
		exit( 1 ); 
  } 
		
	/*connect to the socket */
	if (connect( sd, (struct sockaddr*)&server_addr, sizeof(server_addr) ) == -1 )
	{
		perror( "Error: Connection Issue" ); 
		exit(1);
	}
	
	/*create thread for reading */
	if (pthread_create(&t_read, NULL, read_handler, NULL) != 0)
	{
		perror("Error Creating Read Thread");
		exit(1);
	}
	//create a thread for writing
	if (pthread_create(&t_write, NULL, write_handler, NULL) !=0)
	{
		perror("Error Creating Write Thread");
		exit(1);
	}
	
	//initilize the signal handler
	signal(SIGINT, signalhandler);
	
	while(quit != 1); //program goes off and does other stuff
	
	//stop the execution of the write thread
	if (pthread_cancel(t_write) != 0)
	{
		perror("Problem ending thread");
		exit(1);
	}

	//read thread has already been ended
	
	close(sd); //close the socket
	return (0);
}

//this message will be displayed to screen whenver
//user types Cntrl-C
void signalhandler(int sig)
{
	printf("\nPlease enter '/exit' to quit\n");
}

//Runs as a seperate thread of execution, handles all reading for the server
void *read_handler(void *arg)
{
	char read_buf[512];
	while(quit != 1)
	{
		//read date from socket into read_buf
		if (read(sd, read_buf, sizeof(read_buf)) < 0)
		{
			perror("Reading Data Error");
			exit(1);
		}
		else //if the read was sucessful
		{
			/*if read_buf == quit command, client should quit*/
			if (strcmp(read_buf, "/directive -> quit") == 0)
			{
				quit = 1; //helps tell rest of program to quit
				pthread_exit(0); //exit the thread
			}
			//otherwise, just print the message to the terminal
			else
			{
				printf("%s\n", read_buf);
			}
		}
	}
}

//Runs as a seperate thread of execution, handles all writing to the server
void *write_handler(void *arg)
{
	char buf[512]; 
	printf("Please Enter Your User Name\n");
	
	gets(buf); //helps throw away leading whitespace that would otherwise be sent as the name of the user
	gets(buf);//gets allows strings to contain spaces
	
	/*take name, send it to the server */
	write(sd, buf, sizeof(buf));
	
	//Necessary for when the server is full, doesn't leave the user confused after he/she enters his/her name
	printf("Attempting to join server... If server is full you'll be placed in a wait queue.\n");
	
	//continue to read from the server
	while((gets(buf) != EOF) && quit != 1)
	{
		write(sd, buf, sizeof(buf));
		
		//if user enters quit directive, begin to quit the program
		if ((strcmp(buf, "/exit") == 0) || (strcmp(buf, "/quit") == 0) || (strcmp(buf, "/part") == 0))
		{
			printf("Quitting....\n");
			quit = 1;
			pthread_exit(0);
		}
	}
	quit = 1;
	pthread_exit(0); //may be redundant, but is there just in case
}