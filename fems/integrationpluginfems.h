﻿/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                                         *
 *  Copyright (C) 2020 a <a@a.com>                 *
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

#ifndef INTEGRATIONPLUGINFEMS_H
#define INTEGRATIONPLUGINFEMS_H

#include "femsconnection.h"
#include "integrations/integrationplugin.h"
#include <QHash>
#include <QNetworkReply>
#include <QTimer>
#include <QUuid>

class PluginTimer;

class IntegrationPluginFems : public IntegrationPlugin {
  Q_OBJECT

  Q_PLUGIN_METADATA(IID "io.nymea.IntegrationPlugin" FILE
                        "integrationpluginfems.json")
  Q_INTERFACES(IntegrationPlugin)

public:
  explicit IntegrationPluginFems(QObject *parent = nullptr);

  void init() override;

  void setupThing(ThingSetupInfo *info) override;
  void postSetupThing(Thing* thing) override;
  void executeAction(ThingActionInfo *info) override;

  void thingRemoved(Thing *thing) override;


private:

  enum ValueType{
      QSTRING = 0,
      DOUBLE = 1,
      MY_BOOLEAN = 2,
      MY_INT = 3,

  };
  PluginTimer *m_connectionRefreshTimer = nullptr;
  Thing* GetThingByParentAndClassId(Thing* parentThing, ThingClassId identifier);
  QHash<FemsConnection *, Thing *> m_femsConnections;
  void refreshConnection(FemsConnection *connection);
  void updateSumState(FemsConnection *connection);
  void updateMeters(FemsConnection *connection);
  void updateStorages(FemsConnection *connection);
  bool connectionError(FemsNetworkReply *reply);
  void changeMeterString();
  void checkBatteryState(Thing *parentThing);
  void calculateStateOfHealth(Thing *parentThing);

  bool jsonError(QByteArray data);

  QString getValueOfRequestedData(QJsonDocument *json);

  void addValueToThing(Thing *parentThing, ThingClassId identifier,
                       StateTypeId stateName,
                       const QVariant *value,
                       ValueType valueType, int scale);

  void addValueToThing(Thing *childThing, StateTypeId stateName,
                       const QVariant *value, ValueType valueType, int scale = 0);

  QString batteryState = "idle";

  QString meter;

};

#endif // INTEGRATIONPLUGINFEMS_H
