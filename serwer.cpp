#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <errno.h>
#include <cstring>
#include <string>
#include <ctime>
#include <vector>
#include <map>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>

#include <nlohmann/json.hpp>
#include "config.hpp"

using json = nlohmann::json;
using namespace nlohmann::literals;

int endRound(std::vector<int> &clients, std::map<int, std::string> &names, std::map<int, int> &points, 
    std::map<int, int> &rounds, std::map<int, bool> &ifPlaying, std::map<int, std::string> &writeBuffers, int epollFD,
    int &firstPlayer, std::vector<std::string> &categories, std::map<int, json> &answers);
int calculatePoints(int clientSocket, int firstPlayer, std::vector<int> &clients, std::map<int, std::string> &names, std::vector<std::string> &categories, 
    std::map<int, json> &answers, std::map<int, std::string> &writeBuffers, int epollFD);
bool checkifUnique(int clientSocket, std::vector<int> &clients, std::string answer, std::string category, std::map<int, json> &answers);
int startRound(bool &ifGameRunning, std::vector<int> &clients, std::map<int, std::string> &names, std::map<int, int> &points, 
    std::map<int, int> &rounds, std::map<int, bool> &ifPlaying, std::map<int, std::string> &writeBuffers, int epollFD,
    char &currentLetter, time_t &roundStartTime, int &roundEndRequirement, int &submitedAnswers, int playerCount, int &firstPlayer);
void eraseData(int clientSocket, std::map<int, std::string> &names, std::map<int, int> &points, std::map<int, int> &rounds,
    std::map<int, bool> &ifPlaying, std::map<int, json> &answers);
void setAllPlayers(std::vector<int> &clients, std::map<int, bool> &ifPlaying);
char generateRandomLetter();
json getGameData(int playerCount, std::vector<int> &clients, std::map<int, std::string> &names, std::map<int, int> &points, 
    std::map<int, int> &rounds, char currentLetter, time_t roundStartTime, int submitedAnswers, int roundEndRequirement);
std::vector<int> changeToVector(std::vector<int> &clients, std::map<int, int> &map);
std::vector<std::string> changeToVector(std::vector<int> &clients, std::map<int, std::string> &map);
int setupAllWriteBuffers(json &dataJSON, std::vector<int> &clients, std::map<int, std::string> &writerBuffers, int epollFD);
int setupWriteBuffer(json &dataJSON, int clientSocket, std::map<int, std::string> &writeBuffers, int epollFD);
bool checkIfNameRepeated(std::string name, std::map<int, std::string> &names);
bool checkIfMessageReady(int clientSocket, json &dataJSON, std::map<int, std::string> &readBuffers);
int checkWriteBuffer(int clientSocket, std::map<int, std::string> &writeBuffers, int epollFD);
int writeToSocket(int clientSocket, std::map<int, std::string> &writeBuffers, int epollFD);
std::string readFromSocket(int clientSocket);
int disconnectClient(int clientSocket, std::vector<int> &clients, int &clientsCount, std::map<int, std::string> &readBuffers, std::map<int, std::string> &writeBuffers, int epollFD);
int connectNewClient(int serverSocket, std::vector<int> &clients, int &clientsCount, std::map<int, std::string> &readBuffers, std::map<int, std::string> &writeBuffers, int epollFD);
int createEpoll(int serverSocket);
int createSerwer(int port=8080, int listening=10);

int main(){
    // Create listenning TCP serwer socket with SO_REUSEADDR option an binded to INADDR_ANY adress on PORT 
    int serverSocket = createSerwer(PORT, LISTENING);
    if(serverSocket < 0) return 1;

    // Create epoll with added listening serwer socket
    int epollFD = createEpoll(serverSocket);
    if(epollFD < 0) return 1;

    // Initialize random seed
    srand(time(NULL));

    // Variables used for receiving and sending data to and from clients
    std::vector<int> clients;
    int clientsCount = 0;
    std::map<int, std::string> readBuffers;
    std::map<int, std::string> writeBuffers;
    std::string data = "";
    json dataJSON;
    json message;

    // Game variables
    std::vector<std::string> categories = {"panstwo","miasto","rzecz","roslina","zwierze","imie"};
    int playerCount = 0;
    std::map<int, bool> ifPlaying;
    std::map<int, std::string> names;
    std::map<int, int> points;
    std::map<int, int> rounds;
    std::map<int, json> answers;
    // std::map<int, time_t> times;
    int firstPlayer = 0;
    bool ifGameRunning = false;
    char currentLetter;
    time_t roundStartTime;
    int roundEndRequirement = INT16_MAX;
    int submitedAnswers = 0;

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
            // Accept new client, add it to epoll and setup variables for client data
            returnValue = connectNewClient(serverSocket, clients, clientsCount, readBuffers, writeBuffers, epollFD);
            if(returnValue < 0) return 1;

        }else if(ee.events & EPOLLIN){
            // New data from client
            // Read data from socket and truncate it if needed ('\n' at the end)
            data = readFromSocket(ee.data.fd);
            if(data == "-1") return 1;

            // Check if connection was lost
            if(data == ""){
                // Remove client from epoll, close its descriptor and update variables for client data
                returnValue = disconnectClient(ee.data.fd, clients, clientsCount, readBuffers, writeBuffers, epollFD);
                if(returnValue < 0) return 1;

                // Remove player from game and sends message to other players
                playerCount--;
                if(ifGameRunning && ifPlaying[ee.data.fd]){
                    submitedAnswers++;
                }
                message = R"({
                        "type": "disconnected"
                    })"_json;
                message["name"] = names[ee.data.fd];
                message["answers"] = submitedAnswers;
                int returnValue = setupAllWriteBuffers(message, clients, writeBuffers, epollFD);
                if(returnValue < 0) return -1;

                eraseData(ee.data.fd, names, points, rounds, ifPlaying, answers);

                if(ifGameRunning && submitedAnswers >= roundEndRequirement){
                    returnValue = endRound(clients, names, points, rounds, ifPlaying, writeBuffers, epollFD, firstPlayer, categories, answers);
                    if(returnValue < 0) return 1;

                    if(playerCount >= MIN_PLAYER_NUMBER){
                        returnValue = startRound(ifGameRunning, clients, names, points, rounds, ifPlaying, writeBuffers, epollFD, currentLetter,
                            roundStartTime, roundEndRequirement, submitedAnswers, playerCount, firstPlayer);
                        if(returnValue < 0) return 1;
                    }else{
                        ifGameRunning = false;
                    }
                }

                continue;   
            }
            // Debug info
            // printf("New data from client on descriptor:%d:%s\n", ee.data.fd, data.data());

            readBuffers[ee.data.fd] += data;

            // Checks if entire message has been received and writes it to dataJSON variable
            while(checkIfMessageReady(ee.data.fd, dataJSON, readBuffers)){
                // Data Service 
                if(!dataJSON.contains("type")){
                    fprintf(stderr, "Invalid data was received");
                    continue;
                }

                if(dataJSON["type"] == "name"){
                    // Player sends his name
                    if(!dataJSON.contains("name")){
                        fprintf(stderr, "Invalid data was received");
                        continue;
                    }

                    if(checkIfNameRepeated(dataJSON["name"], names)){
                        message = R"({
                                "type": "nameRepeated"
                            })"_json;
                        returnValue = setupWriteBuffer(message, ee.data.fd, writeBuffers, epollFD);
                        if(returnValue < 0) return 1;
                        continue;
                    }

                    message = R"({
                            "type": "nameCorrect"
                        })"_json;
                    returnValue = setupWriteBuffer(message, ee.data.fd, writeBuffers, epollFD);
                    if(returnValue < 0) return 1;

                    if(ifGameRunning){
                        message = getGameData(playerCount, clients, names, points, rounds, currentLetter, roundStartTime, submitedAnswers, roundEndRequirement);
                        returnValue = setupWriteBuffer(message, ee.data.fd, writeBuffers, epollFD);
                        if(returnValue < 0) return 1;
                    }

                    ifPlaying[ee.data.fd] = false;
                    names[ee.data.fd] = dataJSON["name"];
                    points[ee.data.fd] = 0;
                    rounds[ee.data.fd] = 0;
                    answers[ee.data.fd] = json();
                    playerCount++;
                    
                    message = R"({
                            "type": "newPlayer"
                        })"_json;
                    message["name"] = dataJSON["name"];
                    returnValue = setupAllWriteBuffers(message, clients, writeBuffers, epollFD);
                    if(returnValue < 0) return 1;

                    if(playerCount >= MIN_PLAYER_NUMBER){
                        returnValue = startRound(ifGameRunning, clients, names, points, rounds, ifPlaying, writeBuffers, epollFD, currentLetter,
                            roundStartTime, roundEndRequirement, submitedAnswers, playerCount, firstPlayer);
                        if(returnValue < 0) return 1;
                    }
                }else if(dataJSON["type"] == "answers"){
                    // Player sends his answers
                    if(!dataJSON.contains("panstwo")
                        || !dataJSON.contains("miasto")
                        || !dataJSON.contains("rzecz")
                        || !dataJSON.contains("roslina")
                        || !dataJSON.contains("zwierze")
                        || !dataJSON.contains("imie")
                        || !dataJSON.contains("time")){
                        fprintf(stderr, "Invalid data was received");
                        continue;
                    }
                    if(!ifGameRunning) continue;
                    if(dataJSON["time"] != roundStartTime) continue;

                    if(firstPlayer == 0){
                        firstPlayer = ee.data.fd;
                    }

                    answers[ee.data.fd] = dataJSON;
                    submitedAnswers++;

                    message = R"({
                            "type": "answer"
                        })"_json;
                    message["name"] = names[ee.data.fd];
                    returnValue = setupAllWriteBuffers(message, clients, writeBuffers, epollFD);
                    if(returnValue < 0) return 1;
                    
                    if(ifGameRunning && submitedAnswers >= roundEndRequirement){
                        returnValue = endRound(clients, names, points, rounds, ifPlaying, writeBuffers, epollFD, firstPlayer, categories, answers);
                        if(returnValue < 0) return 1;

                        if(playerCount >= MIN_PLAYER_NUMBER){
                            returnValue = startRound(ifGameRunning, clients, names, points, rounds, ifPlaying, writeBuffers, epollFD, currentLetter,
                                roundStartTime, roundEndRequirement, submitedAnswers, playerCount, firstPlayer);
                            if(returnValue < 0) return 1;
                        }else{
                            ifGameRunning = false;
                        }
                    }
                }else if(dataJSON["type"] == "time"){
                    if(!dataJSON.contains("time")){
                        fprintf(stderr, "Invalid data was received");
                        continue;
                    }
                    if(!ifGameRunning) continue;
                    if(dataJSON["time"] != roundStartTime) continue;
                    submitedAnswers++;
                    
                    if(ifGameRunning && submitedAnswers >= roundEndRequirement){
                        returnValue = endRound(clients, names, points, rounds, ifPlaying, writeBuffers, epollFD, firstPlayer, categories, answers);
                        if(returnValue < 0) return 1;

                        if(playerCount >= MIN_PLAYER_NUMBER){
                            returnValue = startRound(ifGameRunning, clients, names, points, rounds, ifPlaying, writeBuffers, epollFD, currentLetter,
                                roundStartTime, roundEndRequirement, submitedAnswers, playerCount, firstPlayer);
                            if(returnValue < 0) return 1;
                        }else{
                            ifGameRunning = false;
                        }
                    }
                }

                // Debug info
                // printf("Json object in data:%s\n", dataJSON.dump().data());
                // returnValue = setupAllWriteBuffers(dataJSON, clients, writeBuffers, epollFD);
                // if (returnValue < 0) return 1;
            }
        }else if(ee.events & EPOLLOUT){
            // Message can be sent
            // Send data from writeBuffer and changes epoll setting if all data was sent
            returnValue = writeToSocket(ee.data.fd, writeBuffers, epollFD);
            if(returnValue < 0) return -1;
        }
    }
}

int endRound(std::vector<int> &clients, std::map<int, std::string> &names, std::map<int, int> &points, 
    std::map<int, int> &rounds, std::map<int, bool> &ifPlaying, std::map<int, std::string> &writeBuffers, int epollFD,
    int &firstPlayer, std::vector<std::string> &categories, std::map<int, json> &answers
){
    json message = R"({
            "type": "end"
        })"_json;
    int returnValue = setupAllWriteBuffers(message, clients, writeBuffers, epollFD);
    if(returnValue < 0) return -1;

    for(const int& client: clients){
        if(!ifPlaying[client]) continue;

        points[client] += calculatePoints(client, firstPlayer, clients, names, categories, answers, writeBuffers, epollFD);
        rounds[client]++;
    }

    sleep(10);
    return 0;
}

int calculatePoints(int clientSocket, int firstPlayer, std::vector<int> &clients, std::map<int, std::string> &names, std::vector<std::string> &categories, 
    std::map<int, json> &answers, std::map<int, std::string> &writeBuffers, int epollFD
){
    int points = 0;
    json message = R"({
            "type": "summary"
        })"_json;
    message["name"] = names[clientSocket];
    if(clientSocket == firstPlayer){
        for(const std::string &category: categories){
            std::string answer = answers[clientSocket][category];
            message[category] = answer;
            if(answer == "") continue;

            points += 4;
            if(checkifUnique(clientSocket, clients, answer, category, answers)) points +=3;
        }
    }else{
        for(const std::string &category: categories){
            std::string answer = answers[clientSocket][category];
            message[category] = answer;
            if(answer == "") continue;

            points += 2;
            if(checkifUnique(clientSocket, clients, answer, category, answers)) points +=3;
        }
    }
    message["points"] = points;
    int returnValue = setupAllWriteBuffers(message, clients, writeBuffers, epollFD);
    if(returnValue < 0) return -1;

    return points;
}

bool checkifUnique(int clientSocket, std::vector<int> &clients, std::string answer, std::string category, std::map<int, json> &answers){
    for(const int &client: clients){
        if(client == clientSocket) continue;
        if(answers[client][category] == answer) return false;
    }
    return true;
}

int startRound(bool &ifGameRunning, std::vector<int> &clients, std::map<int, std::string> &names, std::map<int, int> &points, 
    std::map<int, int> &rounds, std::map<int, bool> &ifPlaying, std::map<int, std::string> &writeBuffers, int epollFD,
    char &currentLetter, time_t &roundStartTime, int &roundEndRequirement, int &submitedAnswers, int playerCount, int &firstPlayer
){
    ifGameRunning = true;
    firstPlayer = 0;
    setAllPlayers(clients, ifPlaying);

    json message = R"({
        "type": "start"
    })"_json;
    int returnValue = setupAllWriteBuffers(message, clients, writeBuffers, epollFD);
    if(returnValue < 0) return -1;

    currentLetter = generateRandomLetter();
    roundStartTime = time(NULL);
    roundEndRequirement = playerCount/2;
    submitedAnswers = 0;

    message = getGameData(playerCount, clients, names, points, rounds, currentLetter, roundStartTime, submitedAnswers, roundEndRequirement);
    returnValue = setupAllWriteBuffers(message, clients, writeBuffers, epollFD);
    if(returnValue < 0) return 1;
}

void eraseData(int clientSocket, std::map<int, std::string> &names, std::map<int, int> &points, std::map<int, int> &rounds,
    std::map<int, bool> &ifPlaying, std::map<int, json> &answers
){
    auto it1 = names.find(clientSocket);
    if (it1 != names.end()) {
        names.erase(it1);
    }
    auto it2 = points.find(clientSocket);
    if (it2 != points.end()) {
        points.erase(it2);
    } 
    auto it3 = rounds.find(clientSocket);
    if (it3!= rounds.end()) {
        rounds.erase(it3);
    }
    auto it4 = ifPlaying.find(clientSocket);
    if (it4 != ifPlaying.end()) {
        ifPlaying.erase(it4);
    }
    auto it5 = answers.find(clientSocket);
    if (it5 != answers.end()) {
        answers.erase(it5);
    }
}

void setAllPlayers(std::vector<int> &clients, std::map<int, bool> &ifPlaying){
    for(const int &client : clients){
        ifPlaying[client] = true;
    }
}

char generateRandomLetter(){
    return ('a'+ (rand()%26));
}

json getGameData(int playerCount, std::vector<int> &clients, std::map<int, std::string> &names, std::map<int, int> &points, 
    std::map<int, int> &rounds, char currentLetter, time_t roundStartTime, int submitedAnswers, int roundEndRequirement
){
    json returnValue = R"({
        "type":"gameData"
    })"_json;
    returnValue["players"] = playerCount;
    returnValue["names"] = changeToVector(clients, names);
    returnValue["points"] = changeToVector(clients, points);
    returnValue["rounds"] = changeToVector(clients, rounds);
    returnValue["letter"] = currentLetter;
    returnValue["time"] = roundStartTime;
    returnValue["answers"] = submitedAnswers;
    returnValue["requirement"] = roundEndRequirement;
    return returnValue;
}

std::vector<int> changeToVector(std::vector<int> &clients, std::map<int, int> &map){
    std::vector<int> returnVector;
    for(const int &client : clients){
        returnVector.push_back(map[client]);
    }
    return returnVector;
}

std::vector<std::string> changeToVector(std::vector<int> &clients, std::map<int, std::string> &map){
    std::vector<std::string> returnVector;
    for(const int &client : clients){
        returnVector.push_back(map[client]);
    }
    return returnVector;
}

int setupAllWriteBuffers(json &dataJSON, std::vector<int> &clients, std::map<int, std::string> &writeBuffers, int epollFD){
    for(const int &client : clients){
        int returnValue = setupWriteBuffer(dataJSON, client, writeBuffers, epollFD);
        if(returnValue < 0) return -1;
    }
    return 0;
}

int setupWriteBuffer(json &dataJSON, int clientSocket, std::map<int, std::string> &writeBuffers, int epollFD){
    if(writeBuffers[clientSocket] == ""){
        epoll_event ee;
        ee.events = EPOLLIN | EPOLLOUT;
        ee.data.fd = clientSocket;

        int returnValue = epoll_ctl(epollFD, EPOLL_CTL_MOD, clientSocket, &ee);
        if(returnValue < 0){
            perror("epoll_ctl"); 
            return -1;
        }
    }

    std::string message = dataJSON.dump();
    writeBuffers[clientSocket] += message;
    writeBuffers[clientSocket] += "$";
    return 0;
}

bool checkIfNameRepeated(std::string name, std::map<int, std::string> &names){
    for (auto const& [key, val] : names){
        if(name == val){
            return true;
        }
    }
    return false;
}

bool checkIfMessageReady(int clientSocket, json &dataJSON, std::map<int, std::string> &readBuffers){
    int pos = readBuffers[clientSocket].find("$");

    if(pos > -1){
        std::string data = readBuffers[clientSocket].substr(0, pos);
        readBuffers[clientSocket] = readBuffers[clientSocket].substr(pos+1);

        // Debug info
        // printf("New data from client on descriptor:%d:%s\n", clientSocket, data.data());
        // printf("The rest on readBuffers[%d]:%s\n", clientSocket, readBuffers[clientSocket].data());

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
        return -1;
    }

    // Debug info
    // printf("Message sent to client on descriptor:%d:%s\n", clientSocket, message.substr(0, returnValue).data());

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
    // printf("Client on descriptor:%d has disconnected\n", clientSocket);
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
    // printf("Client on descriptor:%d has connected\n", clientSocket);
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

int createSerwer(int port=8080, int listening=10){
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

    returnValue = listen(serverSocket, listening);
    if(returnValue < 0){
        perror("listen"); 
        return -1;
    }

    return serverSocket;
}
