/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*
* Copyright 2022 - 2023, Consolinno Energy GmbH
* Contact: info@consolinno.de
*
* GNU Lesser General Public License Usage
* Alternatively, this project may be redistributed and/or modified under the
* terms of the GNU Lesser General Public License as published by the Free
* Software Foundation; version 3. This project is distributed in the hope that
* it will be useful, but WITHOUT ANY WARRANTY; without even the implied
* warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
* Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public License
* along with this project. If not, see <https://www.gnu.org/licenses/>.
*
* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef INTEGRATIONPLUGINASKOMA_H
#define INTEGRATIONPLUGINASKOMA_H

#include <network/networkaccessmanager.h>
#include <network/networkdevicediscovery.h>

#include <integrations/integrationplugin.h>
#include <plugintimer.h>

#include "extern-plugininfo.h"

#include <QObject>
#include <QHostAddress>
#include <QTimer>

#include "askoheat.h"

class NetworkDeviceMonitor;

class IntegrationPluginAskoma: public IntegrationPlugin
{
    Q_OBJECT

    Q_PLUGIN_METADATA(IID "io.nymea.IntegrationPlugin" FILE "integrationpluginaskoma.json")
    Q_INTERFACES(IntegrationPlugin)

public:
    explicit IntegrationPluginAskoma();
    void discoverThings(ThingDiscoveryInfo *info) override;
    void setupThing(ThingSetupInfo *info) override;
    void postSetupThing(Thing *thing) override;
    void thingRemoved(Thing *thing) override;
    void executeAction(ThingActionInfo *info) override;

private:
    QHash<Thing *, Askoheat *> m_askoheats; 
    void setHeatingPower(Thing *thing);

    PluginTimer *m_pluginTimer = nullptr;

    QList<QNetworkReply *> l_pendingReplies;
    NetworkDeviceInfos m_discoveryResults;
};

#endif // INTEGRATIONPLUGINASKOMA_H


