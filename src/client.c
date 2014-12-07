/************************************************************************/
/*   PROGRAM NAME: client.c  (works with serverX.c)                     */
/*                                                                      */
/*   Client creates a socket to connect to Server.                      */
/*   When the communication established, Client writes data to server   */
/*   and echoes the response from Server.                               */
/*                                                                      */
/*   To run this program, first compile the server_ex.c and run it      */
/*   on a server machine. Then run the client program on another        */
/*   machine.                                                           */
/*                                                                      */
/*   COMPILE:    gcc client.c -o client -lnsl -pthread                          */
/*   TO RUN:     client  server-machine-name                            */
/*                                                                      */
/************************************************************************/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>  /* define socket */
#include <netinet/in.h>  /* define internet socket */
#include <netdb.h>       /* define internet socket */
#include <pthread.h>
#include <signal.h>


#define SERVER_PORT 7777 /* define a server port number */

int quit = 0; //Used to quit program
int sd;

//Threads to handle read and write
pthread_t t_read, t_write;

//Declarations of function used in client
void signalhandler(int sig);
void *read_handler(void *soc);
void *write_handler(void *soc);

int main( int argc, char* argv[] )
{
  struct sockaddr_in server_addr = { AF_INET, htons( SERVER_PORT ) };
  char buf[512];
  struct hostent *hp;

  if( argc != 2 )
  {
	printf( "Usage: %s [hostname]\n", argv[0] );
	exit(1);
  }

  /* get the host info */
  if( ( hp = gethostbyname( argv[1] ) ) == NULL )
  {
	printf( "%s: %s unknown host\n", argv[0], argv[1] );
	exit( 1 );
  }

  bcopy( hp->h_addr_list[0], (char*)&server_addr.sin_addr, hp->h_length );

  /* create a socket */
  if( ( sd = socket( AF_INET, SOCK_STREAM, 0 ) ) == -1 )
  {
	perror( "client: socket failed" );
	exit( 1 );
  }

  /* connect a socket */
  if( connect( sd, (struct sockaddr*)&server_addr, sizeof(server_addr) ) == -1 )
  {
	perror( "client: connection failed" );
	exit( 1 );
  }

  printf("Server \"%s\" connected!\n", argv[1]);

  /* create a thread for reading */
  if( pthread_create( &t_read, NULL, read_handler, NULL) != 0 )
  {
	perror( "Error, creating thread for reading failed" );
	exit( 1 );
  }

  /* create a thread for writing */
  if( pthread_create( &t_write, NULL, write_handler, NULL) != 0 )
  {
	perror( "Error, creating thread for writing failed" );
	exit( 1 );
  }
  
  signal(SIGINT, signalhandler);
  
  while(quit != 1); //program goes off and does other stuff

  close(sd); //close the socket
  return(0);
}

//If Ctrl-C is pressed by the user, then this message will be printed to the user
void signalhandler(int sig)
{
  printf( "\n[HELP] Please type \"/quit\", \"/exit\" or \"/part\" in order to exit the chatroom.\n");
}

void *read_handler(void *soc)
{
  char buf[512];
  
  while(quit != 1)
  {
	//Read the data from socket into buf
	if(read(sd, buf, sizeof(buf[512])) < 0 )
	{
	  perror( "Error, there was a problem reading" );
	  exit( 1 );
	}
	else
	{
	  if( strcmp(buf, "/__quit") == 0)
	  {
		quit = 1;
		pthread_exit(0);
	  }
	  else
	  {
		printf("%s\n", buf);
	  }
	}
  }
}

void *write_handler(void *soc)
{
	char buf[512];

	printf("Please Enter Your User Name\n");
	gets(buf);
	gets(buf);

	/*take name, send it to the server */
	write(sd, buf, sizeof(buf));
	
	printf("Attempting to connect with server, if server is full please wait...\n");

	while((gets(buf) != EOF) && (quit != 1) )
	{
		write(sd, buf, sizeof(buf));

		if((strcmp(buf,"/exit")==0)||(strcmp(buf,"/quit")==0)||(strcmp(buf,"/part")==0))
		{
			printf("Quitting now...");
			quit = 1;
			pthread_exit(0);
		}
	}
	quit = 1;
}
