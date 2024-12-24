#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <errno.h>
#include <cstring>
#include <string>
#include <vector>
#include <map>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>

#define PORT 8080
#define BUFFER_SIZE 255


int setupAllWriteBuffers(std::string &message, std::vector<int> &clients, int clientCount, std::map<int, std::string> &writerBuffers, int epollFD);

std::string serialize(){
    
}

int createEpoll(int serverSocket);

int createSerwer(int port);

int main(){


    int serverSocket = createSerwer(PORT);
    if(serverSocket < 0){
        perror("createSerwer"); 
        return 1;
    }

    int epollFD = createEpoll(serverSocket);
    if(serverSocket < 0){
        perror("createEpoll"); 
        return 1;
    }

    // Client Socket variables 
    sockaddr_in clientAddres;
    socklen_t clientAddresSize = sizeof clientAddres;
    int clientSocket;
    std::vector<int> clients;
    std::map<int, std::string> readBuffers;
    std::map<int, std::string> writeBuffers;
    int clientsCount = 0;

    // Game variables
    int playerCount = 0;
    std::map<int, std::string> names;
    std::map<int, int> points;
    bool ifGameRunning = false;

    // Reading variables
    epoll_event ee;
    int returnValue;
    std::string message = "";
    char buff[BUFFER_SIZE];
    while(true){
        returnValue = epoll_wait(epollFD, &ee, 1, -1);
        if(returnValue < 0){
            perror("epoll_wait"); 
            return 1;
        }

        if(ee.data.fd == serverSocket){
            // New client
            clientSocket = accept(serverSocket, (sockaddr*)&clientAddres, &clientAddresSize);
            if(clientSocket < 0){
                perror("accept"); 
                return 1;
            }

            clients.push_back(clientSocket);
            readBuffers[clientSocket] = "";
            writeBuffers[clientSocket] = "";
            clientsCount++;

            ee.events = EPOLLIN;
            ee.data.fd = clientSocket;
            returnValue = epoll_ctl(epollFD, EPOLL_CTL_ADD, clientSocket, &ee);
            if(returnValue < 0){
                perror("accept"); 
                return 1;
            }

            printf("Client connected.\n");
        }else if(ee.events == EPOLLIN){
            // Message from client

        }else if(ee.events == EPOLLOUT){
            // Message to client
        }
    }
}

int setupAllWriteBuffers(std::string &message, std::vector<int> &clients, int clientCount, std::map<int, std::string> &writerBuffers, int epollFD){
    for(int i = 0; i < clientCount; i++){
        if(writerBuffers[clients[i]] == ""){
            epoll_event temp;
            temp.events = EPOLLOUT;
            temp.data.fd = clients[i];

            int returnValue = epoll_ctl(epollFD, EPOLL_CTL_ADD, clients[i], &temp);
            if(returnValue < 0){
                perror("epoll_ctl"); 
                return 1;
            }

        }
        writerBuffers[clients[i]] += message;
    }
    return 0;
}

int createEpoll(int serverSocket){
    int epollFD = epoll_create1(0);
    if(epollFD < 0){
        perror("epoll_create1"); 
        return -1;
    }

    epoll_event ee;
    ee.events = EPOLLIN;
    ee.data.fd = serverSocket;
    int returnValue = epoll_ctl(epollFD, EPOLL_CTL_ADD, serverSocket, &ee);
    if(returnValue < 0){
        perror("epoll_ctl"); 
        return -1;
    }

    return epollFD;
}

int createSerwer(int port){
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if(serverSocket < 0){
        perror("socket"); 
        return -1;
    }

    int on = 1;
    int returnValue = setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &on, sizeof on);
    if(returnValue < 0){
        perror("setsockopt"); 
        return -1;
    }

    sockaddr_in serverAddres;
    memset(&serverAddres, 0, sizeof(serverAddres));
    serverAddres.sin_family = AF_INET;
    serverAddres.sin_port = htons(port);
    serverAddres.sin_addr.s_addr = htonl(INADDR_ANY);
    socklen_t serverAddresSize = sizeof serverAddres;
    returnValue = bind(serverSocket, (sockaddr*)&serverAddres, serverAddresSize);
    if(returnValue < 0){
        perror("bind"); 
        return -1;
    }

    returnValue = listen(serverSocket, 10);
    if(returnValue < 0){
        perror("listen"); 
        return -1;
    }

    return serverSocket;
}