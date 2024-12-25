#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include <QTcpSocket>
#include <QTimer>

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

    void connTimedOut();
    void socketConnected();
    void socketDisconnected();
    void socketError();
    void socketDataRec();

    void connectBtnHit();
    void joinBtnHit();
    void sendBtnHit();

private:
    Ui::MainWindow *ui;
};
#endif // MAINWINDOW_H
