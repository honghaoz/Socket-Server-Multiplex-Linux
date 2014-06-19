#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <netinet/in.h>
#include <netdb.h>
//#include <ifaddrs.h>

#define MAX_NUMBER_OF_CLIENTS 5

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
    
    // Prepare for string length
    uint32_t network_byte_order;
    uint32_t string_length;
    
    //Receive a message from client
    while((receive_size = recv(sock, &network_byte_order, sizeof(uint32_t), 0)) > 0){
        // Receive size must be same as sizeof(uint32_t) = 4
        if (receive_size != sizeof(uint32_t)) {
            printf("string length error\n");
        }
        // Get string length
        string_length = ntohl(network_byte_order);
        
        // Prepare memeory for string body
        char *client_message = (char *)malloc(sizeof(char) * string_length);
        // Revice string
        receive_size = recv(sock, client_message, string_length, 0);
        if (receive_size == string_length) {
//            struct timeval now;
//            gettimeofday(&now, NULL);
//            printf("%s - %ld.%d\n", client_message, now.tv_sec, now.tv_usec);
            printf("%s\n", client_message);
            process_to_title_case(client_message);
            // Send string length
            send(sock, &network_byte_order, sizeof(uint32_t), 0);
            // Send string
            if(send(sock, client_message, receive_size, 0) == -1) {
                printf("Send failed\n");
            }
        } else {
            printf("string length error\n");
        }
        free(client_message);
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
    struct addrinfo hints, *res, *p;
    int status;
    char ipstr[INET6_ADDRSTRLEN];
    
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // AF_INET or AF_INET6 to force version
    hints.ai_socktype = SOCK_STREAM;
    
    char hostname[128];
    gethostname(hostname, sizeof hostname);
    
    if ((status = getaddrinfo(hostname, NULL, &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return 2;
    }
    
    for(p = res;p != NULL; p = p->ai_next) {
        void *addr;
//        char *ipver;
        // get the pointer to the address itself,
        // different fields in IPv4 and IPv6:
        if (p->ai_family == AF_INET) { // IPv4
            struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;
            addr = &(ipv4->sin_addr);
//            ipver = "IPv4";
        } else { // IPv6
            struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)p->ai_addr;
            addr = &(ipv6->sin6_addr);
//            ipver = "IPv6";
        }
        
        // convert the IP to a string and print it:
        inet_ntop(p->ai_family, addr, ipstr, sizeof ipstr);
        printf("SERVER_ADDRESS %s\n", ipstr);
    }
    freeaddrinfo(res); // free the linked list
    
    int socket_desc, client_sock, new_client_sock, addr_size;
    
    //Create socket
    socket_desc = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_desc == -1) {
//        printf("Could not create socket\n");
    }
//    printf("Socket created\n");
    
    //Prepare the sockaddr_in structure
    struct sockaddr_in server, client;
    memset(&server, 0, sizeof(struct sockaddr_in));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(0);
    
    //Bind
    if(bind(socket_desc, (struct sockaddr *)&server, sizeof(struct sockaddr_in)) == -1) {
        perror("Bind failed\n");
        exit(-1);
    }
//    printf("Bind done\n");

    //Listen, up to 5 clients
    listen(socket_desc , MAX_NUMBER_OF_CLIENTS);

//    struct sockaddr_in localAddress;
    socklen_t addressLength = sizeof(server);
    getsockname(socket_desc, (struct sockaddr*)&server, &addressLength);
    
    printf("SERVER_PORT %d\n", ntohs(server.sin_port));
//    int len = sizeof(struct sockaddr);
//    if(getsockname(socket_desc, (struct sockaddr *)&server, &len) == -1){
//        perror("getsockname");
//    } else{
//        printf("SERVER_PORT %d\n", ntohl(server.sin_port));
//    }
    
    
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