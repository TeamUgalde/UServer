#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "netinet/in.h"
#include <fcntl.h>
#include "signal.h"
#include <unistd.h>
#include "uthash.h"
#include <sys/mman.h>

#define BACKLOG 64
#define BUFFER_SIZE 8096
#define MAX_REQUESTS 100

//File name and the number of times that it has been requested.
typedef struct file {
    char  name[30];
    int count;
    UT_hash_handle hh;
}file_t;

//Global variables.
int port, mode, processAmount, socketfd;
char resourcePath[200];
char userInput[10];
socklen_t socketLength;
static struct sockaddr_in clientAddr, serverAddr;

//Variables for threads work.
pthread_mutex_t cliMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mapMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cliCond = PTHREAD_COND_INITIALIZER;
int	get, processed;
int requests[MAX_REQUESTS];

//FileMap using uthash. It stores files information.
static file_t *fileMap;


//-------------------------------------------------------------------------------------
//Returns the number of times that a file has been requested.
int getFileRequests(char *requestedFile) {
    file_t *f;
    int res;

    HASH_FIND_STR(fileMap, requestedFile, f);
    if (f==NULL) {
      f = (file_t*)malloc(sizeof(file_t));
      f->count = res = 1;
      strcpy(f->name, requestedFile);
      HASH_ADD_STR(fileMap, name, f);
    }else{
        res = ++f->count;
    }
    return res;
}


//-------------------------------------------------------------------------------------
// Prints requested file information.
void printRequestInfo(char *resource) {
    if(mode != 2 && mode != 4) {
        pthread_mutex_lock(&mapMutex);
        printf("Se solicitó el recurso: %s (%d)\n", resource, getFileRequests(resource));
        pthread_mutex_unlock(&mapMutex);
    }else {
        printf("Se solicitó el recurso: %s\n", resource);
    }
}


//-------------------------------------------------------------------------------------
// Initializes the server socket. After this function the server will be listening for requests.
void initializeServer() {
    //Request a new socket.
    if((socketfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        printf("Error al abrir el socket del servidor.\n\n");
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

    //This variable will be needed when accepting requests.
    socketLength = sizeof(clientAddr);
}


//-------------------------------------------------------------------------------------
// Returns 1 if the userInput is equals to 'fin'
int isFin() {
    return (strlen(userInput) == 3 && userInput[0] == 'f' && userInput[1] == 'i' && userInput[2] == 'n');
}


//-------------------------------------------------------------------------------------
// Returns the whole path to the requested resource.
const char *getResourcePath(char* resource) {
    char *path = (char*) malloc(400 * sizeof(char));
    strcat(path, resourcePath);
	strcat(path, resource);
	return path;
}


//-------------------------------------------------------------------------------------
// Returns a string containing the name of the requested resource.
char *getResourceString(int bytesRead, char* buffer) {
    for(int i = 0; i < bytesRead; i++) {
        if(buffer[i] == '\n' || buffer[i] == '\r') buffer[i]='$';
    }
    int index = 0;
    char *temp = (char*) malloc(100 * sizeof(char));
    for(int i = 4; i < BUFFER_SIZE; i++) {
		if(buffer[i] == ' ') break;
		temp[index++] = buffer[i];
	}
	temp[index] = 0;
	return temp;
}


//-------------------------------------------------------------------------------------
// This function process the incoming request and writes the response into the client socket.
void* processRequest(void* fd) {
    char buffer[BUFFER_SIZE + 1];
    int requestfd = *((int *) fd);
    int bytesRead, filefd;
    bytesRead = read(requestfd, buffer, BUFFER_SIZE);
    if(bytesRead < 1) {
        printf("Error al leer la solicitud. \n\n");
        return NULL;
    }
    if(bytesRead > 0 && bytesRead < BUFFER_SIZE) buffer[bytesRead] = 0;

    char * resource = getResourceString(bytesRead, buffer);

    if((filefd = open(getResourcePath(resource), O_RDONLY)) == -1) {
        char error[] = "HTTP/1.1 404 Not Found\nContent-Length: 136\nConnection: close\nContent-Type: text/html\n\n<html><head>\n<title>404 Not Found</title>\n</head><body>\n<h1>Not Found</h1>\nThe requested URL was not found on this server.\n</body></html>\n";
        write(requestfd, error, 224);
		printf("Error al abrir el recurso %s.\n", resource);
	}else {
		while((bytesRead = read(filefd, buffer, BUFFER_SIZE)) > 0) {
            (void) write(requestfd, buffer, bytesRead);
        }
	}

	close(requestfd);
    printRequestInfo(resource);
	return NULL;
}


//-------------------------------------------------------------------------------------
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


//-------------------------------------------------------------------------------------
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


//-------------------------------------------------------------------------------------
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
                printf("Error al crear nuevo hilo.\n");
                exit(0);
            }
        }
    }
}


//-------------------------------------------------------------------------------------
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
    while(1) pause();
}


//-------------------------------------------------------------------------------------
//Thread function, it waits for an incoming request.
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


//-------------------------------------------------------------------------------------
//Initializes server and attends requests depending on the mode.
void* startServer() {

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
    return NULL;
}


int main(int argc, char ** argv) {

    if(argc < 4 || argc > 5) printf("Número inválido de argumentos.\n\n");
    else {
        //Assign arguments.
        port = atoi(argv[1]);
        strcpy(resourcePath, argv[2]);
        mode = atoi(argv[3]);
        if(argc == 5) processAmount = atoi(argv[4]);

        //Create server thread.
        pthread_t main_thread;
        pthread_create(&main_thread, NULL, &startServer, NULL);

        //Waits until user enters the exit word.
        while(scanf("%s", userInput),!isFin());
        if(mode == 4) kill(0, SIGTERM);
    }
    return 0;
}
