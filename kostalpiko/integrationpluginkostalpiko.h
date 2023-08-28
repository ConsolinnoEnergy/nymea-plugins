/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                                         *
 *  Copyright (C) 2020 Consolinno Energy GmbH <f.stoecker@consolinno.de>                 *
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

#ifndef INTEGRATIONPLUGINKOSTALPIKO_H
#define INTEGRATIONPLUGINKOSTALPIKO_H

#include <integrations/integrationplugin.h>

#include "kostalpikoconnection.h"

#include <QObject>
#include <QHash>
#include <QNetworkReply>
#include <QTimer>
#include <QUuid>

class PluginTimer;

class IntegrationPluginKostalPiko: public IntegrationPlugin
{
    Q_OBJECT

    Q_PLUGIN_METADATA(IID "io.nymea.IntegrationPlugin" FILE "integrationpluginkostalpiko.json")
    Q_INTERFACES(IntegrationPlugin)


public:
    explicit IntegrationPluginKostalPiko();
    void discoverThings(ThingDiscoveryInfo *info) override;
    void setupThing(ThingSetupInfo *info) override;
    void postSetupThing(Thing *info) override;
    void thingRemoved(Thing *thing) override;

private:

    PluginTimer *m_connectionRefreshTimer = nullptr;

    KostalPikoConnection *m_kostalConnection;
    Thing *m_connectionThing = nullptr;

    void refreshConnection();
    //Consumption
    void updateCurrentPower(KostalPikoConnection *connection);
    //Production
    void updateTotalEnergyProduced(KostalPikoConnection *connection);
    bool m_toggle;
};

#endif // INTEGRATIONPLUGINKOSTALPIKO_H
