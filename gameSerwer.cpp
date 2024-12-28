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

class Serwer{
private:
    // Variables used for sockets
    int serverSocket, epollFD;

    // Variables used for receiving and sending data to and from clients
    std::vector<int> clients;
    int clientsCount;
    std::map<int, std::string> readBuffers;
    std::map<int, std::string> writeBuffers;

    // Game variables
    std::vector<std::string> categories;
    int playerCount;
    std::map<int, bool> ifPlaying;
    std::map<int, std::string> names;
    std::map<int, int> points;
    std::map<int, int> rounds;
    std::map<int, json> answers;
    int firstPlayer;
    bool ifGameRunning;
    bool ifRoundEnded;
    std::string currentLetter;
    time_t roundStartTime;
    int roundEndRequirement;
    int submitedAnswers;
    
    // Methods
    int createSerwer(int port, int listening){
        int serverSocketTemp = socket(AF_INET, SOCK_STREAM, 0);
        if(serverSocketTemp < 0){
            perror("socket"); 
            return -1;
        }

        int on = 1;
        int returnValue = setsockopt(serverSocketTemp, SOL_SOCKET, SO_REUSEADDR, &on, sizeof on);
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
        returnValue = bind(serverSocketTemp, (sockaddr*)&serverAddres, serverAddresSize);
        if(returnValue < 0){
            perror("bind"); 
            return -1;
        }

        returnValue = listen(serverSocketTemp, listening);
        if(returnValue < 0){
            perror("listen"); 
            return -1;
        }

        return serverSocketTemp;
    }
    int createEpoll(){
        int epollFDTemp = epoll_create1(0);
        if(epollFDTemp < 0){
            perror("epoll_create1"); 
            return -1;
        }

        epoll_event ee;
        ee.events = EPOLLIN;
        ee.data.fd = serverSocket;
        int returnValue = epoll_ctl(epollFDTemp, EPOLL_CTL_ADD, serverSocket, &ee);
        if(returnValue < 0){
            perror("epoll_ctl"); 
            return -1;
        }

        return epollFDTemp;
    }
    int connectNewClient(){
        sockaddr_in clientAddres;
        socklen_t clientAddresSize = sizeof clientAddres;

        int clientSocket = accept(serverSocket, (sockaddr*)&clientAddres, &clientAddresSize);
        if(clientSocket < 0){
            perror("accept"); 
            return -1;
        }

        clients.push_back(clientSocket);
        clientsCount++;
        readBuffers[clientSocket] = "";
        writeBuffers[clientSocket] = "";

        // Debug info
        // printf("Clients after push_back:[");
        // for(const int &i : clients){
        //     printf("%d, ", i);
        // }
        // printf("]\n");

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
        return clientSocket;
    }
    int disconnectClient(int clientSocket){
        clients.erase(std::remove(clients.begin(), clients.end(), clientSocket), clients.end());

        clientsCount--;
        auto it1 = readBuffers.find(clientSocket);
        if (it1 != readBuffers.end()) {
            readBuffers.erase(it1);
        }
        auto it2 = writeBuffers.find(clientSocket);
        if (it2 != writeBuffers.end()) {
            writeBuffers.erase(it2);
        } 

        // Debug info
        // printf("Clients after erase:[");
        // for(const int &i : clients){
        //     printf("%d, ", i);
        // }
        // printf("]\n");

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
    int writeToSocket(int clientSocket){
        // Check if buffer is empty (no messages to be sent)
        int returnValue = checkWriteBuffer(clientSocket);
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
        returnValue = checkWriteBuffer(clientSocket);
        if(returnValue < 0) return -1;

        return 0;
    }
    int checkWriteBuffer(int clientSocket){
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
    bool checkIfMessageReady(int clientSocket, json &JSON){
        int pos = readBuffers[clientSocket].find("$");

        if(pos > -1){
            std::string data = readBuffers[clientSocket].substr(0, pos);
            readBuffers[clientSocket] = readBuffers[clientSocket].substr(pos+1);

            // Debug info
            // printf("New data from client on descriptor:%d:%s\n", clientSocket, data.data());
            // printf("The rest on readBuffers[%d]:%s\n", clientSocket, readBuffers[clientSocket].data());

            JSON = json::parse(data);
            return true;        
        }
        return false;
    }
    bool checkIfNameRepeated(std::string name){
        for (auto const& x: names){
            if(name == x.second){
                return true;
            }
        }
        return false;
    }
    int setupWriteBuffer(json &JSON, int clientSocket){
        // Debug info
        // printf("send to %d: %s\n", clientSocket, JSON.dump().data());
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

        std::string message = JSON.dump();
        writeBuffers[clientSocket] += message;
        writeBuffers[clientSocket] += "$";
        return 0;
    }
    int setupAllWriteBuffers(json &JSON){
        // Debug info
        // printf("send to All: %s\n", JSON.dump().data());
        for(const int &client : clients){
            if(names[client] == "") continue;
            std::string typ = JSON["type"];
            if(typ == "sleep" && !ifPlaying[client]) continue;
            int returnValue = setupWriteBuffer(JSON, client);
            if(returnValue < 0) return -1;
        }
        return 0;
    }
    std::vector<std::string> changeToVector(std::map<int, std::string> &map){
        std::vector<std::string> returnVector;
        for(const int &client : clients){
            returnVector.push_back(map[client]);
        }
        return returnVector;
    }
    std::vector<int> changeToVector(std::map<int, int> &map){
        std::vector<int> returnVector;
        for(const int &client : clients){
            returnVector.push_back(map[client]);
        }
        return returnVector;
    }
    json getGameData(){
        json returnValue = R"({
            "type":"gameData"
        })"_json;
        returnValue["players"] = playerCount;
        returnValue["names"] = changeToVector(names);
        returnValue["points"] = changeToVector(points);
        returnValue["rounds"] = changeToVector(rounds);
        returnValue["letter"] = currentLetter;
        returnValue["time"] = roundStartTime;
        returnValue["answers"] = submitedAnswers;
        returnValue["requirement"] = roundEndRequirement;
        return returnValue;
    }
    std::string generateRandomLetter(){
        char letter = ('a'+ (rand()%26));
        std::string returnValue{letter};
        return returnValue;
    }
    void setAllPlayers(){
        for(const int &client : clients){
            ifPlaying[client] = true;
        }
    }
    void eraseData(int clientSocket){
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
    int startRound(){
        ifRoundEnded = false;
        ifGameRunning = true;
        firstPlayer = 0;
        setAllPlayers();
        setEmptyAnswers();

        json message = R"({
            "type": "start"
        })"_json;
        int returnValue = setupAllWriteBuffers(message);
        if(returnValue < 0) return -1;

        currentLetter = generateRandomLetter();
        roundStartTime = time(NULL);
        roundEndRequirement = playerCount/2;
        submitedAnswers = 0;

        message = getGameData();
        returnValue = setupAllWriteBuffers(message);
        if(returnValue < 0) return 1;
        return 0;
    }
    bool checkifUnique(int clientSocket, std::string answer, std::string category){
        for(const int &client: clients){
            if(client == clientSocket) continue;
            if(answers[client][category] == answer) return false;
        }
        return true;
    }
    int calculatePoints(int clientSocket){
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
                if(checkifUnique(clientSocket, answer, category)) points +=3;
            }
        }else{
            for(const std::string &category: categories){
                std::string answer = answers[clientSocket][category];
                message[category] = answer;
                if(answer == "") continue;

                points += 2;
                if(checkifUnique(clientSocket, answer, category)) points +=3;
            }
        }
        message["points"] = points;
        int returnValue = setupAllWriteBuffers(message);
        if(returnValue < 0) return -1;

        return points;
    }
    int endRound(){
        ifRoundEnded = true;
        json message = R"({
                "type": "end"
            })"_json;
        int returnValue = setupAllWriteBuffers(message);
        if(returnValue < 0) return -1;

        for(const int& client: clients){
            if(!ifPlaying[client]) continue;
            rounds[client]++;

            if(answers[client]["time"] == 0) continue;
            points[client] += calculatePoints(client);
        }

        message = R"({
                "type": "sleep"
            })"_json;
        returnValue = setupAllWriteBuffers(message);
        if(returnValue < 0) return -1;
        return 0;
    }
    void setEmptyAnswers(){
        for(const int &client: clients){
            json empty = R"({
                "type":"answers",
                "time":0
            })"_json;
            answers[client] = empty;
        }
    }
    void clearGameData(){
        clientsCount = 0;
        playerCount = 0;
        firstPlayer = 0;
        ifGameRunning = false;
        ifRoundEnded = false;
        roundEndRequirement = INT16_MAX;
        submitedAnswers = 0;
    }
    int newClient(){
        // Accept new client, add it to epoll and setup variables for client data
        int clientSocket = connectNewClient();
        if(clientSocket < 0) return -1;

        // Send config data to client
        json message = R"({
                "type": "config"
            })"_json;
        message["min"] = MIN_PLAYER_NUMBER;
        message["time"] = ROUND_TIME;
        message["delay"] = ROUND_START_DELAY;
        int returnValue = setupWriteBuffer(message, clientSocket);
        if(returnValue < 0) return -1;
        return 0;
    }
    int checkRoundEnd(){
        if(ifGameRunning && submitedAnswers >= roundEndRequirement && !ifRoundEnded){
            int returnValue = endRound();
            if(returnValue < 0) return -1;
        }
        return 0;
    } 
    int disconnectPlayer(int clientSocket){
        // Remove client from epoll, close its descriptor and update variables for client data
        int returnValue = disconnectClient(clientSocket);
        if(returnValue < 0) return -1;

        // Remove player from game and sends message to other players

        // Disconnecting counts as submitted answer if was a player and a game is running
        if(ifGameRunning && ifPlaying[clientSocket]) submitedAnswers++;

        // Check if client was actually a player, send message to other players and delete his data
        if(names[clientSocket] != ""){
            playerCount--;
            json message = R"({
                    "type": "disconnected"
                })"_json;
            message["name"] = names[clientSocket];
            message["answers"] = submitedAnswers;
            message["players"] = playerCount;
            int returnValue = setupAllWriteBuffers(message);
            if(returnValue < 0) return -1;
            eraseData(clientSocket);
        } 

        // Check if round should end
        returnValue = checkRoundEnd();
        if(returnValue < 0) return -1;

        // Check if all players has disconnected
        if(playerCount == 0){
            clearGameData();
        }

        return 0; 
    }
    bool checkIfIgnore(json &dataJSON){
        // Ignore messages that are received while game is not running 
        if(!ifGameRunning) return true;
        // round has already ended
        if(ifRoundEnded) return true;
        // or are from previous round
        if(dataJSON["time"] != roundStartTime) return true;
        return false;
    }
    int manageMessage(int clientSocket, json &dataJSON){
        // Debug info
        // printf("Json object in data:%s\n", dataJSON.dump().data());

        if(!dataJSON.contains("type")){
            fprintf(stderr, "Invalid data was received");
            return 0;
        }

        json message;
        int returnValue;
        if(dataJSON["type"] == "name"){
            // Player sent his name
            if(!dataJSON.contains("name")){
                fprintf(stderr, "Invalid data was received");
                return 0;
            }

            if(checkIfNameRepeated(dataJSON["name"])){
                message = R"({
                        "type": "nameRepeated"
                    })"_json;
                returnValue = setupWriteBuffer(message, clientSocket);
                if(returnValue < 0) return -1;
                return 0;
            }

            message = R"({
                    "type": "nameCorrect"
                })"_json;
            returnValue = setupWriteBuffer(message, clientSocket);
            if(returnValue < 0) return -1;

            // Send player game data if game is running
            if(ifGameRunning){
                message = getGameData();
                returnValue = setupWriteBuffer(message, clientSocket);
                if(returnValue < 0) return -1;
            }

            // Set up player data
            ifPlaying[clientSocket] = false;
            names[clientSocket] = dataJSON["name"];
            points[clientSocket] = 0;
            rounds[clientSocket] = 0;
            json empty = R"({
                "type":"answers",
                "time":0
            })"_json;
            answers[clientSocket] = empty;
            playerCount++;
            
            // Send message to players about new player
            message = R"({
                    "type": "newPlayer"
                })"_json;
            message["name"] = dataJSON["name"];
            message["players"] = playerCount;
            returnValue = setupAllWriteBuffers(message);
            if(returnValue < 0) return -1;

            // Check if round can be started
            if(!ifGameRunning && playerCount >= MIN_PLAYER_NUMBER){
                returnValue = startRound();
                if(returnValue < 0) return -1;
            }

        }else if(dataJSON["type"] == "answers"){
            // Player sent his answers
            if(!dataJSON.contains("panstwo")
                || !dataJSON.contains("miasto")
                || !dataJSON.contains("rzecz")
                || !dataJSON.contains("roslina")
                || !dataJSON.contains("zwierze")
                || !dataJSON.contains("imie")
                || !dataJSON.contains("time")){
                fprintf(stderr, "Invalid data was received");
                return 0;
            }

            // Check if message should be ignored
            if(checkIfIgnore(dataJSON)) return 0;

            // Mark first player
            if(firstPlayer == 0){
                firstPlayer = clientSocket;
            }

            // Save player answers 
            answers[clientSocket] = dataJSON;
            submitedAnswers++;

            // Send message about submitted answer
            message = R"({
                    "type": "answer"
                })"_json;
            message["name"] = names[clientSocket];
            returnValue = setupAllWriteBuffers(message);
            if(returnValue < 0) return -1;

            // Check if round should end
            returnValue = checkRoundEnd();
            if(returnValue < 0) return -1;

        }else if(dataJSON["type"] == "time"){
            // Time to answer has ended
            if(!dataJSON.contains("time")){
                fprintf(stderr, "Invalid data was received");
                return 0;
            }
    
            // Check if message should be ignored
            if(checkIfIgnore(dataJSON)) return 0;
            
            // Check if round should end
            returnValue = checkRoundEnd();
            if(returnValue < 0) return -1;
            
        }else if(dataJSON["type"] == "round"){
            // Time for summary of prevoius round has ended
            if(!dataJSON.contains("time")){
                fprintf(stderr, "Invalid data was received");
                return 0;
            }
            
            // Check if message should be ignored
            // Ignore messages that are received while game is not running 
            if(!ifGameRunning) return true;
            // round has not ended
            if(!ifRoundEnded) return true;
            // or are from previous round
            if(dataJSON["time"] != roundStartTime) return true;

            // Check if round can be started
            if(playerCount >= MIN_PLAYER_NUMBER){
                returnValue = startRound();
                if(returnValue < 0) return -1;
            }else{
                ifGameRunning = false;
            }
        }
        return 0;
    }
    int clientMessageAvailable(int clientSocket){
        // Read data from socket and truncate it if needed ('\n' at the end)
        std::string data = readFromSocket(clientSocket);
        if(data == "-1") return -1;

        int returnValue;
        // Check if connection was lost or player disconnencted
        if(data == ""){  
            returnValue = disconnectPlayer(clientSocket);
            if(returnValue < 0) return -1;
        }

        // Debug info
        // printf("New data from client on descriptor:%d:%s\n", clientSocket, data.data());

        readBuffers[clientSocket] += data;

        json dataJSON;
        // Checks if entire message has been received and writes it to dataJSON variable
        while(checkIfMessageReady(clientSocket, dataJSON)){
            returnValue = manageMessage(clientSocket, dataJSON);
            if(returnValue < 0) return -1;
        }

        return 0;
    }

    // Main loop of a tcp serwer
    int mainLoop(){
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
                int returnValue = newClient();
                if(returnValue < 0) return 1;
            }else if(ee.events & EPOLLIN){
                // New data from client
                int returnValue = clientMessageAvailable(ee.data.fd);
                if(returnValue < 0) return 1;
            }else if(ee.events & EPOLLOUT){
                // Message can be sent

                // Send data from writeBuffer and changes epoll setting if all data was sent
                returnValue = writeToSocket(ee.data.fd);
                if(returnValue < 0) return 1;
            }
        }
    }

public:
    Serwer(){
        categories = {"panstwo","miasto","rzecz","roslina","zwierze","imie"};
        clearGameData();
    }

    int run(){
        // Create listenning TCP serwer socket with SO_REUSEADDR option an binded to INADDR_ANY adress on PORT 
        serverSocket = createSerwer(PORT, LISTENING);
        if(serverSocket < 0) return 1;

        // Create epoll with added listening serwer socket
        epollFD = createEpoll();
        if(epollFD < 0) return 1;

        // Initialize random seed
        srand(time(NULL));

        return mainLoop();
    }
};

int main(){
    Serwer serwer = Serwer();
    return serwer.run();
}