#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "netinet/in.h"
#include <fcntl.h>
#include "signal.h"
#include <unistd.h>

#define BACKLOG 64
#define BUFFER_SIZE 8096

//Global variables.
int port, mode, processAmount, fd, socketfd, connectionfd;
char resourcePath[200];

socklen_t socketLength;
static struct sockaddr_in clientAddr, serverAddr;

static char buffer[BUFFER_SIZE + 1];


void initializeServer() {
    if((socketfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        printf("Error al abrir el socket.\n\n");
        exit(-1);
    }

    //Initialize server adress structure
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    //Bind a port with the given socket.
    if(bind(socketfd, (struct sockaddr *) &serverAddr, sizeof(serverAddr)) < 0) {
        printf("No se pudo ligar el socket con el puerto %d \n\n", port);
        exit(-1);
    }

    //Enable the socket to accept connections.
    if(listen(socketfd, BACKLOG) == -1) {
        printf("Error al intentar habilitar la recepción de conexiones.\n\n");
        exit(-1);
    }
}

const char *getResourceString(int bytesRead) {
    for(int i = 0; i < bytesRead; i++) {
        if(buffer[i] == '\n' || buffer[i] == '\r') buffer[i]='$';
    }
    int index = 0;
    char *resource = (char*) malloc(400 * sizeof(char));
    char temp[100];
    for(int i = 4; i < BUFFER_SIZE; i++) {
		if(buffer[i] == ' ') break;
		temp[index++] = buffer[i];
	}
	temp[index] = 0;
	strcat(resource, resourcePath);
	strcat(resource, temp);
	return resource;
}

void processRequest(void* fd) {
    int requestfd = *((int *) fd);
    int bytesRead, filefd;
    bytesRead = read(requestfd, buffer, BUFFER_SIZE);

    if(bytesRead < 1) {
        printf("Error al leer la solicitud, devolver error http. \n\n");
    }
    if(bytesRead > 0 && bytesRead < BUFFER_SIZE) {
        buffer[bytesRead] = 0;
    }

    if((filefd = open(getResourceString(bytesRead), O_RDONLY)) == -1) {
		printf("Error al abrir el recurso, devolver error http \n\n");
	}
	while((bytesRead = read(filefd, buffer, BUFFER_SIZE)) > 0) {
		(void) write(requestfd, buffer, bytesRead);
	}
	close(requestfd);
}

void fifo() {
    while(1) {
        socketLength = sizeof(clientAddr);
        if((connectionfd = accept(socketfd, (struct sockaddr *) &clientAddr, &socketLength)) == -1) {
            printf("Error al aceptar la conexión.\n\n");
        }else {
            processRequest((void *)&connectionfd);
            close(connectionfd);
        }
    }
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

    if(argc < 4 || argc > 5) printf("Número inválido de argumentos.\n\n");
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
                printf("Modo inválido.\n\n");
        }
    }
    return 0;
}
