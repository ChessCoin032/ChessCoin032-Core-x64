// chatworker.cpp
#include "chatworker.h"
#include <QNetworkProxy>

ChatWorker::ChatWorker(QObject *parent)
    : QObject(parent), socket(new QTcpSocket(this))
{
    // Configure socket
    socket->setProxy(QNetworkProxy::NoProxy);

    connect(socket, &QTcpSocket::connected, this, &ChatWorker::onConnected);
    connect(socket, &QTcpSocket::disconnected, this, &ChatWorker::onDisconnected);
    connect(socket, &QTcpSocket::readyRead, this, &ChatWorker::onReadyRead);
    connect(socket, &QTcpSocket::bytesWritten, this, &ChatWorker::onBytesWritten);
    connect(socket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::error),
            this, &ChatWorker::onHandleError);
}

ChatWorker::~ChatWorker()
{
    if (socket) {
        socket->disconnectFromHost();
        if (socket->state() == QAbstractSocket::ConnectedState) {
            socket->waitForDisconnected(1000);
        }
        socket->deleteLater();
    }
}


void ChatWorker::connectToServer(const QString &serverIp, quint16 port)
{
    if (socket->state() != QAbstractSocket::UnconnectedState) {
        socket->abort();
    }

    // Configure socket
    socket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
    socket->connectToHost(serverIp, port);

    if (!socket->waitForConnected(5000)) {
        emit errorOccurred(tr("Connection timeout"));
    }
}

void ChatWorker::sendMessage(const QString &message)
{
    if (!socket || socket->state() != QAbstractSocket::ConnectedState) {
        emit errorOccurred(tr("Socket not connected"));
        return;
    }

    socket->write(message.toUtf8());
    socket->flush();
}

void ChatWorker::onConnected()
{
    emit connected();
}

void ChatWorker::onDisconnected()
{
    emit disconnected();
}

void ChatWorker::onReadyRead()
{
    QByteArray data = socket->readAll();
    QString receivedMessage = QString::fromUtf8(data);
    emit messageReceived(receivedMessage);
}

void ChatWorker::onHandleError(QAbstractSocket::SocketError socketError)
{
    Q_UNUSED(socketError);
    emit errorOccurred(socket->errorString());
}

void ChatWorker::onBytesWritten(qint64 bytes)
{
    emit bytesWritten(bytes);
}
