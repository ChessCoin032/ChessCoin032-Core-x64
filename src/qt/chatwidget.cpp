#include "chatwidget.h"
#include "ui_chatwidget.h"
#include "chatworker.h"
#include <QThread>
#include <QMessageBox>
#include <QHostInfo>
#include <QShortcut>
#include <QDateTime>
#include <QAction>
#include <QMenu>
#include <QSettings>
#include "util.h"

#ifdef Q_OS_MAC
#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SystemConfiguration.h>
#endif

QChatWidget::QChatWidget(QWidget *parent, int ver)
    : QWidget(parent)
    , ui(new Ui::chatWidget)
    , workerThread(new QThread(this))
    , worker(new ChatWorker())
    , checkPortTimer(new QTimer(this))
    , bConnected(false)
    , messageHistory()
{
    ui->setupUi(this);
    ui->connButton->hide();

    version = ver;
    chatServerIndex = -1;
    vChatServers.push_back(QString(CHATSERVER0));
    vChatServers.push_back(QString(CHATSERVER1));

    QString computerName = getComputerName();
    ui->labelHost->setText(computerName);

    connect(this, &QChatWidget::clearRequested, this, &QChatWidget::on_clearButton_clicked);

    connect(ui->txtSend, &QLineEdit::returnPressed, this, &QChatWidget::on_sendButton_clicked);

    // Set the layout of the main widget
    setLayout(ui->vertLayout);

    ui->txtReceive->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(ui->txtReceive, &QTextEdit::customContextMenuRequested, this, [=](const QPoint &pos) {
        QMenu* menu = ui->txtReceive->createStandardContextMenu();
        QAction* clearAction = new QAction("Clear All", menu);

        connect(clearAction, &QAction::triggered, this, [=]() {
            emit clearRequested();
        });

        menu->addSeparator();  // Optional: visual separation
        menu->addAction(clearAction);
        menu->exec(ui->txtReceive->mapToGlobal(pos));
        delete menu;  // Clean up
    });

    connect(workerThread, &QThread::finished, worker, &QObject::deleteLater);
    connect(worker, &ChatWorker::connected, this, &QChatWidget::onConnected);
    connect(worker, &ChatWorker::disconnected, this, &QChatWidget::onDisconnected);
    connect(worker, &ChatWorker::messageReceived, this, &QChatWidget::onReadyRead);
    connect(worker, &ChatWorker::errorOccurred, this, &QChatWidget::onHandleError);
    connect(worker, &ChatWorker::bytesWritten, this, &QChatWidget::onBytesWritten);
    connect(this,   &QChatWidget::connectToServer, worker, &ChatWorker::connectToServer);
    connect(this,   &QChatWidget::sendMessage, worker, &ChatWorker::sendMessage);

    worker->moveToThread(workerThread);
    workerThread->start();

    tryConnect();

    // Setup and start the timer
    connect(checkPortTimer, &QTimer::timeout, this, &QChatWidget::onCheckPort);
    checkPortTimer->start(5 * 60 * 1000);  // Check every 5 minutes

    // Setup cleanup timer for message history   
    cleanupTimer = new QTimer(this);
    connect(cleanupTimer, &QTimer::timeout, this, [this]() {
        QMutexLocker locker(&messageHistoryMutex);
        qint64 now = QDateTime::currentMSecsSinceEpoch();
        for (auto it = messageHistory.begin(); it != messageHistory.end(); ) {
            if (now - it.value() > 5000) {  // 5-second retention
                it = messageHistory.erase(it);
            } else {
                ++it;
            }
        }
    });
    cleanupTimer->start(10000);  // Clean every 10 seconds
}

QChatWidget::~QChatWidget()
{
    messageHistory.clear();

    workerThread->quit();
    workerThread->wait();

    delete ui;
}

void QChatWidget::tryConnect()
{
    int port = (version > 1) ? CHATSERVERPORT1 : CHATSERVERPORT0;

    strChatSvrAddr = getChatServerAddr();
    emit connectToServer(strChatSvrAddr, port);
}

void QChatWidget::onConnected()
{
    QString message = "You are now online with Chat " + QString::number(version) + ".0";
    appendChatLog(message);
    setConnectState(true);
}

void QChatWidget::onDisconnected()
{
    QString message = "You are offline with Chat " + QString::number(version) + ".0";
    appendChatLog(message, Qt::red);

    setConnectState(false);

    if (version > 1)
    {
        MilliSleep(500);
        appendChatLog(QString("Switching to %1 server now...").arg(getNextServerName()));
        tryConnect();
    }
}

void QChatWidget::on_sendButton_clicked()
{
    QString message = ui->txtSend->text();

    if (message.isEmpty())
    {
        QMessageBox::critical(this, tr("Send Failed"), tr("Please enter a message before sending."));
        return;
    }

    if (!bConnected)
    {
        QMessageBox::critical(this, tr("Send Failed"), tr("Unable to send.\nConnection has been lost."));
        return;
    }

    if (!message.compare(QString("$echo"), Qt::CaseInsensitive))
    {
        QMessageBox::critical(this, tr("Send Failed"), tr("Your message contains restricted words and cannot be sent."));
        ui->txtSend->setText("");
        return;
    }

    if (message.startsWith('/')) {
        QMessageBox::critical(this, tr("Send Failed"), tr("You cannot send messages that begin with '/'.\nThese are reserved for commands."));
        return;
    }

    int length = message.length();
    if (length > 1024)
    {
        QMessageBox::critical(this, tr("Send Failed"), tr("You cannot send more than 1024 characters."));
        return;
    }

    appendChatLog(message, Qt::yellow);

    emit sendMessage(message);

    ui->txtSend->setText("");
}

void QChatWidget::onReadyRead(const QString &message)
{
    setConnectState(true);

    {
        QString currentMessage = message.trimmed();

        // Deduplication check for received messages
        QMutexLocker locker(&messageHistoryMutex);

        qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (messageHistory.contains(currentMessage)) {
            qint64 lastReceived = messageHistory.value(currentMessage);
            if (now - lastReceived < 1000) { // 1-second
                return;
            }
        }

        messageHistory.insert(currentMessage, now);
    }

    appendChatLog(message);
}

void QChatWidget::onHandleError(const QString &errorString)
{
    appendChatLog(errorString, Qt::red);

    setConnectState(false);
}

void QChatWidget::on_clearButton_clicked()
{
    QString selectedTxt = ui->txtReceive->textCursor().selectedText();
    if (selectedTxt.isEmpty())
    {
        QMessageBox::StandardButton msgRet;
        msgRet = QMessageBox::question(this, tr("chesscoin-qt"),
            tr("Do you want to clear all chat histories?"), QMessageBox::Yes | QMessageBox::No);

        if (msgRet == QMessageBox::No)
            return;

        ui->txtReceive->clear();
        return;
    }

    ui->txtReceive->textCursor().removeSelectedText();
}

void QChatWidget::onBytesWritten(qint64 bytes)
{
    Q_UNUSED(bytes);

}

void QChatWidget::on_connButton_clicked()
{
    tryConnect();
}

QString QChatWidget::getUTCTimeForChatroom()
{
    // Convert local date and time to UTC
    QDateTime localDateTime = QDateTime::currentDateTime();
    QDateTime utcDateTime = localDateTime.toUTC();
    return utcDateTime.toString("[hh:mm:ss] ");
}

void QChatWidget::setConnectState(bool connected)
{
    if (connected)
    {
        ui->labelState->setPixmap(QIcon(":/icons/connected").pixmap(20, 20));
        ui->labelState->update();
        if (version > 1) {
            QString msg = QString("%1 Server : <b>ONLINE</b>").arg(getCurrentServerName());
            ui->labelStateText->setText(msg);
        }
        else
            ui->labelStateText->setText("Server : <b>ONLINE</b>");

        ui->connButton->hide();

        if (!bConnected) {
            appendChatLog(tr("Secure connection established"));
        }
    }
    else
    {
        ui->labelState->setPixmap(QIcon(":/icons/disconnected").pixmap(20, 20));
        ui->labelState->update();

        if (version > 1) {
            QString msg = QString("%1 Server : <b>OFFLINE</b>").arg(getCurrentServerName());
            ui->labelStateText->setText(msg);
            ui->connButton->hide();
        }
        else {
            ui->labelStateText->setText("Server : <b>OFFLINE</b>");
            ui->connButton->show();
        }

        if (bConnected) {
            appendChatLog(tr("Connection has been lost"));
        }
    }

    bConnected = connected;
}

void QChatWidget::onCheckPort()
{
    int port = (version > 1) ? CHATSERVERPORT1 : CHATSERVERPORT0;

    QTcpSocket testSocket;
    testSocket.connectToHost(strChatSvrAddr, port);

    if (testSocket.waitForConnected(5000)) {
        testSocket.disconnectFromHost();
        setConnectState(true);
    }
    else {
        setConnectState(false);
    }

    if (version > 1)
    {
        if (!bConnected) {
            MilliSleep(500);
            appendChatLog(QString("Switching to %1 server now...").arg(getCurrentServerName()));
            tryConnect();
        }
    }
}

QString QChatWidget::getCurrentServerName()
{
    int idx = chatServerIndex % CHATSERVERCOUNT;
    if (idx == 0)
        return tr(NAMECHATSERVER0);

    return tr(NAMECHATSERVER1);
}

QString QChatWidget::getNextServerName()
{
    int idx = chatServerIndex % CHATSERVERCOUNT;
    if (idx == 0)
        return tr(NAMECHATSERVER1);

    return tr(NAMECHATSERVER0);
}

QString QChatWidget::getChatServerAddr()
{
    if (version > 1)
        chatServerIndex++;
    else
        chatServerIndex = 0;

    return vChatServers[chatServerIndex % CHATSERVERCOUNT];
}

void QChatWidget::appendChatLog(const QString& msg, int textColor)
{
    QString sendtime = getUTCTimeForChatroom();
    QString formattedText;

    if (textColor == Qt::white)
        formattedText = QString("<font color='white'>%1%2</font>").arg(sendtime, msg);
    else if (textColor == Qt::red)
        formattedText = QString("<font color='red'>%1%2</font>").arg(sendtime, msg);
    else if (textColor == Qt::yellow)
        formattedText = QString("<font color='yellow'>%1%2</font>").arg(sendtime, msg);
    else
        formattedText = QString("<font color='green'>%1%2</font>").arg(sendtime, msg);

    ui->txtReceive->append(formattedText);
}

QString QChatWidget::getComputerName()
{
    QString computerName = "";

#ifdef Q_OS_MAC
    CFStringRef nameRef = SCDynamicStoreCopyComputerName(NULL, NULL);
    if (nameRef) {
        computerName = QString::fromCFString(nameRef);
        CFRelease(nameRef);
        return computerName;
    }
#endif

    computerName = QHostInfo::localHostName();
    return computerName;
}
