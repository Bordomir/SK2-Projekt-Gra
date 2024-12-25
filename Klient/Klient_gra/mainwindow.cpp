#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "config.h"

#include <QMessageBox>
#include <cstring>
#include <string>
#include <vector>

#include "nlohmann/json.hpp"

using json = nlohmann::json;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    connTimeoutTimer->setSingleShot(true);

    connect(ui->connectButton, &QPushButton::clicked, this, &MainWindow::connectBtnHit);
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
    ui->serwerMessages->append("<b>Połączenie trwało za długo</b>");
    QMessageBox::critical(this, "Error", "Połączenie trwało za długo");
}

void MainWindow::socketConnected(){
    connTimeoutTimer->stop();
    connTimeoutTimer->disconnect();
    ui->serwerMessages->append("<b>Połączono</b>");

    ui->joinBox->setEnabled(true);
}

void MainWindow::socketDisconnected(){
    ui->serwerMessages->append("<b>Rozłączono z " + ui->serwerIP->text() + ":" + QString::number(PORT) + "</b>");
    socket->disconnect();

    ui->connectBox->setEnabled(true);
    ui->joinBox->setEnabled(false);
}

void MainWindow::socketError(){
    ui->serwerMessages->append("<b>Wystąpił problem</b>");
    socket->abort();

    ui->connectBox->setEnabled(true);
    ui->joinBox->setEnabled(false);
}

void MainWindow::socketDataRec(){
    std::string message = socket->readAll().toStdString();

    readBuffer += message;

    json dataJSON;
    while(MainWindow::checkIfMessageReady(dataJSON)){
        // Data Service
        // TODO

        // Debug info
        ui->serwerMessages->append("<b>" +QString::fromStdString(dataJSON.dump().data()) + "</b>");
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

    if(name.find("$") > -1){
        ui->serwerMessages->append("<b>Podana nazwa gracza ma niedopuszczalny znak '$'</b>");
        return;
    }

    nlohmann::json dataJSON;
    dataJSON["type"] = "name";
    dataJSON["name"] = name;

    socket->write(dataJSON.dump().data());

    ui->serwerMessages->append("<b>Wysłano nazwę gracza do serwera</b>");
}

void MainWindow::sendBtnHit(){
    std::vector<std::string> categories = {"panstwo","miasto","rzecz","roslina","zwierze","imie"};
    json dataJSON;
    dataJSON["type"] = "answers";
    for(int i = 0; i < 6; i++){
        std::string answer = ui->playerAnswersTable->item(0,i) ->data(Qt::DisplayRole).toString().toStdString();
        if(answer.find("$") > -1){
            answer = "";
        }
        dataJSON[categories[i]] = answer;
    }


    socket->write(dataJSON.dump().data());

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
