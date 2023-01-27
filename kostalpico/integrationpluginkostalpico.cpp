/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                                         *
 *  Copyright (C) 2020 Consolinno Energy GmbH <f.stoecker@consolinno.de> *
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

#include "integrationpluginkostalpico.h"
#include "plugininfo.h"
#include <QXmlStreamReader>

IntegrationPluginKostalpico::IntegrationPluginKostalpico() {}

void IntegrationPluginKostalpico::discoverThings(ThingDiscoverInfo *info) {

  if (!hardwareManager()->networkDeviceDiscovery()->available()) {
    qCWarning(dcKostalpico()) << "Failed to discover network devices. The "
                                 "network device discovery is not available.";
    info->finish(Thing::ThingErrorHardwareNotAvailable,
                 QT_TR_NOOP("Unable to discover devices in your network."));
    return;
  }

  qCDebug(dcKostalpico()) << "Starting network discovery...";
  NetworkDeviceDiscoveryReply *discoveryReply =
      hardwareManager()->networkDeviceDiscovery()->discover();
  connect(discoveryReply, &NetworkDeviceDiscoveryReply::finished,
          discoveryReply, &NetworkDeviceDiscoveryReply::deleteLater);
  connect(discoveryReply, &NetworkDeviceDiscoveryReply::finished, info, [=]() {
    ThingDescriptors descriptors;
    qCDebug(dcKostalpico())
        << "Discovery finished. Found"
        << discoveryReply->networkDeviceInfos().count() << "devices";
    foreach (const NetworkDeviceInfo &networkDeviceInfo,
             discoveryReply->networkDeviceInfos()) {
      qCDebug(dcKostalpico()) << networkDeviceInfo;
      if (networkDeviceInfo.macAddress().isNull())
        continue;

      // Hostname or MAC manufacturer must include Fronius
      if (!(networkDeviceInfo.macAddressManufacturer().toLower().contains(
                "Atmel") ||
            networkDeviceInfo.hostName().toLower().contains("PikoPlus")))
        continue;

      QString title;
      if (networkDeviceInfo.hostName().isEmpty()) {
        title += "Piko Plus";
      } else {
        title += "Piko Plus (" + networkDeviceInfo.hostName() + ")";
      }

      QString description;
      if (networkDeviceInfo.macAddressManufacturer().isEmpty()) {
        description = networkDeviceInfo.macAddress();
      } else {
        description = networkDeviceInfo.macAddress() + " (" +
                      networkDeviceInfo.macAddressManufacturer() + ")";
      }

      ThingDescriptor descriptor(connectionThingClassId, title, description);

      // Check if we already have set up this device
      Things existingThings = myThings().filterByParam(
          connectionThingMacParamTypeId, networkDeviceInfo.macAddress());
      if (existingThings.count() == 1) {
        qCDebug(dcKostalpico()) << "This thing already exists in the system."
                                << existingThings.first() << networkDeviceInfo;
        descriptor.setThingId(existingThings.first()->id());
      }

      ParamList params;
      params << Param(connectionThingAddressParamTypeId,
                      networkDeviceInfo.address().toString());
      params << Param(connectionThingMacParamTypeId,
                      networkDeviceInfo.macAddress());
      descriptor.setParams(params);
      info->addThingDescriptor(descriptor);
    }
    info->finish(Thing::ThingErrorNoError);
  });
}
void IntegrationPluginKostalpico::setupThing(ThingSetupInfo *info) {
  // A thing is being set up. Use info->thing() to get details of the thing, do
  // the required setup (e.g. connect to the device) and call info->finish()
  // when done.
  Thing *thing = info->thing();
  qCDebug(dcKostalpico()) << "Setup thing" << info->thing();

  if (thing->thingClassId() == connectionThingClassId) {

    QHostAddress address(
        thing->paramValue(connectionThingAddressParamTypeId).toString());

    // Handle reconfigure
    if (m_kostalConnections.values().contains(thing)) {
      KostalPicoConnection *connection = m_kostalConnections.key(thing);
      m_kostalConnections.remove(connection);
      connection->deleteLater();
    }

    // Create the connection
    KostalPicoConnection *connection = new KostalPicoConnection(
        hardwareManager()->networkManager(), address, thing);

    // Verify the version
    KostalNetworkReply *reply = connection->getActiveDevices();
    connect(reply, &KostalNetworkReply::finished, info, [=] {
      QByteArray data = reply->networkReply()->readAll();
      if (reply->networkReply()->error() != QNetworkReply::NoError) {
        qCWarning(dcKostalpico())
            << "Network request error:" << reply->networkReply()->error()
            << reply->networkReply()->errorString()
            << reply->networkReply()->url();
        if (reply->networkReply()->error() ==
            QNetworkReply::ContentNotFoundError) {
          info->finish(
              Thing::ThingErrorHardwareNotAvailable,
              QT_TR_NOOP(
                  "The device does not reply to our requests. Please verify "
                  "that the Fronius Solar API is enabled on the device."));
        } else {
          info->finish(Thing::ThingErrorHardwareNotAvailable,
                       QT_TR_NOOP("The device is not reachable."));
        }
        return;
      }

      // Convert the rawdata to a JSON document

      QXmlStreamReader xmlDoc = new QXmlStreamReader(&data);
      if (xmlDoc.hasError()) {
        qCWarning(dcKostalpico()) << "Failed to parse XML data" << data;
        info->finish(Thing::ThingErrorHardwareFailure,
                     QT_TR_NOOP("The data received from the device could not "
                                "be processed because the format is unknown."));
        return;
      }
      // TODO / Possible check for Version ?

      m_kostalConnections.insert(connection, thing);
      info->finish(Thing::ThingErrorNoError);

      // Update the already known states
      thing->setStateValue("connected", true);
    });

    connect(connection, &KostalPicoConnection::availableChanged, this,
            [=](bool available) {
              qCDebug(dcKostalpico())
                  << thing << "Available changed" << available;
              thing->setStateValue("connected", available);

              if (!available) {
                // Update all child things, they will be set to available once
                // the connection starts working again
                foreach (Thing *childThing,
                         myThings().filterByParentId(thing->id())) {
                  childThing->setStateValue("connected", false);
                }
              }
            });

  } else if ((thing->thingClassId() == kostalpicoThingClassId)) {

    // Verify the parent connection
    Thing *parentThing = myThings().findById(thing->parentId());
    if (!parentThing) {
      qCWarning(dcKostalpico()) << "Could not find the parent for" << thing;
      info->finish(Thing::ThingErrorHardwareNotAvailable);
      return;
    }

    KostalPicoConnection *connection = m_kostalConnections.key(parentThing);
    if (!connection) {
      qCWarning(dcKostalpico())
          << "Could not find the parent connection for" << thing;
      info->finish(Thing::ThingErrorHardwareNotAvailable);
      return;
    }

    info->finish(Thing::ThingErrorNoError);

  } else {
    Q_ASSERT_X(false, "setupThing",
               QString("Unhandled thingClassId: %1")
                   .arg(thing->thingClassId().toString())
                   .toUtf8());
  }
}

void IntegrationPluginKostalpico::postSetupThing(Thing *thing) {
  qCDebug(dcKostalpico()) << "Post setup" << thing->name();

  if (thing->thingClassId() == connectionThingClassId) {

    // Create a refresh timer for monitoring the active devices
    if (!m_connectionRefreshTimer) {
      m_connectionRefreshTimer =
          hardwareManager()->pluginTimerManager()->registerTimer(2);
      connect(m_connectionRefreshTimer, &PluginTimer::timeout, this, [this]() {
        foreach (KostalPicoConnection *connection, m_kostalConnections.keys()) {
          refreshConnection(connection);
        }
      });
      m_connectionRefreshTimer->start();
    }

    // Refresh now
    KostalPicoConnection *connection = m_kostalConnections.key(thing);
    if (connection) {
      refreshConnection(connection);
    }
  }
}

void IntegrationPluginKostalpico::executeAction(ThingActionInfo *info) {
  Q_UNUSED(info)
}

void IntegrationPluginKostalpico::thingRemoved(Thing *thing) {
  if (thing->thingClassId() == connectionThingClassId) {
    KostalPicoConnection *connection = m_kostalConnections.key(thing);
    m_kostalConnections.remove(connection);
    connection->deleteLater();
  }

  if (myThings().filterByThingClassId(connectionThingClassId).isEmpty()) {
    hardwareManager()->pluginTimerManager()->unregisterTimer(
        m_connectionRefreshTimer);
    m_connectionRefreshTimer = nullptr;
  }
}
void IntegrationPluginKostalpico::refreshConnection(
    KostalPicoConnection *connection) {
  if (connection->busy()) {
    qCWarning(dcKostalpico()) << "Connection busy. Skipping refresh cycle for host"
                           << connection->address().toString();
    return;
  }

  // Note: this call will be used to monitor the available state of the
  // connection internally
  KostalNetworkReply *reply = connection->getActiveDevices();
  connect(reply, &KostalNetworkReply::finished, this, [=]() {
    if (reply->networkReply()->error() != QNetworkReply::NoError) {
      // Note: the connection warns about any errors if available changed
      return;
    }

    Thing *connectionThing = m_kostalConnections.value(connection);
    if (!connectionThing)
      return;

    QByteArray data = reply->networkReply()->readAll();

    QXmlStreamReader xmlDoc = new QXmlStreamReader(&data);
    if (xmlDoc.hasError()) {
      qCWarning(dcKostalpico())
          << "Failed to parse XML data" << data << ":" << error.errorString();
      return;
    }

    // Parse the data for thing information
    QList<ThingDescriptor> thingDescriptors;

    QVariantMap bodyMap = jsonDoc.toVariant().toMap().value("Body").toMap();
    // qCDebug(dcKostalpico()) << "System:" <<
    // qUtf8Printable(QJsonDocument::fromVariant(bodyMap).toJson());

    // Check if there are new inverters
    QVariantMap inverterMap =
        bodyMap.value("Data").toMap().value("Inverter").toMap();
    foreach (const QString &inverterId, inverterMap.keys()) {
      QVariantMap inverterInfo = inverterMap.value(inverterId).toMap();
      const QString serialNumber = inverterInfo.value("Serial").toString();

      // Note: we use the id to identify for backwards compatibility
      if (myThings()
              .filterByParentId(connectionThing->id())
              .filterByParam(inverterThingIdParamTypeId, inverterId)
              .isEmpty()) {
        QString thingDescription = connectionThing->name();
        ThingDescriptor descriptor(inverterThingClassId,
                                   "Fronius Solar Inverter", thingDescription,
                                   connectionThing->id());
        ParamList params;
        params.append(Param(inverterThingIdParamTypeId, inverterId));
        params.append(
            Param(inverterThingSerialNumberParamTypeId, serialNumber));
        descriptor.setParams(params);
        thingDescriptors.append(descriptor);
      }
    }

    // Check if there are new meters
    QVariantMap meterMap = bodyMap.value("Data").toMap().value("Meter").toMap();
    foreach (const QString &meterId, meterMap.keys()) {
      // Note: we use the id to identify for backwards compatibility
      if (myThings()
              .filterByParentId(connectionThing->id())
              .filterByParam(meterThingIdParamTypeId, meterId)
              .isEmpty()) {
        // Get the meter realtime data for details
        FroniusNetworkReply *realtimeDataReply =
            connection->getMeterRealtimeData(meterId.toInt());
        connect(realtimeDataReply, &FroniusNetworkReply::finished, this, [=]() {
          if (realtimeDataReply->networkReply()->error() !=
              QNetworkReply::NoError) {
            return;
          }

          QByteArray data = realtimeDataReply->networkReply()->readAll();

          QJsonParseError error;
          QJsonDocument jsonDoc = QJsonDocument::fromJson(data, &error);
          if (error.error != QJsonParseError::NoError) {
            qCWarning(dcKostalpico()) << "Meter: Failed to parse JSON data" << data
                                   << ":" << error.errorString();
            return;
          }

          // Parse the data and update the states of our device
          QVariantMap dataMap = jsonDoc.toVariant()
                                    .toMap()
                                    .value("Body")
                                    .toMap()
                                    .value("Data")
                                    .toMap();
          QString thingName;
          QString serialNumber;

          if (dataMap.contains("Details")) {
            QVariantMap details = dataMap.value("Details").toMap();
            thingName = details.value("Manufacturer", "Fronius").toString() +
                        " " + details.value("Model", "Smart Meter").toString();
            serialNumber = details.value("Serial").toString();
          } else {
            thingName = connectionThing->name() + " Meter " + meterId;
          }

          ThingDescriptor descriptor(meterThingClassId, thingName, QString(),
                                     connectionThing->id());
          ParamList params;
          params.append(Param(meterThingIdParamTypeId, meterId));
          params.append(Param(meterThingSerialNumberParamTypeId, serialNumber));
          descriptor.setParams(params);
          emit autoThingsAppeared(ThingDescriptors() << descriptor);
        });
      }
    }

    // Check if there are new energy storages
    QVariantMap storageMap =
        bodyMap.value("Data").toMap().value("Storage").toMap();
    foreach (const QString &storageId, storageMap.keys()) {
      // Note: we use the id to identify for backwards compatibility
      if (myThings()
              .filterByParentId(connectionThing->id())
              .filterByParam(storageThingIdParamTypeId, storageId)
              .isEmpty()) {

        // Get the meter realtime data for details
        FroniusNetworkReply *realtimeDataReply =
            connection->getStorageRealtimeData(storageId.toInt());
        connect(realtimeDataReply, &FroniusNetworkReply::finished, this, [=]() {
          if (realtimeDataReply->networkReply()->error() !=
              QNetworkReply::NoError) {
            return;
          }

          QByteArray data = realtimeDataReply->networkReply()->readAll();

          QJsonParseError error;
          QJsonDocument jsonDoc = QJsonDocument::fromJson(data, &error);
          if (error.error != QJsonParseError::NoError) {
            qCWarning(dcKostalpico()) << "Storage: Failed to parse JSON data"
                                   << data << ":" << error.errorString();
            return;
          }

          // Parse the data and update the states of our device
          QVariantMap dataMap = jsonDoc.toVariant()
                                    .toMap()
                                    .value("Body")
                                    .toMap()
                                    .value("Data")
                                    .toMap()
                                    .value("Controller")
                                    .toMap();

          QString thingName;
          QString serialNumber;
          if (dataMap.contains("Details")) {
            QVariantMap details = dataMap.value("Details").toMap();
            thingName = details.value("Manufacturer", "Fronius").toString() +
                        " " +
                        details.value("Model", "Energy Storage").toString();
            serialNumber = details.value("Serial").toString();
          } else {
            thingName = connectionThing->name() + " Storage " + storageId;
          }

          ThingDescriptor descriptor(storageThingClassId, thingName, QString(),
                                     connectionThing->id());
          ParamList params;
          params.append(Param(storageThingIdParamTypeId, storageId));
          params.append(
              Param(storageThingSerialNumberParamTypeId, serialNumber));
          descriptor.setParams(params);
          emit autoThingsAppeared(ThingDescriptors() << descriptor);
        });
      }
    }

    // Inform about unhandled devices
    QVariantMap ohmpilotMap =
        bodyMap.value("Data").toMap().value("Ohmpilot").toMap();
    foreach (QString ohmpilotId, ohmpilotMap.keys()) {
      qCDebug(dcKostalpico()) << "Unhandled device Ohmpilot" << ohmpilotId;
    }

    QVariantMap sensorCardMap =
        bodyMap.value("Data").toMap().value("SensorCard").toMap();
    foreach (QString sensorCardId, sensorCardMap.keys()) {
      qCDebug(dcKostalpico()) << "Unhandled device SensorCard" << sensorCardId;
    }

    QVariantMap stringControlMap =
        bodyMap.value("Data").toMap().value("StringControl").toMap();
    foreach (QString stringControlId, stringControlMap.keys()) {
      qCDebug(dcKostalpico()) << "Unhandled device StringControl"
                           << stringControlId;
    }

    if (!thingDescriptors.empty()) {
      emit autoThingsAppeared(thingDescriptors);
      thingDescriptors.clear();
    }

    // All devices
    updatePowerFlow(connection);
    updateInverters(connection);
    updateMeters(connection);
    updateStorages(connection);
  });
}

//TODO
//void updateTotalEnergyConsumed(KostalPicoConnection *connection);
//TODO
void IntegrationPluginKostalpico::updateCurrentPower(KostalPicoConnection *connection)
{
    Thing *parentThing = m_froniusConnections.value(connection);

    // Get power flow realtime data and update storage and pv power values according to the total values
    // The inverter details inform about the PV production after feeding the storage, but we should use the total
    // to make sure the sum is correct. Battery seems to be feeded DC to DC before the AC power convertion
    FroniusNetworkReply *powerFlowReply = connection->getPowerFlowRealtimeData();
    connect(powerFlowReply, &FroniusNetworkReply::finished, this, [=]() {
        if (powerFlowReply->networkReply()->error() != QNetworkReply::NoError) {
            return;
        }

        QByteArray data = powerFlowReply->networkReply()->readAll();

        QJsonParseError error;
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data, &error);
        if (error.error != QJsonParseError::NoError) {
            qCWarning(dcFronius()) << "Meter: Failed to parse JSON data" << data << ":" << error.errorString();
            return;
        }

        // Parse the data and update the states of our device
        QVariantMap dataMap = jsonDoc.toVariant().toMap().value("Body").toMap().value("Data").toMap();
        //qCDebug(dcFronius()) << "Power flow data" << qUtf8Printable(QJsonDocument::fromVariant(dataMap).toJson(QJsonDocument::Indented));

        // Find the inverter for this connection and set the total power
        Things availableInverters = myThings().filterByParentId(parentThing->id()).filterByThingClassId(inverterThingClassId);
        if (availableInverters.count() == 1) {
            Thing *inverterThing = availableInverters.first();
            double pvPower = dataMap.value("Site").toMap().value("P_PV").toDouble();
            inverterThing->setStateValue(inverterCurrentPowerStateTypeId, - pvPower);
        }

        // Find the storage for this connection and update the current power
        Things availableStorages = myThings().filterByParentId(parentThing->id()).filterByThingClassId(storageThingClassId);
        if (availableStorages.count() == 1) {
            Thing *storageThing = availableStorages.first();
            // Note: negative (charge), positiv (discharge)
            double akkuPower = - dataMap.value("Site").toMap().value("P_Akku").toDouble();
            storageThing->setStateValue(storageCurrentPowerStateTypeId, akkuPower);
            if (akkuPower < 0) {
                storageThing->setStateValue(storageChargingStateStateTypeId, "discharging");
            } else if (akkuPower > 0) {
                storageThing->setStateValue(storageChargingStateStateTypeId, "charging");
            } else {
                storageThing->setStateValue(storageChargingStateStateTypeId, "idle");
            }
        }

    });
}

