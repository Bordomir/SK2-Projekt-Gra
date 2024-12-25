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

#include "nlohmann/json.hpp"

#define PORT 8080
#define BUFFER_SIZE 255

using json = nlohmann::json;
using namespace nlohmann::literals;

int setupAllWriteBuffers(std::string &message, std::vector<int> &clients, int clientCount, std::map<int, std::string> &writerBuffers, int epollFD);

int connectNewClient(int serverSocket, std::vector<int> &clients, int &clientsCount, std::map<int, std::string> &readBuffers, std::map<int, std::string> &writeBuffers, int epollFD);

int createEpoll(int serverSocket);

int createSerwer(int port);

int main(){
    // Creating listenning TCP serwer socket with SO_REUSEADDR option an binded to INADDR_ANY adress on PORT 
    int serverSocket = createSerwer(PORT);
    if(serverSocket < 0) return 1;

    // Creating epoll with added listening serwer socket
    int epollFD = createEpoll(serverSocket);
    if(epollFD < 0) return 1;

    // Variables used for receiving and sending data to and from clients
    std::vector<int> clients;
    int clientsCount = 0;
    std::map<int, std::string> readBuffers;
    std::map<int, std::string> writeBuffers;
    std::string message = "";
    char buff[BUFFER_SIZE];

    // Game variables
    // int playerCount = 0;
    // std::map<int, std::string> names;
    // std::map<int, int> points;
    // bool ifGameRunning = false;

    // Other variables
    epoll_event ee;
    int returnValue;
    while(true){
        returnValue = epoll_wait(epollFD, &ee, 1, -1);
        if(returnValue < 0){
            perror("epoll_wait"); 
            return 1;
        }

        if(ee.data.fd == serverSocket){
            // New client
            returnValue = connectNewClient(serverSocket, clients, clientsCount, readBuffers, writeBuffers, epollFD);
            if(returnValue < 0) return 1;
        }else if(ee.events & EPOLLIN){
            memset(buff, 0, BUFFER_SIZE);
            returnValue = read(ee.data.fd, &buff, BUFFER_SIZE);
            if(returnValue < 0){
                perror("read"); 
                return 1;
            }

            message = buff;
            message = message.substr(0,returnValue);
            if(message[message.length()-1] == '\n'){
                message = message.substr(0,message.length()-1);
            }

            if(message == "exit" || message == ""){
                // Client disconnects
                for(int i = 0; i < clientsCount; i++){
                    if(clients[i] == ee.data.fd){
                        clients.erase(clients.begin() + i);
                        break;
                    }
                }
                readBuffers[ee.data.fd] = "";
                writeBuffers[ee.data.fd] = "";
                clientsCount--;

                returnValue = epoll_ctl(epollFD, EPOLL_CTL_DEL, ee.data.fd, NULL);
                if(returnValue < 0){
                    perror("epoll_ctl"); 
                    return 1;
                }

                returnValue = close(ee.data.fd);
                if(returnValue < 0){
                    perror("close"); 
                    return 1;
                }

                // Debug info
                printf("Client on descriptor:%d has disconnected\n", ee.data.fd);
            }else{
                // Message from client

                // Debug info
                printf("New message from client on descriptor:%d:%s\n", ee.data.fd, message.data());

                readBuffers[ee.data.fd] += message;

                // Check if entire message has been received
                int pos = readBuffers[ee.data.fd].find("$");
                while(pos > -1){
                    std::string data = readBuffers[ee.data.fd].substr(0, pos);
                    readBuffers[ee.data.fd] = readBuffers[ee.data.fd].substr(pos+1);

                    // Debug info
                    printf("New data from client on descriptor:%d:%s\n", ee.data.fd, data.data());
                    printf("The rest on readBuffers[%d]:%s\n", ee.data.fd, readBuffers[ee.data.fd].data());

                    // Data service
                    // TODO
                    json dataJSON = json::parse(data);
                    printf("Json object in data:%s\n", dataJSON.dump().data());

                    std::string temp = "";
                    char temp2[1024];
                    sprintf(temp2, "Message from client on descriptor:%d:\n%s$\n", ee.data.fd, dataJSON.dump().data());
                    temp = temp2;
                    returnValue = setupAllWriteBuffers(temp, clients, clientsCount, writeBuffers, epollFD);
                    if (returnValue < 0) return 1;
                    
                    pos = readBuffers[ee.data.fd].find("$");
                }
            }

        }else if(ee.events & EPOLLOUT){
            // Message to client

            if(writeBuffers[ee.data.fd] == ""){
                ee.events = EPOLLIN;
                returnValue = epoll_ctl(epollFD, EPOLL_CTL_MOD, ee.data.fd, &ee);
                if(returnValue < 0){
                    perror("epoll_ctl"); 
                    return 1;
                }
                continue;
            }

            message = writeBuffers[ee.data.fd];

            returnValue = write(ee.data.fd, message.data(), message.size());
            if(returnValue < 0){
                perror("write"); 
                return 1;
            }

            // Debug info
            printf("Message sent to client on descriptor:%d:%s\n", ee.data.fd, message.substr(0, returnValue).data());

            message = message.substr(returnValue);
            writeBuffers[ee.data.fd] = message;

            // Check if no more messages are to be send
            if(message == ""){
                ee.events = EPOLLIN;
                returnValue = epoll_ctl(epollFD, EPOLL_CTL_MOD, ee.data.fd, &ee);
                if(returnValue < 0){
                    perror("epoll_ctl"); 
                    return 1;
                }
            }
        }
    }
}

int setupAllWriteBuffers(std::string &message, std::vector<int> &clients, int clientsCount, std::map<int, std::string> &writeBuffers, int epollFD){
    for(int i = 0; i < clientsCount; i++){
        if(writeBuffers[clients[i]] == ""){
            epoll_event ee;
            ee.events = EPOLLIN | EPOLLOUT;
            ee.data.fd = clients[i];

            int returnValue = epoll_ctl(epollFD, EPOLL_CTL_MOD, clients[i], &ee);
            if(returnValue < 0){
                perror("epoll_ctl"); 
                return 1;
            }

        }
        writeBuffers[clients[i]] += message;
    }
    return 0;
}

int connectNewClient(int serverSocket, std::vector<int> &clients, int &clientsCount, std::map<int, std::string> &readBuffers, std::map<int, std::string> &writeBuffers, int epollFD){
    sockaddr_in clientAddres;
    socklen_t clientAddresSize = sizeof clientAddres;

    int clientSocket = accept(serverSocket, (sockaddr*)&clientAddres, &clientAddresSize);
    if(clientSocket < 0){
        perror("accept"); 
        return -1;
    }

    clients.push_back(clientSocket);
    readBuffers[clientSocket] = "";
    writeBuffers[clientSocket] = "";
    clientsCount++;

    epoll_event ee;
    ee.events = EPOLLIN;
    ee.data.fd = clientSocket;
    int returnValue = epoll_ctl(epollFD, EPOLL_CTL_ADD, clientSocket, &ee);
    if(returnValue < 0){
        perror("epoll_ctl");
        return -1;
    }

    // Debug info
    printf("Client on descriptor:%d has connected\n", clientSocket);
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