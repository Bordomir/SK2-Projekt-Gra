#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#pragma once

#include <QMainWindow>

#include <QTcpSocket>
#include <QTimer>
#include <string>
#include <ctime>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    /* TODO: Dodać zmienną reprezentująca gniazdo - wskaźnik na  QTcpSocket */
    QTcpSocket* socket;
    QTimer *connTimeoutTimer = new QTimer(this);
    QTimer *timer = new QTimer(this);

    // Variable used for receiving data from serwer
    std::string readBuffer = "";

    // Game variables
    std::vector<std::string> categories = {"panstwo","miasto","rzecz","roslina","zwierze","imie"};
    int playerCount;
    int playerAnswers;
    bool ifPlaying;
    int minutes;
    int seconds;
    std::string letter;
    time_t roundStartTime;

    void connTimedOut();
    void socketConnected();
    void socketDisconnected();
    void socketError();
    void socketDataRec();

    void connectBtnHit();
    void joinBtnHit();
    void sendBtnHit();

    bool checkIfMessageReady(json &dataJSON);
    void timeChange();
private:
    Ui::MainWindow *ui;
};
#endif // MAINWINDOW_H
