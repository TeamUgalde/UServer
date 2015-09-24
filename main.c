#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "netinet/in.h"

#define BACKLOG 64
#define BUFFER_SIZE 8096;

//Global variables.
int port, mode, processAmount, fd, socketfd, requestfd;
char resourcePath[300];

socklen_t socketLength;
static struct sockaddr_in clientAddr, serverAddr;

static char buffer[BUFFER_SIZE + 1];


void initializeServer() {
    if((socketfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        printf("Error al abrir el socket.\n");
        exit(-1);
    }

    //Initialize server adress structure
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    //Bind a port with the given socket.
    if(bind(socketfd, (struct sockaddr *) &serverAddr, sizeof(serverAddr)) == -1) {
        printf("No se pudo ligar el socket con el puerto %d.n", port);
        exit(-1);
    }

    //Enable the socket to accept connections.
    if(listen(socketfd, BACKLOG) == -1) {
        printf("Error al intentar habilitar la recepción de conexiones.\n");
        exit(-1);
    }
}

char[] getResourceString(int bytesRead) {
    for(int i = 0; i < bytesRead; i++) {
        if(buffer[i] == '\n' || buffer[i] == '\r') buffer[i]='$';
    }
    int index = 0;
    char resource[200];
    for(int i = 4; i < BUFFER_SIZE; i++) {
		if(buffer[i] == ' ') break;
		resource[index++] = buffer[i];
	}
	return resource;
}

void processRequest(void* fd) {
    int requestfd = *((int *)) fd);
    int bytesRead, filefd;
    bytesRead = read(requestfd, buffer, BUFFER_SIZE);

    if(bytesRead < 1) {
        printf("Error al leer la solicitud, devolver error http");
    }
    if(bytesRead > 0 && bytesRead < BUFFER_SIZE) {
        buffer[bytesRead] = 0;
    }

    char resource[100];
    strcpy(resource, getResourceString(bytesRead));
    strcat(resourcePath, resource);

    if((filefd = open(&resourcePath, O_RDONLY)) == -1) {
		printf("Error al abrir el recurso, devolver error http");
	}
	while((bytesRead = read(filefd, buffer, BUFFER_SIZE)) > 0) {
		(void) write(requestfd, buffer, bytesRead);
	}
	close(requestfd);
	exit(1);
}

void fifo() {

}

void forked() {

}

void threaded() {

}

void preForked() {

}

void preThreaded() {

}


int main(int argc, char ** argv) {

    if(argc < 4 || argc > 5) printf("Número inválido de argumentos.\n");
    else {

        //Assign arguments.
        port = atoi(argv[1]);
        strcpy(resourcePath, argv[2]);
        mode = atoi(argv[3]);
        if(argc == 5) processAmount = atoi(argv[5]);

        initializeServer();

        switch(mode) {
            case 1:
                fifo();
                break;
            case 2:
                forked();
                break;
            case 3:
                threaded();
                break;
            case 4:
                preForked();
                break;
            case 5:
                preThreaded();
                break;
            default :
                printf("Modo inválido.\n");
        }
    }
    return 0;
}
