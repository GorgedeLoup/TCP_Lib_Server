#ifndef SERVER_H
#define SERVER_H

#include "server_global.h"
#include "variable.h"

#include <QObject>
#include <QtNetwork>
#include <QList>
#include <QHash>
#include <QDate>
#include <QTime>
#include <QStringList>
#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(SERVER)

class SERVERSHARED_EXPORT Server : public QObject
{
    Q_OBJECT

public:
    Server(QObject *parent = 0);
    ~Server();

public slots:
    inline void setCoordinate(QHash<float, QList<Spot3DCoordinate> > spot3D){m_spot3D = spot3D;}
    inline void setSpotOrder(QHash<float, QList<int> > spotOrder){m_spotOrder = spotOrder;}
    inline void setParameter(SpotSonicationParameter parameter){m_parameter = parameter;}

    void sendPlanHash();
    void sendCommand(cmdType);
    void progressListen();

private slots:
    void connectServer();    // Connect to client
    void displayError(QAbstractSocket::SocketError);    // Display error
    QString getLocalIP();

    void convertSpot();
    QString writeSendInfo();
    void readSendBack();
    void writtenBytes(qint64);
    void writeConfig();
    void readConfig();

    void progressConnection();
    void readProgress();

signals:
    sendingCompleted();
    error_sendBackCheck();
    error_sendCheck();

private:
    QTcpSocket *m_tcpServer;
    QByteArray m_outBlock;
    QByteArray m_inBlock;

    QStringList m_cmdList;

    qint64 m_totalBytes;    // Total bytes to send for this send progress
    qint64 m_writtenBytes;

    QHash<float, QList<Spot3DCoordinate> > m_spot3D;
    QHash<float, QList<float> > m_hashX;
    QHash<float, QList<float> > m_hashY;
    QHash<float, QList<float> > m_hashZ;
    QHash<float, QList<int> > m_spotOrder;
    SpotSonicationParameter m_parameter;

    QDate m_date;
    QTime m_time;
    int m_sendTimeNum;
    QString m_receivedInfo;

    QString m_ipAddress;
    quint16 m_ipPort;
    quint16 m_ipAnotherPort;

    void setCmdString();

    QTcpServer *m_progressServer;
    QTcpSocket *m_progressSocket;
    qint64 m_progressBytes;
    QHash<QString, int> m_progressHash;
};


#endif // SERVER_H
