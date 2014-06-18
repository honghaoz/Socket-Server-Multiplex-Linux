#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <unistd.h>
#include <pthread.h>

#define MAX_NUMBER_OF_CLIENTS 5
#define MAX_BUFFER_SIZE 2000

/**
 *  Process a string to Title Case
 */
char* process_to_title_case(char *string) {
    char *temp = string;
    while (*temp != '\0') {
        // Capitalizing
        if ('a' <= *temp && *temp <= 'z') {
            *temp = *temp - 'a' + 'A';
        }
        temp++;
        // Move to next word, and process to lower-case
        while ((*temp != ' ') && (*temp != '\0')) {
            if ('A' <= *temp && *temp <= 'Z') {
                *temp = *temp - 'A' + 'a';
            }
            temp++;
        }
        // Either *temp == ' ' or '\0'
        if (*temp == '\0') {
            break;
        }
        temp++;
    }
    return string;
}

/**
 * Thread function, handle a connection for each client
 */
void *handle_a_connection(void *socket_desc)
{
    //Get the socket descriptor
    int sock = (int)((long)socket_desc);
    ssize_t receive_size;
    char client_message_buffer[MAX_BUFFER_SIZE];
    char *client_message;
    
    //Receive a message from client
    while((receive_size = recv(sock, client_message_buffer, MAX_BUFFER_SIZE, 0)) > 0){
//        printf("%zd\n", receive_size);
        //Send the message back to client
        client_message = (char *)malloc(sizeof(char) * receive_size);
        memcpy(client_message, client_message_buffer, sizeof(char) * receive_size);
        struct timeval now;
        gettimeofday(&now, NULL);
        printf("%s - %ld.%d\n", client_message, now.tv_sec, now.tv_usec);
//        printf("%zu:%s\n", strlen(client_message), client_message);
        process_to_title_case(client_message);
//        printf("%zu:%s\n", strlen(client_message), client_message);
//        printf("%d: %s - \n",strlen(client_message),client_message);
        if(send(sock, client_message, receive_size, 0) == -1) {
            printf("Send failed\n");
        }
        free(client_message);
        memset(client_message_buffer, 0, MAX_BUFFER_SIZE);
    }
    
    if(receive_size == 0) {
        printf("Client disconnected\n");
        fflush(stdout);
    }
    else if(receive_size == -1) {
        perror("recv failed");
    }
    pthread_exit(socket_desc);
}

int main(int argc , char *argv[])
{
    int socket_desc, client_sock, new_client_sock, addr_size;
    
    //Create socket
    socket_desc = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_desc == -1) {
        printf("Could not create socket\n");
    }
    printf("Socket created\n");

    //Prepare the sockaddr_in structure
    struct sockaddr_in server, client;
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(8888);
    
    //Bind
    if(bind(socket_desc, (struct sockaddr *)&server, sizeof(struct sockaddr_in)) == -1) {
        perror("Bind failed\n");
        exit(-1);
    }
    printf("Bind done\n");

    //Listen, up to 5 clients
    listen(socket_desc , MAX_NUMBER_OF_CLIENTS);

    //Accept and incoming connection
    printf("Waiting for incoming connections...\n");
    
    pthread_attr_t thread_attr;
    pthread_attr_init(&thread_attr);
    
    addr_size = sizeof(struct sockaddr_in);
    while((client_sock = accept(socket_desc, (struct sockaddr *)&client, (socklen_t *)&addr_size))) {
        printf("Connection accepted\n");
        
        pthread_t thread_for_client;
        pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_JOINABLE);
        new_client_sock = client_sock;
        
        if(pthread_create(&thread_for_client, &thread_attr, handle_a_connection, (void*)((long)new_client_sock))) {
            perror("Could not create thread");
            exit(-1);
        }
        
        //Now join the thread , so that we dont terminate before the thread
//        pthread_join(thread_for_client, NULL);
        printf("Handler assigned\n");
    }
    
    pthread_attr_destroy(&thread_attr);
    
    if (client_sock < 0)
    {
        perror("Accept failed");
        exit(-1);
    }
    
    return 0;
}