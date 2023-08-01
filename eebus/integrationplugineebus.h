/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                                         *
 *  Copyright (C) 2023 Andreas Penzkofer <a.penzkofer@consolinno.de>       *
 *                                                                         *
 *  This library is free software; you can redistribute it and/or          *
 *  modify it under the terms of the GNU Lesser General Public             *
 *  License as published by the Free Software Foundation;                  *
 *  version 3 of the License.                                              *
 *                                                                         *
 *  This library is distributed in the hope that it will be useful,        *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU      *
 *  Lesser General Public License for more details.                        *
 *                                                                         *
 *  You should have received a copy of the GNU Lesser General Public       *
 *  License along with this library; If not, see                           *
 *  <http://www.gnu.org/licenses/>.                                        *
 *                                                                         *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef INTEGRATIONPLUGINEEBUS_H
#define INTEGRATIONPLUGINEEBUS_H

#include "integrations/integrationplugin.h"
#include <QString>
class PluginTimer;
#include <QNetworkRequest>
#include <QtNetwork/QTcpSocket>

class IntegrationPluginEebus : public IntegrationPlugin
{
    Q_OBJECT

    Q_PLUGIN_METADATA(IID "io.nymea.IntegrationPlugin" FILE "integrationplugineebus.json")
    Q_INTERFACES(IntegrationPlugin)

public:
    explicit IntegrationPluginEebus();
    void init() override;
    void discoverThings(ThingDiscoveryInfo *info) override;
    void setupThing(ThingSetupInfo *info) override;
    void postSetupThing(Thing *thing) override;
    void executeAction(ThingActionInfo *info) override;
    void thingRemoved(Thing *thing) override;


private slots:

private:
    PluginTimer *m_timer = nullptr;
    // PluginTimer *m_timer2 = nullptr;  // apparently the plugin does not work with 2 PluginTimers
    QTcpSocket *m_sockets;
    float m_consumptionLimit = 0;

    const char *ip = "127.0.0.1";
    const int port = 20000;
    uint8_t m_retryConnection;
    void refreshValues(Thing *wallbox);
    void createRequestMonitorPowerConsumption(Thing *thing);
    void createRequestMonitorBattery(Thing *thing);
    void createRequestMonitorLPCValue(Thing *thing);
    void onConsumptionLimitChanged(qlonglong);

    void sendDiscoveryRequest();
    void readDiscoveryResponse(ThingDiscoveryInfo *info, QString ownSki);
    void handleDiscoveryResponse(const QByteArray &responseData, ThingDiscoveryInfo *info, QString ownSki);
    void processDiscoveryObject(const QJsonObject &responseObject, ThingDiscoveryInfo *info, QString ownSki);
    void sendOwnSkiRequest();
    void readOwnSkiResponse(ThingDiscoveryInfo *info);
    QString handleOwnSkiResponse(const QByteArray &responseData);


    void sendTrustSkiRequest(ThingSetupInfo *info);
    void readTrustSkiResponse(ThingSetupInfo *info);
    void handleTrustSkiResponse(const QByteArray &responseData);
    void sendGetIdentifierRequest(ThingSetupInfo *info);
    void readGetIdentifierResponse(ThingSetupInfo *info);
    void handleGetIdentifierResponse(const QByteArray &responseData, ThingSetupInfo *info);


};

#endif // INTEGRATIONPLUGINEEBUS_H
