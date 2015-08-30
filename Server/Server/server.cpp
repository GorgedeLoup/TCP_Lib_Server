#include <QDebug>
#include "server.h"

Q_LOGGING_CATEGORY(SERVER, "SERVER")

Server::Server()
{
// Variables initialization and build connections
    m_sendTimeNum = 1;
    m_totalBytes = 0;
    m_tcpServer = new QTcpSocket(this);

    m_config = readConfig();
    m_config_IPStr = m_config.at(0);
    qDebug() << "CONFIG [Server]/address:" << m_config_IPStr;
    m_config_portInt = m_config.at(1).toInt();
    qDebug() << "CONFIG [Server]/port:" << m_config_portInt;

    connect(m_tcpServer, SIGNAL(readyRead()), this, SLOT(readSendBack()));

    connect(m_tcpServer, SIGNAL(error(QAbstractSocket::SocketError)),
            this, SLOT(displayError(QAbstractSocket::SocketError)));
}

Server::~Server()
{
}


void Server::setCoordinate(QHash<float, QList<Spot3DCoordinate> > spot3D){m_spot3D = spot3D;}

void Server::setSpotOrder(QHash<float, QList<int> > spotOrder){m_spotOrder = spotOrder;}

void Server::setParameter(SpotSonicationParameter parameter){m_parameter = parameter;}


// Connect to client and start session
void Server::connectServer()
{
    QHostAddress ipAddress(m_config_IPStr);    // Set the IP address of another computer
    m_tcpServer->connectToHost(ipAddress, m_config_portInt);    // Connect
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


// Send command start
void Server::sendCommandStart()
{
    connectServer();

    QDataStream m_sendOut(&m_outBlock, QIODevice::WriteOnly);
    m_sendOut.setVersion(QDataStream::Qt_4_6);

    qDebug() << "Sending command start...";

    m_sendOut << qint64(1);
    m_sendOut << qint64(1);

    m_tcpServer->write(m_outBlock);
    m_outBlock.resize(0);

    m_tcpServer->close();
    qDebug() << "Send finished";
    qCDebug(SERVER()) << SERVER().categoryName() << ":" << "SEND COMMAND START";
    qDebug() << "**************************************";
}


// Send command stop
void Server::sendCommandStop()
{
    connectServer();

    QDataStream m_sendOut(&m_outBlock, QIODevice::WriteOnly);
    m_sendOut.setVersion(QDataStream::Qt_4_6);

    qDebug() << "Sending command stop...";

    m_sendOut << qint64(1);
    m_sendOut << qint64(2);

    m_tcpServer->write(m_outBlock);
    m_outBlock.resize(0);

    m_tcpServer->close();
    qDebug() << "Send finished";
    qCDebug(SERVER()) << SERVER().categoryName() << ":" << "SEND COMMAND STOP";
    qDebug() << "**************************************";
}


// Send command pause
void Server::sendCommandPause()
{
    connectServer();

    QDataStream m_sendOut(&m_outBlock, QIODevice::WriteOnly);
    m_sendOut.setVersion(QDataStream::Qt_4_6);

    qDebug() << "Sending command pause...";

    m_sendOut << qint64(1);
    m_sendOut << qint64(3);

    m_tcpServer->write(m_outBlock);

    m_outBlock.resize(0);

    m_tcpServer->close();
    qDebug() << "Send finished";
    qCDebug(SERVER()) << SERVER().categoryName() << ":" << "SEND COMMAND PAUSE";
    qDebug() << "**************************************";
}


// Send command resume
void Server::sendCommandResume()
{
    connectServer();

    QDataStream m_sendOut(&m_outBlock, QIODevice::WriteOnly);
    m_sendOut.setVersion(QDataStream::Qt_4_6);

    qDebug() << "Sending command resume...";

    m_sendOut << qint64(1);
    m_sendOut << qint64(4);

    m_tcpServer->write(m_outBlock);
    m_outBlock.resize(0);

    m_tcpServer->close();
    qDebug() << "Send finished";
    qCDebug(SERVER()) << SERVER().categoryName() << ":" << "SEND COMMAND RESUME";
    qDebug() << "**************************************";
}


void Server::writeConfig()
{
    QSettings *settings = new QSettings(INIPATH, QSettings::IniFormat);

    settings->setValue("Server/address", "172.168.0.116");
    settings->setValue("Server/port", "6666");
    delete settings;
}


QStringList Server::readConfig()
{
    QSettings *settings = new QSettings(INIPATH, QSettings::IniFormat);
    QStringList config;
    config << settings->value("Server/address").toString()
           << settings->value("Server/port").toString();
    delete settings;
    return config;
}
