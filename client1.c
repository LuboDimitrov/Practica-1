

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#define LENGTH 2048

volatile sig_atomic_t flag = 0;
// socket file descriptor
int sockfd = 0;
// client id
char name[32];
int flagReceived = 0;
int flagCanHelp = 0;
/*Function that trims the client id.*/
void str_trim_lf(char *arr, int length)
{
    for (int i = 0; i < length; i++)
    {
        if (arr[i] == '\n')
        {
            arr[i] = '\0';
            break;
        }
    }
}

void catch_ctrl_c_and_exit(int sig)
{
    flag = 1;
}

void send_msg_handler()
{
    char message[LENGTH] = {};
    char buffer[LENGTH + 32] = {};

    fflush(stdout);

    sprintf(buffer, "%s: %s\n", name, "Got your back buddy!");
    send(sockfd, buffer, strlen(buffer), 0);

    bzero(message, LENGTH);
    bzero(buffer, LENGTH + 32);

    // catch_ctrl_c_and_exit(2);
}

/*Function that handles the messages received from the server*/

/* void recv_msg_handler()
{
    char message[LENGTH] = {};
    int receive = recv(sockfd, message, LENGTH, 0);
    if (receive > 0)
    {
        int helpNumber = atoi(message);
        printf("Help number: %d\n", helpNumber);
        fflush(stdout);
        flagReceived = 1;
    }
    
    memset(message, 0, sizeof(message));
} */

/*Due to several problems, we dont read the help message from the server
and we only send them help 30% of the time*/

void help_handler()
{
    int random_sleep;
    int random_help;

    while (1)
    {
        random_sleep = rand() % 10;
        printf("Sleeping for %d seconds\n", random_sleep);
        sleep(random_sleep);
        /* if (flagReceived == 1){
            flagReceived = 0;
            recv_msg_handler();
        } */
        printf("Im back again\n");
        random_help = rand() % 10;
        printf("Random help: %d\n", random_help);
        // 30% of the time we send them help
        if (random_help < 3)
        {
            printf("Helping\n");
            send_msg_handler();
        }
    }
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        printf("Usage: %s <PORT>\n", argv[0]);
        return EXIT_FAILURE;
    }

    char *ip = "127.0.0.1";
    int port = atoi(argv[1]);

    signal(SIGINT, catch_ctrl_c_and_exit);

    srand(time(0));
    int random_id = rand() % 100;

    sprintf(name, "%d", random_id);

    printf("Your id is: %s\n", name);
    str_trim_lf(name, strlen(name));

    struct sockaddr_in server_addr;

    /* Socket setup */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip);
    server_addr.sin_port = htons(port);

    // Connect to Server
    int connection_attempt = connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (connection_attempt == -1)
    {
        printf("ERROR: connect\n");
        return EXIT_FAILURE;
    }

    // Send client id to the server
    send(sockfd, name, sizeof(name), 0);

    // Create a thread to run the help_handler function
    pthread_t help_thread;
    pthread_create(&help_thread, NULL, (void *)help_handler, NULL);

    /* pthread_t recv_msg_thread;
    pthread_create(&recv_msg_thread, NULL, (void *) recv_msg_handler, NULL); */

    while (1)
    {
        if (flag)
        {
            printf("\nI'm off!\n");
            break;
        }
    }

    close(sockfd);

    return EXIT_SUCCESS;
}