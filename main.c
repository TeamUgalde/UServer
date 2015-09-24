#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "netinet/in.h"

#define BACKLOG 64

//Global variables.
int port, mode, processAmount, fd, socketfd, requestfd;
char resourcePath[200];

socklen_t socketLength;
static struct sockaddr_in clientAddr, serverAddr;


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

void fifo() {}
void forked() {}
void threaded() {}
void preForked() {}
void preThreaded() {}


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
