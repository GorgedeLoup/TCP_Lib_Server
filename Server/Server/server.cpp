#include <QDebug>
#include "server.h"

Q_LOGGING_CATEGORY(SERVER, "SERVER")

Server::Server(QObject *parent) : QObject(parent),
      m_totalBytes(0), m_sendTimeNum(1)
{
// Variables initialization and build connections
    m_tcpServer = new QTcpSocket(this);
    m_progressServer = new QTcpServer(this);
    m_progressSocket = new QTcpSocket(this);

    setCmdString();

    readConfig();
    qDebug() << "CONFIG [Server]/address:" << m_ipAddress;
    qDebug() << "CONFIG [Server]/port:" << m_ipPort;
    qDebug() << "CONFIG [Server]/port2:" << m_ipAnotherPort;

    connect(m_tcpServer, SIGNAL(readyRead()), this, SLOT(readSendBack()));

    connect(m_tcpServer, SIGNAL(error(QAbstractSocket::SocketError)),
            this, SLOT(displayError(QAbstractSocket::SocketError)));
}

Server::~Server()
{
}

// Connect to client and start session
void Server::connectServer()
{
    QHostAddress ipAddress(m_ipAddress);    // Set the IP address of another computer
    m_tcpServer->connectToHost(ipAddress, m_ipPort);    // Connect
}


// Display error report
void Server::displayError(QAbstractSocket::SocketError)
{
    qCWarning(SERVER()) << SERVER().categoryName() << ":" << m_tcpServer->errorString();
    m_tcpServer->close();
}


void Server::convertSpot()
{
    QHash<float, QList<Spot3DCoordinate> >::iterator i;
    for (i = m_spot3D.begin(); i != m_spot3D.end(); ++i)
    {
        float currentKey = i.key();
        QList<Spot3DCoordinate> currentList = i.value();
        QList<float> newListX;
        QList<float> newListY;
        QList<float> newListZ;
        int listSize = currentList.size();
        Spot3DCoordinate currentStruct;
        for (int j = 0; j < listSize; j++)
        {
            currentStruct = currentList.at(j);
            newListX.append(currentStruct.x);
            newListY.append(currentStruct.y);
            newListZ.append(currentStruct.z);
        }
        m_hashX[currentKey] = newListX;
        m_hashY[currentKey] = newListY;
        m_hashZ[currentKey] = newListZ;
    }
    qDebug() << "X:" << m_hashX;
    qDebug() << "Y:" << m_hashY;
    qDebug() << "Z:" << m_hashZ;
}


// Generate the log information of treatment plan sending
QString Server::writeSendInfo()
{
    QString sendInfo;
    QString date = m_date.currentDate().toString();
    QString time = m_time.currentTime().toString();
    QString server = "ServerName";
    QString client = "ClientName";
    QString timeNum = "Time: " + QString::number(m_sendTimeNum, 10);
    sendInfo = "From: " + server + ", " + "To: " + client + ", " + timeNum + ", " + date + ", " + time;
    return sendInfo;
}


// Read send-back information
void Server::readSendBack()
{    
    qDebug() << "readyRead...";
    QDataStream in(m_tcpServer);
    in.setVersion(QDataStream::Qt_4_6);

    qDebug() << "bytesAvail:" << m_tcpServer->bytesAvailable();
    in >> m_receivedInfo;

    qDebug() << "m_receivedInfo:" << m_receivedInfo;
    m_tcpServer->close();
}


// Slot function to capture written bytes of socket
void Server::writtenBytes(qint64 bytesWrite)
{
    qDebug() << "Bytes Written:" << bytesWrite;

    m_writtenBytes = bytesWrite;

    if (m_writtenBytes != m_totalBytes)
    {
        emit error_sendCheck();
        qDebug() << "Send error !";
        qCWarning(SERVER()) << SERVER().categoryName() << ":" << "SEND CHECK FAILED !";
    }
    else {
        qDebug() << "Send checked !";
        qCDebug(SERVER()) << SERVER().categoryName() << ":" << "SEND FINISHED.";
        qDebug() << "-------------------------------------";}
}


// Send treatment plan
void Server::sendPlanHash()
{
    connectServer();

    // Write data
    connect(m_tcpServer, SIGNAL(bytesWritten(qint64)),
            this, SLOT(writtenBytes(qint64)));    // Check if the data has been all well written

    QDataStream m_sendOut(&m_outBlock, QIODevice::WriteOnly);
    m_sendOut.setVersion(QDataStream::Qt_4_6);

    qDebug() << "Sending plan...";
    QString sendInfo = writeSendInfo();

    convertSpot();

    m_sendOut << qint64(0)
              << qint64(0)
              << m_hashX
              << m_hashY
              << m_hashZ
              << m_spotOrder
              << m_parameter.volt
              << m_parameter.totalTime
              << m_parameter.period
              << m_parameter.dutyCycle
              << m_parameter.coolingTime
              << sendInfo;

    qDebug() << "Spot order:" << m_spotOrder;
    qDebug() << "Volt:" << m_parameter.volt << "Total time:" << m_parameter.totalTime << "Period" << m_parameter.period
             << "Duty cycle" << m_parameter.dutyCycle << "Cooling time" << m_parameter.coolingTime;
    qDebug() << "sendInfo:" << sendInfo;

    m_totalBytes = m_outBlock.size();
    qDebug() << "m_totalBytes:" << m_totalBytes;
    m_sendOut.device()->seek(0);
    m_sendOut << qint64(2) << m_totalBytes;    // Find the head of array and write the haed information

    m_tcpServer->write(m_outBlock);
    m_tcpServer->waitForBytesWritten(3000);

    disconnect(m_tcpServer, SIGNAL(bytesWritten(qint64)),
               this, SLOT(writtenBytes(qint64)));

    qDebug() << "Send finished";

    m_tcpServer->waitForReadyRead(3000);

    // Check the consistency of the send-back data
    if(m_receivedInfo == sendInfo)
    {
        qDebug() << "Send-back checked.";
        qCDebug(SERVER()) << SERVER().categoryName() << ":" << "SEND TREATMENT PLAN SECCEEDED.";
        qDebug() << "**************************************";
        // Clear the variables
        m_receivedInfo = "";
        m_sendTimeNum += 1;
        m_writtenBytes = 0;
        m_totalBytes = 0;
        m_outBlock.resize(0);
        emit sendingCompleted();
//        m_spot3D.clear();
//        m_spotOrder.clear();
//        m_parameter.clear();
    }
    else
    {
        emit error_sendBackCheck();
        qCWarning(SERVER()) << SERVER().categoryName() << ":" << "SEND BACK CHECK FAILED !";
        qDebug() << "Check failed ! emit error signal...";
    }
}


void Server::setCmdString()
{
    //  TODO
    m_cmdList << "SEND COMMAND START"
              << "SEND COMMAND STOP"
              << "SEND COMMAND PAUSE"
              << "SEND COMMAND RESUME";
}


void Server::sendCommand(cmdType iType)
{
    connectServer();

    QDataStream m_sendOut(&m_outBlock, QIODevice::WriteOnly);
    m_sendOut.setVersion(QDataStream::Qt_4_6);

    qDebug() << "Start sending command...";

    m_sendOut << qint64(1);
    m_sendOut << qint64(iType);

    m_tcpServer->write(m_outBlock);
    m_outBlock.resize(0);

    m_tcpServer->close();
    qDebug() << "Send finished";
    qCDebug(SERVER()) << SERVER().categoryName() << ":" << m_cmdList.at(iType - 1);
    qDebug() << "**************************************";
    emit sendingCompleted();
}


void Server::readConfig()
{
    QSettings *settings = new QSettings(CONFIG_PATH, QSettings::IniFormat);
    m_ipAddress = settings->value("Server/address").toString();
    m_ipPort = settings->value("Server/port").toString().toUShort(0,10);
    m_ipAnotherPort = settings->value("Server/port2").toString().toUShort(0,10);
    delete settings;
}


void Server::writeConfig()
{
    QSettings *settings = new QSettings(CONFIG_PATH, QSettings::IniFormat);
    settings->setValue("Server/address", m_ipAddress);
    settings->setValue("Server/port", m_ipPort);
    settings->setValue("Server/port2", m_ipAnotherPort);
    delete settings;
}


QString Server::getLocalIP()
{
    QString ipAddress;
    QList<QHostAddress> ipAddressesList = QNetworkInterface::allAddresses();
    for (int i = 0; i < ipAddressesList.size(); ++i) {
        if (ipAddressesList.at(i) != QHostAddress::LocalHost &&
            ipAddressesList.at(i).toIPv4Address() &&
            (ipAddressesList.at(i).toString().indexOf("168") != (-1))) {
            ipAddress = ipAddressesList.at(i).toString();
            break;
        }
    }
    if (ipAddress.isEmpty())
        ipAddress = QHostAddress(QHostAddress::LocalHost).toString();
    return ipAddress;
}


void Server::progressListen()
{
    m_ipAddress = getLocalIP();
    qDebug() << "IP Address:" << m_ipAddress;

    QHostAddress ipAddress(m_ipAddress.toInt());
    if(!m_progressServer->listen(ipAddress, m_ipAnotherPort))
    {
        qCWarning(SERVER()) << SERVER().categoryName() << ":" << m_progressServer->errorString();
        m_progressServer->close();
        return;
    }
    qDebug() << "Progress listen OK";

    connect(m_progressServer, SIGNAL(newConnection()) ,
            this, SLOT(progressConnection()));    // Send newConnection() signal when a new connection is detected
}


void Server::progressConnection()
{
    m_progressSocket = m_progressServer->nextPendingConnection();
    connect(m_progressSocket, SIGNAL(readyRead()), this, SLOT(readProgress()));
    connect(m_progressSocket, SIGNAL(error(QAbstractSocket::SocketError)),
            this, SLOT(displayError(QAbstractSocket::SocketError)));

    qDebug() << "Progress connection OK";
}


void Server::readProgress()
{
    QDataStream in(m_progressSocket);
    in.setVersion(QDataStream::Qt_4_6);

    qDebug() << "Receiving progress information...";

    in >> m_progressBytes
            >> m_progressHash;

    qDebug() << "m_progressBytes:" << m_progressBytes;
    qDebug() << "m_progressHahs:" << m_progressHash;

    m_progressSocket->close();
    qCDebug(SERVER()) << SERVER().categoryName() << ":" << "RECEIVE PROGRESS UPDATE FINISHED.";
    qDebug() << "-------------------";
}
