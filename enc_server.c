#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

#define MAX_BUFFER_SIZE 1000 

void error(const char *msg) {
    perror(msg);
    exit(1);
}

void setupAddressStruct(struct sockaddr_in* address, int portNumber) {
    memset((char*) address, '\0', sizeof(*address));
    address->sin_family = AF_INET;
    address->sin_port = htons(portNumber);
    address->sin_addr.s_addr = INADDR_ANY;
}


char* encryption(char* message, char* key) {
    size_t msg_len = strlen(message);
    char* result_buffer = malloc(msg_len + 2);  // +1 for newline, +1 for '\0'
    if (!result_buffer) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }
    memset(result_buffer, '\0', msg_len + 2);

    char encrypt_array[] = {
        'A','B','C','D','E','F','G','H',
        'I','J','K','L','M','N','O','P',
        'Q','R','S','T','U','V','W','X',
        'Y','Z',' '
    };
    int arr_len = 27;

    for (size_t i = 0; i < msg_len; i++) {
        int total = 0;

        // Find index of message char in encrypt_array
        for (int j = 0; j < arr_len; j++) {
            if (encrypt_array[j] == message[i]) {
                total += j;
                break;
            }
        }

        // Find index of key char in encrypt_array
        for (int j = 0; j < arr_len; j++) {
            if (encrypt_array[j] == key[i]) {
                total += j;
                break;
            }
        }

        result_buffer[i] = encrypt_array[total % arr_len];
    }

    result_buffer[msg_len] = '\n';
    result_buffer[msg_len + 1] = '\0';

    return result_buffer;
}


void handleClient(int connectionSocket) {
    char keyBuffer[MAX_BUFFER_SIZE], msgBuffer[MAX_BUFFER_SIZE], encryptedBuffer[MAX_BUFFER_SIZE];

    memset(msgBuffer, '\0', MAX_BUFFER_SIZE);
    memset(keyBuffer, '\0', MAX_BUFFER_SIZE);
    memset(encryptedBuffer, '\0', MAX_BUFFER_SIZE);

    // 1. Handshake check
    char handshake[16];
    memset(handshake, '\0', sizeof(handshake));

    if (recv(connectionSocket, handshake, sizeof(handshake) - 1, 0) < 0) {
        error("SERVER: ERROR reading handshake");
    }

    if (strcmp(handshake, "enc_client") != 0) {
        fprintf(stderr, "SERVER: Rejected connection from unknown client\n");
        close(connectionSocket);
        exit(2);
    }

    // 2. Send handshake response
    const char* handshakeResponse = "enc_server";
    if (send(connectionSocket, handshakeResponse, strlen(handshakeResponse), 0) < 0) {
        error("SERVER: ERROR sending handshake response");
    }

    // // 3. Receive message and key
    // if (recv(connectionSocket, msgBuffer, 255, 0) < 0)
    //     error("SERVER: ERROR reading message");

    // if (recv(connectionSocket, keyBuffer, 255, 0) < 0)
    //     error("SERVER: ERROR reading key");

    int msgRead = recv(connectionSocket, msgBuffer, sizeof(msgBuffer) - 1, 0);
    if (msgRead < 0)
        error("SERVER: ERROR reading message");
    msgBuffer[msgRead] = '\0'; // Null-terminate

    // printf("msgBuffer: \"%s\"\n", msgBuffer);

    int keyRead = recv(connectionSocket, keyBuffer, sizeof(keyBuffer) - 1, 0);
    if (keyRead < 0)
        error("SERVER: ERROR reading key");
    keyBuffer[keyRead] = '\0'; 
    // printf("keyBuffer: \"%s\"\n", keyBuffer);

    // 4. Encrypt and send result
    // strcpy(encryptedBuffer, encryption(msgBuffer, keyBuffer));
    // strcat(encryptedBuffer, "\n");

    // if (send(connectionSocket, encryptedBuffer, strlen(encryptedBuffer), 0) < 0)
    //     error("SERVER: ERROR writing to socket");

    char* encrypted = encryption(msgBuffer, keyBuffer);  // Returns a null-terminated string
    int encryptedLength = strlen(encrypted);
    

    if (send(connectionSocket, encrypted, encryptedLength, 0) < 0)
    error("SERVER: ERROR writing to socket");

    close(connectionSocket);
}

// Signal handler to reap zombies
void handle_sigchld(int sig) {
    (void)sig;  
    while (waitpid(-1, NULL, WNOHANG) > 0) {
        
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "USAGE: %s port\n", argv[0]);
        exit(1);
    }
    struct sigaction sa;
    sa.sa_handler = handle_sigchld;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    int listenSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (listenSocket < 0) error("ERROR opening socket");

    struct sockaddr_in serverAddress;
    setupAddressStruct(&serverAddress, atoi(argv[1]));

    if (bind(listenSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0)
        error("ERROR on binding");

    listen(listenSocket, 5);

    // Create process pool of 5 children
    for (int i = 0; i < 5; i++) {
        pid_t pid = fork();
        if (pid < 0) error("ERROR on fork");

        if (pid == 0) {
            // Child process: accept and handle clients in a loop
            while (1) {
                struct sockaddr_in clientAddress;
                socklen_t sizeOfClientInfo = sizeof(clientAddress);

                int connectionSocket = accept(listenSocket, (struct sockaddr*)&clientAddress, &sizeOfClientInfo);
                if (connectionSocket < 0) {
                    perror("ERROR on accept");
                    continue;  // try again
                }

                // printf("Child %d: Handling new connection...\n", getpid());
                handleClient(connectionSocket);
            }
            exit(0);
        }
        // Parent continues to next fork
    }

    // Parent process just waits forever
    while (1) pause();

    close(listenSocket);
    return 0;
}
