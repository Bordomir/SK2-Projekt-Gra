#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "config.h"

#include <QMessageBox>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    connTimeoutTimer->setSingleShot(true);
    timer->setInterval(1000);
    timer2->setSingleShot(true);

    playerCount = 0;
    playerAnswers = 0;
    ifPlaying = false;
    minutes = 0;
    seconds = 0;
    minPlayers = 99;
    roundTime = 99;
    letter = "";
    roundStartTime = 0;
    roundStartDelay = 10000;

    connect(timer, &QTimer::timeout, this, &MainWindow::timeChange);
    connect(timer2, &QTimer::timeout, this, &MainWindow::summaryEnd);
    connect(ui->connectButton, &QPushButton::clicked, this, &MainWindow::connectBtnHit);
    connect(ui->disconnectButton, &QPushButton::clicked, this, &MainWindow::dissconnectfromSerwer);
    connect(ui->joinButton, &QPushButton::clicked, this, &MainWindow::joinBtnHit);
    connect(ui->sendButton, &QPushButton::clicked, this, &MainWindow::sendBtnHit);
}

MainWindow::~MainWindow()
{
    socket->close();

    delete ui;
}

void MainWindow::connTimedOut(){
    socket->abort();

    connTimeoutTimer->disconnect();
    ui->connectBox->setEnabled(true);
    ui->disconnectButton->setEnabled(false);
    ui->joinBox->setEnabled(false);
    ui->gameBox->setEnabled(false);
    ui->serwerMessages->append("<b>Połączenie trwało za długo</b>");
    QMessageBox::critical(this, "Error", "Połączenie trwało za długo");
}

void MainWindow::socketConnected(){
    connTimeoutTimer->stop();
    connTimeoutTimer->disconnect();
    ui->serwerMessages->append("<b>Połączono</b>");

    ui->joinBox->setEnabled(true);
    ui->disconnectButton->setEnabled(true);
}

void MainWindow::socketDisconnected(){
    ui->serwerMessages->append("<b>Rozłączono z " + ui->serwerIP->text() + ":" + QString::number(PORT) + "</b>");
    socket->disconnect();

    ui->connectBox->setEnabled(true);
    ui->disconnectButton->setEnabled(false);
    ui->joinBox->setEnabled(false);
    ui->gameBox->setEnabled(false);
    if(timer->isActive()) timer->stop();
    if(timer2->isActive()) timer2->stop();
    playerCount = 0;
    playerAnswers = 0;
    ifPlaying = false;
    minutes = 0;
    seconds = 0;
    minPlayers = 99;
    roundTime = 99;
    letter = "";
    roundStartTime = 0;
    roundStartDelay = 10000;
    ui->allPlayersLine->setText(QString(""));
    ui->requiredPlayersLine->setText(QString(""));
    ui->currentPlayersLine->setText(QString(""));
    ui->letterLine->setText(QString(""));
    ui->timeLine->setText(QString(""));
    for(int i = 0; i < 6; i++){
        QTableWidgetItem item;
        ui->playerAnswersTable->setItem(0,i, &item);
    }
}

void MainWindow::socketError(){
    ui->serwerMessages->append("<b>Wystąpił problem</b>");
    socket->abort();

    ui->connectBox->setEnabled(true);
    ui->disconnectButton->setEnabled(false);
    ui->joinBox->setEnabled(false);
    ui->gameBox->setEnabled(false);
}

void MainWindow::socketDataRec(){
    std::string message = socket->readAll().toStdString();

    readBuffer += message;

    json dataJSON;
    while(MainWindow::checkIfMessageReady(dataJSON)){
        // Data Service
        if(!dataJSON.contains("type")){
            ui->serwerMessages->append("<b>Przychodzące dane są niepoprawne</b>");
            continue;
        }

        if(dataJSON["type"] == "config"){
            // Serwer sends config
            if(!dataJSON.contains("min")
                || !dataJSON.contains("time")
                || !dataJSON.contains("delay")){
                fprintf(stderr, "Invalid data was received");
                continue;
            }
            minPlayers = dataJSON["min"];
            roundTime = dataJSON["time"];
            roundStartDelay = dataJSON["delay"];

        }else if(dataJSON["type"] == "nameRepeated"){
            // Player name was already in use
            ui->serwerMessages->append("<b>Podana nazwa gracza jest już używana</b>");

        }else if(dataJSON["type"] == "nameCorrect"){
            // Player name is correct
            ui->serwerMessages->append("<b>Podana nazwa gracza jest poprawna</b>");
            ui->joinBox->setEnabled(false);

        }else if(dataJSON["type"] == "gameData"){
            // New player has connected
            if(!dataJSON.contains("names")
                || !dataJSON.contains("points")
                || !dataJSON.contains("rounds")
                || !dataJSON.contains("letter")
                || !dataJSON.contains("time")
                || !dataJSON.contains("answers")
                || !dataJSON.contains("requirement")
                || !dataJSON.contains("players")){
                fprintf(stderr, "Invalid data was received");
                continue;
            }
            playerCount = dataJSON["players"];
            int req = dataJSON["requirement"];
            playerAnswers = dataJSON["answers"];
            letter = dataJSON["letter"];
            ui->allPlayersLine->setText(QString::number(playerCount));
            ui->requiredPlayersLine->setText(QString::number(req));
            ui->currentPlayersLine->setText(QString::number(playerAnswers));
            ui->letterLine->setText(QString::fromStdString(letter));

            ui->playersPointsTable->setRowCount(0);
            for(int i = 0; i < playerCount; i++){
                ui->playersPointsTable->insertRow(i);
                std::string name = dataJSON["names"][i];
                int points = dataJSON["points"][i];
                int rounds = dataJSON["rounds"][i];
                double average = 0;
                if(rounds != 0){
                    average = (double)points/(double)rounds;
                }
                ui->playersPointsTable->setItem(i,0, new QTableWidgetItem(QString::fromStdString(name)));
                ui->playersPointsTable->setItem(i,1, new QTableWidgetItem(QString::number(points)));
                ui->playersPointsTable->setItem(i,2, new QTableWidgetItem(QString::number(average)));
            }

            roundStartTime = dataJSON["time"];
            minutes = roundTime;
            seconds = 0;
            if(!ifPlaying){
                time_t curr = time(NULL);
                double sec = difftime(curr, roundStartTime);
                int sec1 = int(sec);
                while(sec1 > 59){
                    minutes--;
                    sec1 -= 60;
                }
                if(sec1 > 0){
                    minutes--;
                    seconds = 60-sec1;
                }
            }
            if(minutes < 0){
                ui->timeLine->setText(QString("0:0"));
            }else{
                ui->timeLine->setText(QString::number(minutes) + ":" + QString::number(seconds));
                timer->start();
            }

        }else if(dataJSON["type"] == "newPlayer"){
            // New player has connected
            if(!dataJSON.contains("name")
                || !dataJSON.contains("players")){
                fprintf(stderr, "Invalid data was received");
                continue;
            }
            playerCount = dataJSON["players"];
            ui->allPlayersLine->setText(QString::number(playerCount));
            std::string name = dataJSON["name"];
            if(playerCount < minPlayers){
                ui->serwerMessages->append("<b>Gracz o nazwie " + QString::fromStdString(name)
                                           + " dołączył ale nadal brakuje " + QString::number(minPlayers-playerCount) + " graczy do rozpoczęcia następnej rundy</b>");
            }else if(playerCount == minPlayers){
                ui->serwerMessages->append("<b>Gracz o nazwie " + QString::fromStdString(name) + " dołączył i następna runda się rozpocznie</b>");
            }else{
                ui->serwerMessages->append("<b>Gracz o nazwie " + QString::fromStdString(name) + " dołączy do gry w następnej rundzie</b>");
            }

        }else if(dataJSON["type"] == "start"){
            // Round starts
            if(timer2->isActive()) timer2->stop();

            ui->gameBox->setEnabled(true);
            for(int i = 0; i < 6; i++){
                QTableWidgetItem item;
                ui->playerAnswersTable->setItem(0,i, &item);
            }

            ui->serwerMessages->append("<b>Runda się rozpoczyna</b>");

        }else if(dataJSON["type"] == "answer"){
            // Someone sent answers
            if(!dataJSON.contains("name")){
                fprintf(stderr, "Invalid data was received");
                continue;
            }
            playerAnswers++;
            ui->currentPlayersLine->setText(QString::number(playerAnswers));
            std::string name = dataJSON["name"];
            ui->serwerMessages->append("<b>Gracz o nazwie " + QString::fromStdString(name) + " wysłał swoją odpowiedź</b>");

        }else if(dataJSON["type"] == "summary"){
            // Summary of a round
            if(!dataJSON.contains("name")
                || !dataJSON.contains("panstwo")
                || !dataJSON.contains("miasto")
                || !dataJSON.contains("rzecz")
                || !dataJSON.contains("roslina")
                || !dataJSON.contains("zwierze")
                || !dataJSON.contains("imie")
                || !dataJSON.contains("points")){
                fprintf(stderr, "Invalid data was received");
                continue;
            }
            std::string name = dataJSON["name"];
            std::string panstwo = dataJSON["panstwo"];
            std::string miasto = dataJSON["miasto"];
            std::string rzecz = dataJSON["rzecz"];
            std::string roslina = dataJSON["roslina"];
            std::string zwierze = dataJSON["zwierze"];
            std::string imie = dataJSON["imie"];
            int points = dataJSON["points"];

            ui->serwerMessages->append("<b>Gracz o nazwie " + QString::fromStdString(name) + " zdobył " + QString::number(points) + " punktów za odpowiedzi:</b>");
            ui->serwerMessages->append("<b>państwo:" + QString::fromStdString(panstwo) + ",</b>");
            ui->serwerMessages->append("<b>miasto:" + QString::fromStdString(miasto) + ",</b>");
            ui->serwerMessages->append("<b>rzecz:" + QString::fromStdString(rzecz) + ",</b>");
            ui->serwerMessages->append("<b>roślina" + QString::fromStdString(roslina) + ",</b>");
            ui->serwerMessages->append("<b>zwierzę:" + QString::fromStdString(zwierze) + ",</b>");
            ui->serwerMessages->append("<b>imie:" + QString::fromStdString(imie) + "</b>");

        }else if(dataJSON["type"] == "end"){
            // Round end
            ui->gameBox->setEnabled(false);
            ui->serwerMessages->append("<b>Runda się zakończyła</b>");
            if(timer->isActive()) timer->stop();
            ui->requiredPlayersLine->setText(QString(""));
            ui->currentPlayersLine->setText(QString(""));
            ui->letterLine->setText(QString(""));
            ui->timeLine->setText(QString(""));

        }else if(dataJSON["type"] == "sleep"){
            // Wait for another round
            timer2->start(roundStartDelay);
            ui->serwerMessages->append("<b>Jeżeli to możliwe następna runda rozpocznie się za 10 sekund</b>");

        }else if(dataJSON["type"] == "disconnected"){
            // Player has disconnected
            if(!dataJSON.contains("name")
                || !dataJSON.contains("answers")
                || !dataJSON.contains("players")){
                fprintf(stderr, "Invalid data was received");
                continue;
            }
            std::string name = dataJSON["name"];
            if(name == "") continue;

            playerCount = dataJSON["players"];
            ui->allPlayersLine->setText(QString::number(playerCount));
            playerAnswers = dataJSON["answers"];
            ui->currentPlayersLine->setText(QString::number(playerAnswers));
            if(playerCount < minPlayers){
                ui->serwerMessages->append("<b>Gracz o nazwie " + QString::fromStdString(name) + " się rozłączył i brakuje "
                                           + QString::number(minPlayers-playerCount) + " graczy do rozpoczęcia następnej rundy</b>");
            }else{
                ui->serwerMessages->append("<b>Gracz o nazwie " + QString::fromStdString(name) + " się rozłączył</b>");
            }
        }
        // Debug info
        // ui->serwerMessages->append("<b>" +QString::fromStdString(dataJSON.dump().data()) + "</b>");
    }
}

void MainWindow::connectBtnHit(){
    ui->connectBox->setEnabled(false);
    ui->serwerMessages->append("<b>Łączenie z " + ui->serwerIP->text() + ":" + QString::number(PORT) + "</b>");

    socket = new QTcpSocket(this);

    connect(socket, &QTcpSocket::connected, this, &MainWindow::socketConnected);
    connect(socket, &QTcpSocket::readyRead, this, &MainWindow::socketDataRec);
    connect(socket, &QTcpSocket::disconnected, this, &MainWindow::socketDisconnected);
    connect(socket, &QTcpSocket::errorOccurred, this, &MainWindow::socketError);

    socket->connectToHost(ui->serwerIP->text(), PORT);

    connect(connTimeoutTimer, &QTimer::timeout, this, &MainWindow::connTimedOut);
    connTimeoutTimer->start(3000);
}

void MainWindow::joinBtnHit(){
    std::string name = ui->playerName->text().trimmed().toStdString();
    int pos = name.find("$");
    if(pos > -1){
        ui->serwerMessages->append("<b>Podana nazwa gracza ma niedopuszczalny znak '$'</b>");
        return;
    }
    if(name == ""){
        ui->serwerMessages->append("<b>Podana nazwa gracza nie może być pusta</b>");
        return;
    }

    nlohmann::json dataJSON;
    dataJSON["type"] = "name";
    dataJSON["name"] = name;

    std::string message = "";
    message += dataJSON.dump().data();
    message += "$";
    socket->write(message.c_str());

    ui->serwerMessages->append("<b>Wysłano nazwę gracza do serwera</b>");
}

void MainWindow::sendBtnHit(){
    json dataJSON;
    dataJSON["type"] = "answers";
    for(int i = 0; i < 6; i++){
        QTableWidgetItem *item = ui->playerAnswersTable->item(0,i);
        if(item == nullptr){
            dataJSON[categories[i]] = "";
            continue;
        }
        QString Qanswer = item->data(Qt::DisplayRole).toString();
        if(Qanswer.isEmpty()){
            dataJSON[categories[i]] = "";
            continue;
        }
        std::string answer = Qanswer.toLower().toStdString();
        int pos = answer.find("$");
        if(pos > -1){
            answer = "";
        }
        if(answer[0] != letter[0]){
            answer = "";
        }
        if(answer.empty()){
            answer = "";
        }
        dataJSON[categories[i]] = answer;
    }
    dataJSON["time"] = roundStartTime;

    std::string message = "";
    message += dataJSON.dump().data();
    message += "$";
    socket->write(message.c_str());

    ui->gameBox->setEnabled(false);

    ui->serwerMessages->append("<b>Wysłano odpowiedzi gracza do serwera</b>");

}

bool MainWindow::checkIfMessageReady(json &dataJSON){
    int pos = readBuffer.find("$");
    bool ifReady = (pos > -1);

    if(ifReady){
        std::string data = readBuffer.substr(0, pos);
        readBuffer = readBuffer.substr(pos+1);

        dataJSON = json::parse(data);
        return true;
    }
    return false;
}

void MainWindow::timeChange(){
    if(seconds == 0){
        minutes--;
        seconds = 59;
    }else{
        seconds--;
    }

    ui->timeLine->setText(QString::number(minutes) + ":" + QString::number(seconds));

    if(seconds == 0 && minutes == 0){
        json dataJSON;
        dataJSON["type"] = "time";
        dataJSON["time"] = roundStartTime;
        std::string message = "";
        message += dataJSON.dump().data();
        message += "$";
        socket->write(message.c_str());

        ui->gameBox->setEnabled(false);

        timer->stop();
    }
}

void MainWindow::summaryEnd(){
    timer2->stop();
    json dataJSON;
    dataJSON["type"] = "round";
    dataJSON["time"] = roundStartTime;
    std::string message = "";
    message += dataJSON.dump().data();
    message += "$";
    socket->write(message.c_str());

    // Debug info
    // ui->serwerMessages->append("<b>round message</b>");

}

void MainWindow::dissconnectfromSerwer(){
    socket->disconnectFromHost();
}
