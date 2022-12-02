#include "femsconnection.h"

#include <QUrlQuery>

FemsConnection::FemsConnection(NetworkAccessManager * networkAccessManager, const QHostAddress &address, QObject *parent, QString user, QString password, bool useEdge, QString port) :
    QObject(parent),
    m_networkManager(networkAccessManager),
    m_address(address),
    m_user(user),
    m_password(password),
    m_use_edge(useEdge),
    m_port(port)
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


/**
 * @brief FemsConnection::isAvailable Check if the Connection to FEMS/OpenEMS Edge is possible at all
 * Create a Basic Connection attempt by reading the _sum/state channel and check the network reply.
 * @return a FemsNetworkReply
 */
FemsNetworkReply *FemsConnection::isAvailable(){
    return this->getFemsDataPoint("_sum/data");
}

FemsNetworkReply FemsConnection::getFemsDataPoint(QString extraPath){
    FemsNetworkReply* reply;
    m_request_Queue.enqueue(reply);

    connect(reply, &FemsNetworkReply::finished, this, [=](){
        if (reply->networkReply()->error() == QNetworkReply::NoError) {
            // Reply was successfully, we can communicate
            if (!m_available) {
                qCDebug(dcFronius()) << "Connection: the connection is now available";
                m_available = true;
                emit availableChanged(m_available);

                // Destroy any pending requests
                qDeleteAll(m_requestQueue);
                m_requestQueue.clear();
            }
        } else {
            // Ther have been errors, seems like we not available any more
            if (m_available) {
                qCDebug(dcFronius()) << "Connection: the connection is not available any more:" << reply->networkReply()->errorString();
                m_available = false;
                emit availableChanged(m_available);
            }
        }
    });

    sendNextRequest();
    return reply;
}


/**
 * @brief FemsConnection::getFemsReply This Method generates a FemsNetworkReply depending on the Configuration (Edge or FEMS)
 * @param path The Path that will be added e.g. _sum/state or _sum/EssSoc etc
 * @return A new FemsNetwork Reply depending on -> FEMS or EDGE connection.
 */
QUrl FemsConnection::getFemsReply(QString path){

   QUrl url;
   QString pathUrl = "/rest/channel/" + path;
   requestUrl.setScheme("http");
   requestUrl.setHost(this->m_address);
   requestUrl.setPort(this->m_port);
   requestUrl.setPath(pathUrl);
   if(this->m_use_edge){
      return new FemsNetworkReply(QNetworkRequest(requestUrl), this, this->m_user, this->m_password);

   } else {
       //http://<BENUTZER>:<PASSWORT>@<IP>:80/rest/channel/Component/Channel
       requestUrl.setUserName(this->m_user);
       requestUrl.setPassword(this->m_password);
       return new FemsNetworkReply(QNetworkRequest(requestUrl), this);
   }
}


/**
 * @brief FemsConnection::sendNextRequest Check if the currentReply has enqueued requests. And work.
 */
void FemsConnection::sendNextRequest(){
    if (m_currentReply)
        return;

    if (m_requestQueue.isEmpty())
        return;

    m_currentReply = m_requestQueue.dequeue();

//    qCDebug(dcFronius()) << "Connection: Sending request" << m_currentReply->request().url().toString();
    m_currentReply->setNetworkReply(m_networkManager->get(m_currentReply->request()));

    connect(m_currentReply, &FroniusNetworkReply::finished, this, [=](){
        if (m_currentReply->networkReply()->error() != QNetworkReply::NoError) {
            qCWarning(dcFems()) << "Connection: Request finished with error:" << m_currentReply->networkReply()->error() << "for url" << m_currentReply->request().url().toString();
        }

        // Note: the network reply will be deleted in the destructor
        m_currentReply->deleteLater();

        m_currentReply = nullptr;
        sendNextRequest();
    });


}
