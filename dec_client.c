#include <netdb.h>      // gethostbyname()
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h> // send(),recv()
#include <sys/types.h>  // ssize_t
#include <unistd.h>
#include <ctype.h>

#define MAX_BUFFER_SIZE 1000

void error(const char *msg) {
    perror(msg);
    exit(1);
}

void printErrorAndExit(const char *msg, int exitCode) {
    fprintf(stderr, "%s\n", msg);
    exit(exitCode);
}


char* readFile(const char* filename, size_t* out_size) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        perror("fopen");
        exit(1);
    }

    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    if (file_size < 0) {
        perror("ftell");
        fclose(fp);
        exit(1);
    }
    fseek(fp, 0, SEEK_SET);

    char *buffer = malloc(file_size + 1);
    if (!buffer) {
        perror("malloc");
        fclose(fp);
        exit(1);
    }

    size_t read_bytes = fread(buffer, 1, file_size, fp);
    if (read_bytes != (size_t)file_size) {
        fprintf(stderr, "Could not read entire file\n");
        free(buffer);
        fclose(fp);
        exit(1);
    }
    buffer[read_bytes] = '\0';

    fclose(fp);
    if (out_size) *out_size = read_bytes;
    return buffer;
}


int validateText(const char *text) {
    for (int i = 0; text[i] != '\0'; i++) {
        if (text[i] != ' ' && text[i] != '\n' && (text[i] < 'A' || text[i] > 'Z')) {
            return 0;
        }
    }
    return 1;
}

void setupAddressStruct(struct sockaddr_in* address, int portNumber, char* hostname) {
    memset((char*)address, '\0', sizeof(*address));
    address->sin_family = AF_INET;
    address->sin_port = htons(portNumber);

    struct hostent* hostInfo = gethostbyname(hostname);
    if (hostInfo == NULL) {
        fprintf(stderr, "CLIENT: ERROR, no such host\n");
        exit(2);
    }

    memcpy((char*)&address->sin_addr.s_addr,
           hostInfo->h_addr_list[0],
           hostInfo->h_length);
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


int main(int argc, char *argv[]) {
    int socketFD, charsWritten, charsRead;
    struct sockaddr_in serverAddress;
    // char ciphertextBuffer[MAX_BUFFER_SIZE];
    // char keyBuffer[MAX_BUFFER_SIZE];
    char buffer[MAX_BUFFER_SIZE];

    // Check usage & args
    if (argc != 4) {
        fprintf(stderr, "USAGE: %s ciphertext key port\n", argv[0]);
        exit(1);
    }

    // Read ciphertext and key files
    // readFileToBuffer(argv[1], ciphertextBuffer, sizeof(ciphertextBuffer));
    // readFileToBuffer(argv[2], keyBuffer, sizeof(keyBuffer));

    size_t ciphertext_len;
    size_t keytext_len;

    char *ciphertextBuffer = readFile(argv[1], &ciphertext_len);
    char *keyBuffer = readFile(argv[2], &keytext_len);

    // Validate ciphertext and key characters
    if (!validateText(ciphertextBuffer)) {
        fprintf(stderr, "Error: ciphertext contains invalid characters\n");
        exit(1);
    }
    if (!validateText(keyBuffer)) {
        fprintf(stderr, "Error: key contains invalid characters\n");
        exit(1);
    }

    // Check that key is at least as long as ciphertext
    if (strlen(keyBuffer) < strlen(ciphertextBuffer)) {
        fprintf(stderr, "Error: key is too short\n");
        exit(1);
    }

    // Create a socket
    socketFD = socket(AF_INET, SOCK_STREAM, 0);
    if (socketFD < 0) {
        error("CLIENT: ERROR opening socket");
    }

    // Set up the server address struct
    setupAddressStruct(&serverAddress, atoi(argv[3]), "localhost");

    // Connect to server
    if (connect(socketFD, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
        fprintf(stderr, "Error: could not contact otp_dec_d on port %s\n", argv[3]);
        close(socketFD);
        exit(2);
    }

    // Send client ID to server and wait for confirmation
    memset(buffer, '\0', sizeof(buffer));
    strcpy(buffer, "dec_client");
    charsWritten = send(socketFD, buffer, strlen(buffer), 0);
    if (charsWritten < 0) {
        error("CLIENT: ERROR writing to socket");
    }




    // Receive server response
    memset(buffer, '\0', sizeof(buffer));
    charsRead = recv(socketFD, buffer, sizeof(buffer) - 1, 0);
    if (charsRead < 0) {
        error("CLIENT: ERROR reading from socket");
    }

    if (strcmp(buffer, "dec_server") != 0) {
        fprintf(stderr, "Error: connected to wrong server type on port %s\n", argv[3]);
        close(socketFD);
        exit(2);
    }

     // Sends the expected size of the message
    int msgSize = strlen(ciphertextBuffer);  // Get message length

    charsWritten = sendAll(socketFD, (char*)&msgSize, sizeof(msgSize)); // Send as raw bytes


     //send cipher text

    // memset(buffer, '\0', sizeof(buffer));
    // strcpy(buffer, ciphertextBuffer);
    charsWritten = sendAll(socketFD, ciphertextBuffer, ciphertext_len);

    // send key 

    
    // memset(buffer, '\0', sizeof(buffer));
    // strcpy(buffer, keyBuffer);
    charsWritten = sendAll(socketFD, keyBuffer, keytext_len);

    // receive ciphertext 

    char *plaintext_buffer = malloc(ciphertext_len + 1);

    // memset(buffer, '\0', sizeof(buffer));
    charsRead = recvAll(socketFD, plaintext_buffer, ciphertext_len);
    fwrite(plaintext_buffer,1,charsRead,stdout);
    fflush(stdout);

    // printf("Buffer: \"%s\"\n", buffer);
    close(socketFD);

    return 0;
}
