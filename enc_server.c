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
    size_t msg_len = strlen(message); // includes the newline at the end
    char* result_buffer = malloc(msg_len + 1);  // +1 for '\0' only
    if (!result_buffer) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }
    memset(result_buffer, '\0', msg_len + 1);

    char encrypt_array[] = {
        'A','B','C','D','E','F','G','H',
        'I','J','K','L','M','N','O','P',
        'Q','R','S','T','U','V','W','X',
        'Y','Z',' '
    };
    int arr_len = 27;

    for (size_t i = 0; i < msg_len; i++) {
        if (message[i] == '\n') {
        result_buffer[i] = '\n'; // Preserve newline
        continue;
    }

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

    // Just add null terminator, no extra newline
    result_buffer[msg_len] = '\0';

    return result_buffer;
}

ssize_t recvAll(int socket, char *buffer, size_t length) {
    size_t totalReceived = 0;
    while (totalReceived < length) {
        ssize_t bytesReceived = recv(socket, buffer + totalReceived, length - totalReceived, 0);
        if (bytesReceived < 0) {
            error("SERVER: ERROR receiving data");
        } else if (bytesReceived == 0) {
            break; // Client closed connection
        }
        totalReceived += bytesReceived;
    }
    return totalReceived;
}

ssize_t sendAll(int socket, const char *buffer, size_t length) {
    size_t totalSent = 0;
    while (totalSent < length) {
        ssize_t bytesSent = send(socket, buffer + totalSent, length - totalSent, 0);
        if (bytesSent < 0) {
            error("SERVER: ERROR sending data");
        } else if (bytesSent == 0) {
            break; // Connection closed
        }
        totalSent += bytesSent;
    }
    return totalSent;
}


void handleClient(int connectionSocket) {
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

    //expect data that will be transmit
    int msgSize;
    if (recv(connectionSocket, &msgSize, sizeof(msgSize), 0) < 0) {
        error("SERVER: ERROR receiving message size");
    }
    // Allocate buffer dynamically based on received size
    char *msgBuffer = malloc(msgSize + 1); // +1 for null termination
    if (!msgBuffer) {
        error("SERVER: ERROR allocating memory");
    }
    char *keyBuffer = malloc(msgSize + 1); // +1 for null termination
    if (!keyBuffer) {
        error("SERVER: ERROR allocating memory");
    }

    int msgRead = recvAll(connectionSocket,msgBuffer, msgSize);
    msgBuffer[msgRead] = '\0'; // Null-terminate

    int keyRead = recvAll(connectionSocket, keyBuffer,msgSize);
    keyBuffer[keyRead] = '\0'; 

    char* encrypted = encryption(msgBuffer, keyBuffer);  // Returns a null-terminated string
    int encryptedLength = strlen(encrypted);
    
    sendAll(connectionSocket,encrypted,msgSize);

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
