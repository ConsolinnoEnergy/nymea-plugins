#ifndef KOSTALPIKOCONNECTION_H
#define KOSTALPIKOCONNECTION_H

#include <QObject>

#include <QQueue>
#include <QHostAddress>

#include <network/networkaccessmanager.h>

#include "kostalnetworkreply.h"

class KostalPikoConnection : public QObject
{
    Q_OBJECT
public:
    explicit KostalPikoConnection(NetworkAccessManager *networkManager, const QHostAddress &address, QObject *parent = nullptr);

    QHostAddress address() const;

    bool available() const;

    bool busy() const;
    //Has Consumption "GridConsumedPower"
    KostalNetworkReply *getMeasurement();

    KostalNetworkReply *getActiveDevices();
    //Has Production
    //Yields->Yield-> Type Produced -> In Watthours
    KostalNetworkReply *getYields();
    //TODO
    /*
    //yields -> look for Type -> If Inverter -> yes
    KostalNetworkReply *getType();
    KostalNetworkReply *getVersion();


    */
signals:
   void availableChanged(bool available);

private:

   NetworkAccessManager *m_networkManager = nullptr;
   QHostAddress m_address;

   bool m_available = false;

   // Request queue to prevent overloading the device with requests
   KostalNetworkReply *m_currentReply = nullptr;
   QQueue<KostalNetworkReply *> m_requestQueue;

   void sendNextRequest();

};

#endif // KOSTALPIKOCONNECTION_H
