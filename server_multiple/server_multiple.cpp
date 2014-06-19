//
//  Server.cpp
//  Server_select() method
//
//  Created by Zhang Honghao on 6/17/14.
//  CS 454(654) A2
//

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
#include <ctype.h>
#include <fcntl.h>

#define MAX_NUMBER_OF_CLIENTS 5

/**
 *  Process a string to Title Case
 */
char* process_to_title_case(char *string) {
    char *temp = string;
    while (*temp != '\0') {
        // Ignore white spaces before next word
        while (isspace(*temp) && (*temp != '\0')) {
            temp++;
        }
        // Capitalizing
        if ('a' <= *temp && *temp <= 'z') {
            *temp = *temp - 'a' + 'A';
        }
        temp++;
        // Move to next word, and process to lower-case
        while (!isspace(*temp) && (*temp != '\0')) {
            if ('A' <= *temp && *temp <= 'Z') {
                *temp = *temp - 'A' + 'a';
            }
            temp++;
        }
    }
    return string;
}

int sock; // Listening socket
int clients[MAX_NUMBER_OF_CLIENTS]; // Array of connected clients
fd_set socks; // Socket file descriptors that we want to wake up for
int highest_socket; // Highest # of socket file descriptor

/**
 *  Keep socks contain the current alive socks
 */
void build_select_list() {
	// Clears out the fd_set called socks
	FD_ZERO(&socks);
	
    // Adds listening sock to socks
	FD_SET(sock, &socks);
	
    // Add the cennected clients' socks to socks
    for (int i = 0; i < MAX_NUMBER_OF_CLIENTS; i++) {
        if (clients[i] != 0) {
            FD_SET(clients[i], &socks);
            if (clients[i] > highest_socket) {
                highest_socket = clients[i];
            }
        }
    }
}

/**
 *  Handle a new connection from clients
 */
void handle_new_connection() {
    int new_client_sock;
    struct sockaddr_in client;
    int addr_size = sizeof(struct sockaddr_in);
    
    // Try to accept a new connection
    new_client_sock = accept(sock, (struct sockaddr *)&client, (socklen_t *)&addr_size);
    if (new_client_sock < 0) {
        perror("Accept failed");
		 exit(-1);
    }
    // Add this new client to clients
    for (int i = 0; (i < MAX_NUMBER_OF_CLIENTS) && (i != -1); i++) {
        if (clients[i] == 0) {
//            printf("\nConnection accepted: FD=%d; Slot=%d\n", new_client_sock, i);
            clients[i] = new_client_sock;
            new_client_sock = -1;
            break;
        }
    }
    if (new_client_sock != -1) {
        perror("No room left for new client.\n");
        close(new_client_sock);
    }
}

/**
 *  Handle the data come from client with client number
 *
 *  @param client_number Client index in clients
 */
void deal_with_data(int client_number) {
    // Get the socket descriptor
    int client_sock = clients[client_number];
    long receive_size;
    
    // Prepare for string length
    uint32_t network_byte_order;
    uint32_t string_length;
    
    // Receive a message from client
    receive_size = recv(client_sock, &network_byte_order, sizeof(uint32_t), 0);
    
    // Receive successfully
    if (receive_size > 0){
        // Receive size must be same as sizeof(uint32_t) = 4
        if (receive_size != sizeof(uint32_t)) {
            printf("string length error\n");
        }
//        printf("    *receive_size:%lu -   ", receive_size);
        // Get string length
        string_length = ntohl(network_byte_order);
        
        // Prepare memeory for string body
        char *client_message = (char *)malloc(sizeof(char) * string_length);
        // Revice string
        receive_size = recv(client_sock, client_message, string_length, 0);
        
//        printf("stringlength:%u - received_size: %zd\n", string_length, receive_size);
        if (receive_size == string_length) {
            //            struct timeval now;
            //            gettimeofday(&now, NULL);
            //            printf("%s - %ld.%d\n", client_message, now.tv_sec, now.tv_usec);
            printf("%s\n", client_message);
            process_to_title_case(client_message);
            // Send string length
            send(client_sock, &network_byte_order, sizeof(uint32_t), 0);
            // Send string
            if(send(client_sock, client_message, receive_size, 0) == -1) {
                //                printf("Send failed\n");
            }
        } else {
            printf("string error: ");
        }
        free(client_message);
    }
    else if (receive_size < 0) {
        perror("Receive failed");
    }
    else {
//        printf("\nConnection lost: FD=%d;  Slot=%d\n", client_sock, client_number);
        close(client_sock);
        
        // Set this place to be available
        clients[client_number] = 0;
    }
}

/**
 *  Read socks that has changed
 */
void read_socks() {
//    printf("read\n");
    // If listening sock is woked up, there is a new client come in
    if (FD_ISSET(sock, &socks)) {
        handle_new_connection();
    }
    
    // Check whether there is new data come in
    for (int i = 0; i < MAX_NUMBER_OF_CLIENTS; i++) {
        if (FD_ISSET(clients[i], &socks)) {
            deal_with_data(i);
        }
    }
}

int main(int argc , char *argv[])
{
    // Get IP (v4) address for this host
    struct addrinfo hints, *res, *p;
    int status;
    char ipstr[INET6_ADDRSTRLEN];
    
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET; // AF_INET or AF_INET6 to force version
    hints.ai_socktype = SOCK_STREAM;
    
    char hostname[128];
    gethostname(hostname, sizeof(hostname));
    if ((status = getaddrinfo(hostname, NULL, &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return 2;
    }
    
    for(p = res; p != NULL; p = p->ai_next) {
        void *addr;
        if (p->ai_family == AF_INET) { // IPv4
            struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;
            addr = &(ipv4->sin_addr);
            // Convert the IP to a string and print it:
            inet_ntop(p->ai_family, addr, ipstr, sizeof ipstr);
            printf("SERVER_ADDRESS %s\n", ipstr);
            break;
        }
    }
    freeaddrinfo(res); // Free the linked list
    
    int addr_size;
    int readsocks; // Number of sockets ready for reading
    
    // Create socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
//        printf("Could not create socket\n");
    }
//    printf("Socket created\n");
    
    // So that we can re-bind to it without TIME_WAIT problems
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &addr_size, sizeof(addr_size));
    
    //Prepare the sockaddr_in structure
    struct sockaddr_in server;
    memset(&server, 0, sizeof(struct sockaddr_in));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(0);
    
    //Bind
    if(bind(sock, (struct sockaddr *)&server, sizeof(struct sockaddr_in)) == -1) {
        perror("Bind failed");
        exit(-1);
    }
//    printf("Bind done\n");

    //Listen, up to 5 clients
    listen(sock , MAX_NUMBER_OF_CLIENTS);

    // Print out port number
    socklen_t addressLength = sizeof(server);
    if (getsockname(sock, (struct sockaddr*)&server, &addressLength) == -1) {
        perror("Get port error");
        exit(-1);
    }
    printf("SERVER_PORT %d\n", ntohs(server.sin_port));
    
    // Since there is only one sock, this is the highest
    highest_socket = sock;
    // Clear the clients
    memset((char *) &clients, 0, sizeof(clients));
    
    // Server loop
    while (1) {
        build_select_list();
//        timeout.tv_sec = 1;
//        timeout.tv_usec = 0;
        readsocks = select(highest_socket + 1, &socks, NULL, NULL, NULL);
        if (readsocks < 0) {
            perror("Select error");
            exit(-1);
        }
        if (readsocks == 0) {
//            printf(".");
//            fflush(stdout);
        } else {
            read_socks();
        }
    }
    
    close(sock);
    return 0;
}