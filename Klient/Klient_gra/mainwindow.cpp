#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "config.h"

#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
}

MainWindow::~MainWindow()
{
    socket->close();

    delete ui;
}

void MainWindow::connTimedOut(){
    socket->abort();

    ui->connectBox->setEnabled(true);
    connTimeoutTimer->disconnect();
    ui->serwerMessages->append("<b>Connect timed out</b>");
    QMessageBox::critical(this, "Error", "Connect timed out");
}

void MainWindow::socketConnected(){

}

void MainWindow::socketDisconnected(){

}

void MainWindow::socketError(){

}

void MainWindow::socketDataRec(){

}

void MainWindow::connectBtnHit(){
    ui->connectBox->setEnabled(false);
    ui->serwerMessages->append("<b>Connecting to " + ui->serwerIP->text() + ":" + QString::number(ui->portSpinBox->value()) + "</b>");

    /* TODO:
     *  - stworzyÄ‡ gniazdo
     *  - poĹ‚Ä…czyÄ‡ zdarzenia z funkcjami je obsĹ‚ugujÄ…cymi:
     *     â€˘ zdarzenie poĹ‚Ä…czenia     (do funkcji socketConnected)
     *     â€˘ zdarzenie odbioru danych (stworzyÄ‡ wĹ‚asnÄ… funkcjÄ™)
     *     â€˘ zdarzenie rozĹ‚Ä…czenia    (stworzyÄ‡ wĹ‚asnÄ… funkcjÄ™)
     *     â€˘ zdarzenie bĹ‚Ä™du          (stworzyÄ‡ wĹ‚asnÄ… funkcjÄ™)
     *  - zaĹĽÄ…daÄ‡ (asynchronicznego) nawiÄ…zania poĹ‚Ä…czenia
     */
    TcpSocket = new QTcpSocket(this);

    connect(TcpSocket, &QTcpSocket::connected, this, &MyWidget::socketConnected);
    connect(TcpSocket, &QTcpSocket::readyRead, this, &MyWidget::socketDataRec);
    connect(TcpSocket, &QTcpSocket::disconnected, this, &MyWidget::socketDisconnected);
    connect(TcpSocket, &QTcpSocket::errorOccurred, this, &MyWidget::socketError);

    socket->connectToHost(ui->hostLineEdit->text(),ui->portSpinBox->value());

    connect(connTimeoutTimer, &QTimer::timeout, this, &MyWidget::connTimedOut);
    connTimeoutTimer->start(3000);

}

void MainWindow::joinBtnHit(){

}

void MainWindow::sendBtnHit(){

}
