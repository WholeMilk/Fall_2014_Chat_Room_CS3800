#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>  
#include <netinet/in.h>  
#include <netdb.h>      
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <time.h>

/* gcc server.c -o server -lnsl -pthread */

#define SERVER_PORT 63122
#define MAX_CLIENT 10
#define BUFFER_SIZE 512

//Two mutexes will be used, prevent any race conditions for read + write
pthread_mutex_t accept_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t send_mutex = PTHREAD_MUTEX_INITIALIZER;

struct sockaddr_in server_addr;
struct sockaddr_in client_addr;

//socket, accept, and read
int sd, ns, k;

//length of socketaddr structure
int length;

//when set to 1, aids in closing program
int quit = 0;

///////////////////////////////////////////
/// @struct clients
/// @brief stores info about each client
///   including socket, name, buffer, etc
///////////////////////////////////////////
typedef struct clients 
{
	int m_fd;
	int m_index; //index that the client is located in the clients[] array
	pthread_t m_thread; //thread for the client
	char m_buffer[BUFFER_SIZE]; //buffer for the client, read/write
	char m_name[BUFFER_SIZE]; //name of user will be stored here
} session;

//acts like the FD array mentioned in supplamental slides
session clients[MAX_CLIENT]; 

//list of functions used in server.c
void default_clients();
int find_opening_client_spot();
void *client_handler(void * client);
void send_to_clients(session * sender_index);
void signalhandler(int sig);
void client_is_leaving(session * client_leaving);
void client_has_entered(session * client_joining);


int main()
{
	struct sockaddr_in server_addr = { AF_INET, htons( SERVER_PORT ) };
	struct sockaddr_in client_addr = { AF_INET };
	length = sizeof( client_addr );
	
	//initlize basic client info
	default_clients();
	
	/* create a stream socket */
	if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		perror("Server Error: Socket Failed");
		exit(1);
	}
	
	//variable needed for setsokopt call
	int setsock = 1;
	
	//assists in using address
	if(setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &setsock, sizeof(setsock)) == -1)
	{
		perror("Server Error: Setsockopt failed");
		exit(1);
	}
	
	/* bind the socket to an internet port */
	if (bind(sd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1 )
	{
		perror("Server Error: Bind Failed");
		exit(1);
	}
	
	//initilize the signal handler
	signal(SIGINT, signalhandler);
	
	/* listen for clients */
	printf(">>Server is now listening for up to 10 clients\n");
	if (listen(sd, 10) == -1)
	{
		perror("Server Error: Listen failed");
		exit(1);
	}
	
	while(quit != 1)
	{
		int opening;
		//only one thread in this section at a time, prevents race cond. for client opening, etc
		pthread_mutex_lock(&accept_mutex);
		
		if ((opening = find_opening_client_spot()) != -1) //if there's an opening, otherwise check again
		{
			//if there's an opening, accept the client
			if((clients[opening].m_fd = accept(sd, (struct sockaddr*)&client_addr, &length)) == -1)
			{
				if (quit != 1) //prevents some extraneous output 
				{
					perror("Server Error: Accepting issue"); 
					exit(1);
				}
			}
			else
			{
				//once client is accepted, create a thread for the client
				if (pthread_create(&(clients[opening].m_thread), NULL, &client_handler, &(clients[opening].m_index)) != 0)
				{
					printf("Error Creating Thread\n");
					exit(1);
				}
			}
		}
		pthread_mutex_unlock(&accept_mutex);
	}
	
	return (0);
}

//initilizes some basic client info
//m_index indicates the location of the client in the clients[] array
//setting fd = -1 says that this client slot isn't active
void default_clients()
{
	int client;
	for (client = 0; client < MAX_CLIENT; client++)
	{
		clients[client].m_index = client;
		clients[client].m_fd = -1; // defualt, not set up yet
	}
}

//used to determien if there is an opening for a new client
// -1 -> there is no opening
// any number greater than -1 indicates the idnex in the clients[]
  //that has an opening
int find_opening_client_spot()
{
	int client;
	for (client = 0; client < MAX_CLIENT; client++)
	{
		if (clients[client].m_fd == -1) 
		{//that index in the clients[] is not being utilized
			return client;
		}
	}
	return -1; // currently at capacity
}

void *client_handler(void * client) 
{
	int client_index = *((int *) client); /*convert value passed to int*/
	
	//first get the name of the client, store it in m_name
	if (read(clients[client_index].m_fd, clients[client_index].m_name, BUFFER_SIZE) < 0)
	{
		perror("Reading Name Error");
		exit(1);
	}
	else
	{
		//print to server terminal that a new client has entered
		printf(">>%s has joined the server\n", clients[client_index].m_name); 
		//welcome the client to the server
		write(clients[client_index].m_fd, ">>Welcome to the Server!", BUFFER_SIZE);
		//tell all other clients that a new user has entered the server
		client_has_entered(&clients[client_index]);
	}
	while (quit != 1 && clients[client_index].m_fd != -1) //while client is active....
	{
		//read from the client, store in m_buffer
		if (read(clients[client_index].m_fd, clients[client_index].m_buffer, BUFFER_SIZE) < 0)
		{
			perror("Reading Data Error");
			exit(1);
		}
		else
		{
			//if the client is ready to quit...
			if ((strcmp(clients[client_index].m_buffer, "/exit") == 0) || (strcmp(clients[client_index].m_buffer, "/quit") == 0) || (strcmp(clients[client_index].m_buffer, "/part") == 0))
			{
				//send the client the quit directive, helps client cleanly leave
				write(clients[client_index].m_fd, "/directive -> quit", BUFFER_SIZE);
				//tell all other clients that the user is leaving the server
				client_is_leaving(&clients[client_index]);
				//free up that client's spot in the clients[] array
				clients[client_index].m_fd = -1;
			}
			else
			{
				pthread_mutex_lock(&send_mutex);
				//send user's message to all other clients
				send_to_clients(&clients[client_index]);
				pthread_mutex_unlock(&send_mutex);
			}
		}
	}
	//this code will be executed once the user has indicated that they are leaving
	//print to the server terminal that the client is leaving
	printf(">>%s has Quit\n", clients[client_index].m_name);
	//close that socket used for that client
	close(clients[client_index].m_fd);
	//end thread's execution
	if (pthread_cancel(clients[client_index].m_thread) != 0)
	{
		perror("Ending thread issue");
		exit(1);
	}
}

//this function will send the contents of the sender's buffer
  //to all other users
void send_to_clients(session * sender)
{
	if (quit != 1) // prevents some bogus output
	{
		int client;
		char write_buffer[BUFFER_SIZE];
		
		//first format the message, name> message
		strncpy(write_buffer, sender->m_name, BUFFER_SIZE);
		strncat(write_buffer, "> ", 2);
		strncat(write_buffer, sender->m_buffer, BUFFER_SIZE);
		
		//print to server terminal
		printf("%s\n", write_buffer);
		
		//send message to all active clients except the sender
		for (client = 0; client < MAX_CLIENT; client++)
		{
			if ((clients[client].m_fd != 1) && (client != sender->m_index))
			{
				write(clients[client].m_fd, write_buffer, BUFFER_SIZE);
			}
		}
	}
}

//Cntrl-C
//tells all active clients that the server is shutting down
//closes all connections, and ends the server
void signalhandler(int sig)
{
	char msg[BUFFER_SIZE];
	time_t start_time, cur_time;//used to wait for 10 seconds
	
	strncpy(msg, ">>The Server will shut down in 10 seconds.", BUFFER_SIZE);
	printf("\n%s\n", msg);
	fflush( stdout ); //ensures that message is printed to server terminal
	
	int client;
	
	//tell all active clients that server is shutting down
	for (client = 0; client < MAX_CLIENT; client++)
	{
		if (clients[client].m_fd != -1)
		{
			write(clients[client].m_fd, msg, BUFFER_SIZE);
		}
	}
	
	//wait 10 seconds, allows user to quit manually if desired
	time(&start_time);
	do
	{
		time(&cur_time);
	}
	while((cur_time - start_time) < 10);
	
	strncpy(msg, "/directive -> quit", BUFFER_SIZE);//the quit direcitve
	//send all active clients the quit directive
	for (client = 0; client < MAX_CLIENT; client++)
	{
		if (clients[client].m_fd != -1)
		{
			write(clients[client].m_fd, msg, BUFFER_SIZE);
		}
	}

	//close connecton to all active clients
	//stop threads for all active clients
	for (client = 0; client < MAX_CLIENT; client++)
	{
		if (clients[client].m_fd != -1)
		{
			clients[client].m_fd = -1;
			close(clients[client].m_fd);
			if (pthread_cancel(clients[client].m_thread) !=0)
			{
				perror("Issue ending thread");
				exit(1);
			}
		}
	}
	
	quit = 1;
	
	//close all connections.. free up server address
	close(k);
	close(ns);
	close(sd);
	unlink(server_addr.sin_addr);
	
}

//this function tells all active clients that 'client_leaving' has quit
void client_is_leaving(session * client_leaving)
{
	int client;
	char write_buffer[BUFFER_SIZE];
	
	//store it in the write_buffer
	strncpy(write_buffer, ">>", BUFFER_SIZE);
	strncat(write_buffer, client_leaving->m_name, BUFFER_SIZE);
	strncat(write_buffer, " is leaving the server.", BUFFER_SIZE);
	
	//tell all active clients that aren't the one currently leaving
	for (client = 0; client < MAX_CLIENT; client++)
	{
		if ((clients[client].m_fd != 1) && (client != client_leaving->m_index))
		{
			write(clients[client].m_fd, write_buffer, BUFFER_SIZE); 
		}
	}
}

//this function tells all active clients that 'client_joining' has entered the server
void client_has_entered(session * client_joining)
{
	int client;
	char write_buffer[BUFFER_SIZE];
	
	//store it in the write_buffer
	strncpy(write_buffer, ">>", BUFFER_SIZE);
	strncat(write_buffer, client_joining->m_name, BUFFER_SIZE);
	strncat(write_buffer, " has entered the server.", BUFFER_SIZE);
	
	//tell all active clients that aren't the one currently entering
	for (client = 0; client < MAX_CLIENT; client++)
	{
		if ((clients[client].m_fd != 1) && (client != client_joining->m_index))
		{
			write(clients[client].m_fd, write_buffer, BUFFER_SIZE);
		}
	}
}