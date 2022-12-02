#include "femsconnection.h"

#include <QUrlQuery>

FemsConnection::FemsConnection(NetworkAccessMAnager * networkAccessManager, const QHostAddress &address, QObject *parent) :
    QObject(parent),
    m_networkManager(networkAccessManager),
    m_address(address)
{

}


QHostAddress FemsConnection::address() const{
    return m_address;
}


bool FemsConnection::available() const {
    return m_available;
}

bool FemsConnection::busy() const{
    return m_request_Queue.count()> 1;
}

/*
    FemsNetworkReply *getActiveDevices();
    FemsNetworkReply *getSumRealData(String sumId);
    FemsNetworkReply *getStorageData(String storageId);
    FemsNetworkReply *getMeterData(String meterId);
    FemsNetworkReply *getFemsState(String sumId);

*/
