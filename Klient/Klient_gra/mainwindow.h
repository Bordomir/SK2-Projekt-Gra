#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#pragma once

#include <QMainWindow>

#include <QTcpSocket>
#include <QTimer>
#include <string>

#include "nlohmann/json_fwd.hpp"

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
    std::string readBuffer = "";

    void connTimedOut();
    void socketConnected();
    void socketDisconnected();
    void socketError();
    void socketDataRec();

    void connectBtnHit();
    void joinBtnHit();
    void sendBtnHit();

    bool checkIfMessageReady(json &dataJSON);
private:
    Ui::MainWindow *ui;
};
#endif // MAINWINDOW_H
