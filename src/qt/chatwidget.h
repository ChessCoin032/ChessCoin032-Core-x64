// chatwidget.h
#ifndef QCHATWIDGET_H
#define QCHATWIDGET_H

#include <QWidget>
#include <QTcpSocket>
#include <QThread>
#include <QTimer>
#include <QMutex>
#include "chatworker.h"

#define CHATSERVER0         "54.38.157.243"
#define CHATSERVER1         "148.113.46.51"
//#define CHATSERVER2       "51.178.41.236"

#define CHATSERVERPORT0     3200
#define CHATSERVERPORT1     3201

#define NAMECHATSERVER0     "Frankfrut"
#define NAMECHATSERVER1     "Mumbai"

#define CHATSERVERCOUNT     2

namespace Ui {
class chatWidget;
}

class QChatWidget : public QWidget
{
    Q_OBJECT

public:
    QChatWidget(QWidget *parent = nullptr, int ver = 1);
    ~QChatWidget();

signals:
    void clearRequested();
    void connectToServer(const QString &serverIp, quint16 port);
    void sendMessage(const QString &message);

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead(const QString &message);
    void onHandleError(const QString &errorString);
    void onBytesWritten(qint64 bytes);
    void onCheckPort();

    void on_sendButton_clicked();
    void on_clearButton_clicked();
    void on_connButton_clicked();

private:
    QString getUTCTimeForChatroom();
    QString getChatServerAddr();
    QString getCurrentServerName();
    QString getNextServerName();
    QString getComputerName();

    void appendChatLog(const QString& msg, int textColor = Qt::white);
    void setConnectState(bool connected);
    void tryConnect();

    Ui::chatWidget *ui;
    QThread *workerThread;
    ChatWorker *worker;
    QTimer *checkPortTimer;
    bool bConnected;
    QString strChatSvrAddr;
    QHash<QString, qint64> messageHistory;
    QMutex messageHistoryMutex;
    QTimer *cleanupTimer;

    QVector<QString> vChatServers;
    int chatServerIndex;

    int version;
};

#endif // QCHATWIDGET_H
