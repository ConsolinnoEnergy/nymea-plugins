#include "femsnetworkreply.h"

FemsNetworkReply::~FemsNetworkReply(){
    if(m_networkReply){
    // We don't need the finished signal any more, object gets deleted
    disconnect(m_networkReply, &QNetworkReply::finished, this, &FemsNetworkReply::finished);

        if (m_networkReply->isRunning()) {
        // Abort the reply, we are not interested in it any more
        m_networkReply->abort();
         }

    m_networkReply->deleteLater();
}
}

QUrl FemsNetworkReply::requestUrl() const{
    return m_request.url();
}


QNetworkRequest FemsNetworkReply::request() const{
    return m_request;
}


QNetworkReply *FemsNetworkReply::networkReply() const{
return m_networkReply;
}


FemsNetworkReply::FemsNetworkReply(const QNetworkRequest &request, QObject *parent) :
    QObject(parent),
            m_request(request){}



void FemsNetworkReply::setNetworkReply(QNetworkReply *networkReply){
    m_networkReply = networkReply;
     // The QNetworkReply will be deleted in the destructor if set
    connect(m_networkReply, &QNetworkReply::finished, this, &FemsNetworkReply::finished);
}

