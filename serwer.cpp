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
#include "config.hpp"

using json = nlohmann::json;
using namespace nlohmann::literals;

int setupAllWriteBuffers(std::string &message, std::vector<int> &clients, int clientCount, std::map<int, std::string> &writerBuffers, int epollFD);
bool checkIfMessageReady(int clientSocket, json &dataJSON, std::map<int, std::string> &readBuffers);
int checkWriteBuffer(int clientSocket, std::map<int, std::string> &writeBuffers, int epollFD);
int writeToSocket(int clientSocket, std::map<int, std::string> &writeBuffers, int epollFD);
std::string readFromSocket(int clientSocket);
int disconnectClient(int clientSocket, std::vector<int> &clients, int &clientsCount, std::map<int, std::string> &readBuffers, std::map<int, std::string> &writeBuffers, int epollFD);
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
    json dataJSON;

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
            // New message from client
            message = readFromSocket(ee.data.fd);
            if(message == "-1") return 1;

            // Check if connection was lost
            if(message == ""){
                returnValue = disconnectClient(ee.data.fd, clients, clientsCount, readBuffers, writeBuffers, epollFD);
                if(returnValue < 0) return 1;
                continue;   
            }
            // Debug info
            printf("New message from client on descriptor:%d:%s\n", ee.data.fd, message.data());

            readBuffers[ee.data.fd] += message;

            while(checkIfMessageReady(ee.data.fd, dataJSON, readBuffers)){
                // Data Service 
                // TODO

                // Debug info
                printf("Json object in data:%s\n", dataJSON.dump().data());
                std::string txt = dataJSON.dump();
                returnValue = setupAllWriteBuffers(txt, clients, clientsCount, writeBuffers, epollFD);
                if (returnValue < 0) return 1;
            }
        }else if(ee.events & EPOLLOUT){
            returnValue = writeToSocket(ee.data.fd, writeBuffers, epollFD);
            if(returnValue < 0) return -1;
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
        writeBuffers[clients[i]] += "$";
    }
    return 0;
}

bool checkIfMessageReady(int clientSocket, json &dataJSON, std::map<int, std::string> &readBuffers){
    int pos = readBuffers[clientSocket].find("$");
    bool ifReady = (pos > -1);

    if(ifReady){
        std::string data = readBuffers[clientSocket].substr(0, pos);
        readBuffers[clientSocket] = readBuffers[clientSocket].substr(pos+1);

        // Debug info
        printf("New data from client on descriptor:%d:%s\n", clientSocket, data.data());
        printf("The rest on readBuffers[%d]:%s\n", clientSocket, readBuffers[clientSocket].data());

        dataJSON = json::parse(data);
        return true;        
    }
    return false;
}

int checkWriteBuffer(int clientSocket, std::map<int, std::string> &writeBuffers, int epollFD){
    if(writeBuffers[clientSocket] == ""){
        epoll_event ee;
        ee.events = EPOLLIN;
        ee.data.fd = clientSocket;
        int returnValue = epoll_ctl(epollFD, EPOLL_CTL_MOD, clientSocket, &ee);
        if(returnValue < 0){
            perror("epoll_ctl"); 
            return -1;
        }
    }
    return 0;
}

int writeToSocket(int clientSocket, std::map<int, std::string> &writeBuffers, int epollFD){
    // Check if buffer is empty (no messages to be sent)
    int returnValue = checkWriteBuffer(clientSocket, writeBuffers, epollFD);
    if(returnValue < 0) return -1;

    std::string message = writeBuffers[clientSocket];

    returnValue = write(clientSocket, message.substr(0,BUFFER_SIZE).data(), message.substr(0,BUFFER_SIZE).size());
    if(returnValue < 0){
        perror("write"); 
        return 1;
    }

    // Debug info
    printf("Message sent to client on descriptor:%d:%s\n", clientSocket, message.substr(0, returnValue).data());

    message = message.substr(returnValue);
    writeBuffers[clientSocket] = message;
    
    // Check if buffer is empty (no messages to be sent)
    returnValue = checkWriteBuffer(clientSocket, writeBuffers, epollFD);
    if(returnValue < 0) return -1;

    return 0;
}

std::string readFromSocket(int clientSocket){
    char buff[BUFFER_SIZE+1];
    memset(buff, 0, BUFFER_SIZE);
    int length = read(clientSocket, &buff, BUFFER_SIZE);
    if(length < 0){
        perror("read"); 
        return "-1";
    }

    std::string message = buff;
    message = message.substr(0, length);
    if(message[message.length()-1] == '\n'){
        message = message.substr(0,message.length()-1);
    }

    return message;
}

int disconnectClient(int clientSocket, std::vector<int> &clients, int &clientsCount, std::map<int, std::string> &readBuffers, std::map<int, std::string> &writeBuffers, int epollFD){
    for(int i = 0; i < clientsCount; i++){
        if(clients[i] == clientSocket){
            clients.erase(clients.begin() + i);
            break;
        }
    }
    readBuffers[clientSocket] = "";
    writeBuffers[clientSocket] = "";
    clientsCount--;

    int returnValue = epoll_ctl(epollFD, EPOLL_CTL_DEL, clientSocket, NULL);
    if(returnValue < 0){
        perror("epoll_ctl"); 
        return -1;
    }

    returnValue = close(clientSocket);
    if(returnValue < 0){
        perror("close"); 
        return -1;
    }

    // Debug info
    printf("Client on descriptor:%d has disconnected\n", clientSocket);
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
