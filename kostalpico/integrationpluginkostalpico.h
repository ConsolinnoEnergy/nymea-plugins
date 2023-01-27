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

#ifndef INTEGRATIONPLUGINKOSTALPICO_H
#define INTEGRATIONPLUGINKOSTALPICO_H

#include "integrations/integrationplugin.h"
#include "kostalpicoconnection.h"

#include <QHash>
#include <QNetworkReply>
#include <QTimer>
#include <QUuid>

class IntegrationPluginKostalpico: public IntegrationPlugin
{
    Q_OBJECT

    Q_PLUGIN_METADATA(IID "io.nymea.IntegrationPlugin" FILE "integrationpluginkostalpico.json")
    Q_INTERFACES(IntegrationPlugin)


public:
    explicit IntegrationPluginKostalpico();

    //void init() override;

    void discoverThings(ThingDiscoverInfo *info) override;

    void setupThing(ThingSetupInfo *info) override;

    void executeAction(ThingActionInfo *info) override;

    void thingRemoved(Thing *thing) override;

private:

    PluginTimer *m_connectionRefreshTimer = nullptr;

    QHash<KostalPicoConnection *, Thing *> m_kostalConnections;

    void refreshConnection(KostalPicoConnection *connection);

    void updateCurrentPower(KostalPicoConnection *connection);
    void updateTotalEnergyConsumed(KostalPicoConnection *connection);
};

#endif // INTEGRATIONPLUGINKOSTALPICO_H
