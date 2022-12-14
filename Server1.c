#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <signal.h>

//Constants
#define MAX_CLIENTS 5
#define BUFFER_SIZE 2048

static int uid = 10;
int number_of_helps;

/* Client structure */
typedef struct
{
    struct sockaddr_in address; //socket
    int sockfd; //socket file descriptor
    int uid; //socket id
    char name[32]; //client name (random id parsed to char)
} client_t;

//clients queue
client_t *clients[MAX_CLIENTS];
//mutex to handle the insertion and removal of a client from the queue
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;


/* Trim client name */
void str_trim_lf(char *arr, int length)
{
    int i;
    for (i = 0; i < length; i++)
    { // trim \n
        if (arr[i] == '\n')
        {
            arr[i] = '\0';
            break;
        }
    }
}

/* 
Add clients to queue. In order to add a new client to the queue
we must verify that the client is not already in the queue.
*/
void queue_add(client_t *cl)
{
    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < MAX_CLIENTS; ++i)
    {
        if (!clients[i])
        {
            clients[i] = cl;
            break;
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

/* Remove clients from the queue */
void queue_remove(int uid)
{
    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < MAX_CLIENTS; ++i)
    {
        if (clients[i])
        {
            if (clients[i]->uid == uid)
            {
                clients[i] = NULL;
                break;
            }
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}


/*This function is used to send a help message to all clients. 
We do it with mutual exclusion just for integrity purposes
@param round : the current help message
*/

void send_help_message(int round)
{
    char *s = "Ayuda\n";
    char buff [1024];
    sprintf(buff, "%d\n", round);
    printf("Round %s", buff);
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; ++i)
    {
        if (clients[i])
        {
            if (write(clients[i]->sockfd, buff, strlen(s)) < 0)
            {
                perror("ERROR: write to descriptor failed");
                break;
            }
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

/* Handle all communication with the client */
void *handle_client(void *arg)
{
    // buffer where we store what we want to send to the clients
    char buff_out[BUFFER_SIZE];
    //client name
    char name[32];
    //flag so we know when a client leaves (testing purposes)
    int leave_flag = 0;

    client_t *cli = (client_t *)arg;

    strcpy(cli->name, name);

    while (1)
    {
        if (leave_flag)
        {
            break;
        }
        // recieve the message from the client
        int receive = recv(cli->sockfd, buff_out, BUFFER_SIZE, 0);
        if (receive > 0)
        {
            if (strlen(buff_out) > 0)
            {
                str_trim_lf(buff_out, strlen(buff_out));
                printf("%s\n", buff_out);
                number_of_helps++;
            }
        }
        else
        {
            leave_flag = 1;
        }
        // flush the buffer
        bzero(buff_out, BUFFER_SIZE);
    }

    /* Delete client from the clients queue and stop thread */
    close(cli->sockfd);
    queue_remove(cli->uid);
    free(cli);
    pthread_detach(pthread_self());

    return NULL;
}

/*This function purpose is to ask for help
to all the clients. First we give a 5 seconds window for 
all the clients to connect and then we send the help message
If within 10 seconds we got 2 or more help messages have helped us, we
print a message in the standard output
*/

void *server_thread()
{
    sleep(5);
    printf("Someone help me!\n");
    number_of_helps = 0;
    int iteration = 0;
    while (1)
    {
        iteration++;
        send_help_message(iteration);
        for (int i = 0; i < 10; i++)
        {
            sleep(1);
            printf("Helps received: %d\n", number_of_helps);
        }
        if (number_of_helps >= 2)
        {
            printf("Thanks for the help!!!!!!!\n");
        }
        else
        {
            printf("No one helped me!\n");
        }
        number_of_helps = 0;
       
    }
}

int main(int argc, char **argv)
{

    //variables and declarations
    char *ip = "127.0.0.1";
    int port = atoi(argv[1]);
    int option = 1;
    int listenfd = 0, connfd = 0;
    struct sockaddr_in serv_addr;
    struct sockaddr_in cli_addr;
    pthread_t tid;

    /* create the socket */
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(ip);
    serv_addr.sin_port = htons(port);

    signal(SIGPIPE, SIG_IGN);

    if (setsockopt(listenfd, SOL_SOCKET, (SO_REUSEPORT | SO_REUSEADDR), (char *)&option, sizeof(option)) < 0)
    {
        perror("ERROR: setsockopt failed");
        return EXIT_FAILURE;
    }

    /* Bind */
    if (bind(listenfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("ERROR: Socket binding failed");
        return EXIT_FAILURE;
    }

    /* Listen */
    if (listen(listenfd, 10) < 0)
    {
        perror("ERROR: Socket listening failed");
        return EXIT_FAILURE;
    }

    printf("SERVER STARTED\n");

    //create a thread for the function that requests help messages
    pthread_create(&tid, NULL, &server_thread, NULL);

    // while true accept new connections
    while (1)
    {
        socklen_t clilen = sizeof(cli_addr);
        connfd = accept(listenfd, (struct sockaddr *)&cli_addr, &clilen);

        /* Configure the new client */
        client_t *cli = (client_t *)malloc(sizeof(client_t));
        cli->address = cli_addr;
        cli->sockfd = connfd;
        cli->uid = uid++;
        // printf("Client PORT : %d\n", ntohs(cli->address.sin_port));

        /* Add client to the clients queue and fcreate a new thread for it */
        queue_add(cli);
        pthread_create(&tid, NULL, &handle_client, (void *)cli);
    }

    return EXIT_SUCCESS;
}
