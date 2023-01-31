#include "kostalpicoconnection.h"

#include <QDebug>
#include <QUrlQuery>

KostalPicoConnection::KostalPicoConnection(NetworkAccessManager *networkManager,
                                           const QHostAddress &address,
                                           QObject *parent)
    : QObject(parent), m_networkManager(networkManager), m_address(address) {}
QHostAddress KostalPicoConnection::address() const { return m_address; }

bool KostalPicoConnection::available() const { return m_available; }

bool KostalPicoConnection::busy() const { return m_requestQueue.count() > 1; }
// TODO
/*
KostalNetworkReply *KostalPicoConnection::getVersion()
{
    QUrl requestUrl;
    requestUrl.setScheme("http");
    requestUrl.setHost(m_address.toString());
    requestUrl.setPath("/solar_api/GetAPIVersion.cgi");

    FroniusNetworkReply *reply = new
FroniusNetworkReply(QNetworkRequest(requestUrl), this);
    m_requestQueue.enqueue(reply);
    sendNextRequest();
    return reply;
}*/

KostalNetworkReply *KostalPicoConnection::getActiveDevices() {
  QUrl requestUrl;
  requestUrl.setScheme("http");
  requestUrl.setHost(m_address.toString());
  requestUrl.setPath("yields.xml");

  KostalNetworkReply *reply =
      new KostalNetworkReply(QNetworkRequest(requestUrl), this);
  m_requestQueue.enqueue(reply);

  // Note: we use this request for detecting if the logger is available or not.
  connect(reply, &KostalNetworkReply::finished, this, [=]() {
    if (reply->networkReply()->error() == QNetworkReply::NoError) {
      // Reply was successfully, we can communicate
      if (!m_available) {
        // qCDebug(dcKostal()) << "Connection: the connection is now available";
        m_available = true;
        emit availableChanged(m_available);

        // Destroy any pending requests
        qDeleteAll(m_requestQueue);
        m_requestQueue.clear();
      }
    } else {
      // Ther have been errors, seems like we not available any more
      if (m_available) {
        // qCDebug(dcKostal()) << "Connection: the connection is not available
        // any more:" << reply->networkReply()->errorString();
        m_available = false;
        emit availableChanged(m_available);
      }
    }
  });

  sendNextRequest();
  return reply;
}

// Watt in Type -> GridPower
KostalNetworkReply *KostalPicoConnection::getMeasurement() {
  QUrl requestUrl;
  requestUrl.setScheme("http");
  requestUrl.setHost(m_address.toString());
  requestUrl.setPath("measurements.xml");
  QUrlQuery query;
  // only allow Inverter atm
  query.addQueryItem("Type", "Inverter");
  requestUrl.setQuery(query);
  KostalNetworkReply *reply =
      new KostalNetworkReply(QNetworkRequest(requestUrl), this);
  m_requestQueue.enqueue(reply);
  sendNextRequest();
  return reply;
}
// Has Production
// Yields->Yield-> Type Produced -> In Watthours
KostalNetworkReply *KostalPicoConnection::getYields() {
  QUrl requestUrl;
  requestUrl.setScheme("http");
  requestUrl.setHost(m_address.toString());
  requestUrl.setPath("yields.xml");
  QUrlQuery query;
  // only allow Inverter atm
  query.addQueryItem("Type", "Inverter");
  requestUrl.setQuery(query);
  KostalNetworkReply *reply =
      new KostalNetworkReply(QNetworkRequest(requestUrl), this);
  m_requestQueue.enqueue(reply);
  sendNextRequest();
  return reply;
}

void KostalPicoConnection::sendNextRequest() {
  if (m_currentReply)
    return;

  if (m_requestQueue.isEmpty())
    return;

  m_currentReply = m_requestQueue.dequeue();

  m_currentReply->setNetworkReply(
      m_networkManager->get(m_currentReply->request()));

  connect(m_currentReply, &KostalNetworkReply::finished, this, [=]() {
    // Note: the network reply will be deleted in the destructor
    m_currentReply->deleteLater();

    m_currentReply = nullptr;
    sendNextRequest();
  });
}
