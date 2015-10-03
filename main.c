#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "netinet/in.h"
#include <fcntl.h>
#include "signal.h"
#include <unistd.h>
#include <errno.h>

#define BACKLOG 64
#define BUFFER_SIZE 8096
#define MAX_REQUESTS 100

//Global variables.
int port, mode, processAmount, socketfd;
char resourcePath[200];
char userInput[10];

socklen_t socketLength;
static struct sockaddr_in clientAddr, serverAddr;

//Variables for threads
pthread_mutex_t cliMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cliCond = PTHREAD_COND_INITIALIZER;
int	get, processed;
int requests[MAX_REQUESTS];

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

    socketLength = sizeof(clientAddr);
}

int isFin() {
    return (strlen(userInput) == 3 && userInput[0] == 'f' && userInput[1] == 'i' && userInput[2] == 'n');
}

const char *getResourceString(int bytesRead, char* buffer) {
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

void* processRequest(void* fd) {
    char buffer[BUFFER_SIZE + 1];
    int requestfd = *((int *) fd);
    int bytesRead, filefd;
    bytesRead = read(requestfd, buffer, BUFFER_SIZE);
    if(bytesRead < 1) {
        printf("Oh dear, something went wrong with read()! %s\n", strerror(errno));
        printf("Error al leer la solicitud, devolver error http. \n\n");
        return NULL;
    }
    if(bytesRead > 0 && bytesRead < BUFFER_SIZE) {
        buffer[bytesRead] = 0;
    }

    if((filefd = open(getResourceString(bytesRead, buffer), O_RDONLY)) == -1) {
		printf("Error al abrir el recurso, devolver error http \n\n");
	}

	while((bytesRead = read(filefd, buffer, BUFFER_SIZE)) > 0) {
		(void) write(requestfd, buffer, bytesRead);
	}

	close(requestfd);
	return NULL;
}

void fifo() {
    int connectionfd;
    while(1) {
        if((connectionfd = accept(socketfd, (struct sockaddr *) &clientAddr, &socketLength)) == -1) {
            printf("Error al aceptar la conexión.\n\n");
        }else {
            processRequest((void *)&connectionfd);
        }
    }
}

void forked() {
    int connectionfd;
    pid_t processId;
    while(1) {
        if ((connectionfd = accept(socketfd, (struct sockaddr *) &clientAddr, &socketLength)) == -1) {
            printf("Error al aceptar la conexión.\n\n");
        }else {
            if ((processId = fork()) < 0) {
                printf("Error al crear el proceso.\n\n");
            }
            else if (processId == 0) {
                processRequest((void *)&connectionfd);
                exit(0);
            }else {
                close(connectionfd);
            }
        }
    }
}

void threaded() {
    int *confd, connectionfd;
    while(1){
        if ((connectionfd = accept(socketfd, (struct sockaddr *) &clientAddr, &socketLength)) == -1) {
            printf("Error al aceptar la conexión.\n\n");
        }else {
            confd = malloc(sizeof(int));
            *confd = connectionfd;
            pthread_t thread;
            if(pthread_create(&thread, NULL, &processRequest, (void*) confd)) {
                close(connectionfd);
                printf("Error al crear nuevo hilo\n");
                exit(0);
            }
        }
    }
}

void preForked() {
    int connectionfd;
    pid_t processIds[processAmount];
    for(int i = 0; i < processAmount; i++) {
        if ((processIds[i] = fork()) < 0) {
                printf("Error al crear el proceso.\n\n");
            }
        else if (processIds[i] == 0) {
            while(1) {
                if((connectionfd = accept(socketfd, (struct sockaddr *) &clientAddr, &socketLength)) == -1) {
                    printf("Error al aceptar la conexión.\n\n");
                }else {
                    processRequest((void *)&connectionfd);
                }
            }
        }
    }

    while(scanf("%s", userInput),!isFin());
    for(int i = 0; i < processAmount; i++) kill(processIds[i], SIGTERM);
    exit(0);
}

void* threadRun() {
    int *connectionfd = malloc(sizeof(int));
	pthread_detach(pthread_self());

	while(1){
    	pthread_mutex_lock(&cliMutex);
		while (get == processed) pthread_cond_wait(&cliCond, &cliMutex);
		*connectionfd = requests[get];
		if (++get == MAX_REQUESTS) get = 0;
		pthread_mutex_unlock(&cliMutex);
		processRequest((void *) connectionfd);
	}
    return NULL;
}

void preThreaded() {
    pthread_t threads[processAmount];
    for(int i = 0; i < processAmount; i++) {
        if(pthread_create(&threads[i], NULL, &threadRun, NULL)) {
            printf("Error al crear nuevo hilo\n");
            exit(0);
        }
    }

    get = processed = 0;
    int connectionfd;
    while(1) {
        if((connectionfd = accept(socketfd, (struct sockaddr *) &clientAddr, &socketLength)) == -1) {
            printf("Error al aceptar la conexión.\n\n");
        }
        pthread_mutex_lock(&cliMutex);
        requests[processed] = connectionfd;
        if(++processed == MAX_REQUESTS) processed = 0;
        pthread_cond_signal(&cliCond);
        pthread_mutex_unlock(&cliMutex);
    }
}

int main(int argc, char ** argv) {

    if(argc < 4 || argc > 5) printf("Número inválido de argumentos.\n\n");
    else {
        //Assign arguments.
        port = atoi(argv[1]);
        strcpy(resourcePath, argv[2]);
        mode = atoi(argv[3]);
        if(argc == 5) processAmount = atoi(argv[4]);

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
