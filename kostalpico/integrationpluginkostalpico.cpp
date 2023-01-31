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
#include "plugintimer.h"
#include <QNetworkInterface>
#include <QXmlStreamReader>
#include <network/networkdevicediscovery.h>

IntegrationPluginKostalpico::IntegrationPluginKostalpico() {}

void IntegrationPluginKostalpico::discoverThings(ThingDiscoveryInfo *info) {
  if (info->thingClassId() == connectionThingClassId) {
    if (!hardwareManager()->networkDeviceDiscovery()->available()) {
      qCWarning(dcKostalpico())
          << "Failed to discover network devices. The network device discovery "
             "is not available.";
      info->finish(Thing::ThingErrorHardwareNotAvailable,
                   QT_TR_NOOP("The discovery is not available. Please enter "
                              "the IP address manually."));
      return;
    }

    qCDebug(dcKostalpico()) << "Starting network discovery...";
    NetworkDeviceDiscoveryReply *discoveryReply =
        hardwareManager()->networkDeviceDiscovery()->discover();
    connect(discoveryReply, &NetworkDeviceDiscoveryReply::finished,
            discoveryReply, &NetworkDeviceDiscoveryReply::deleteLater);
    connect(
        discoveryReply, &NetworkDeviceDiscoveryReply::finished, info, [=]() {
          qCDebug(dcKostalpico())
              << "Discovery finished. Found"
              << discoveryReply->networkDeviceInfos().count() << "devices";
          foreach (const NetworkDeviceInfo &networkDeviceInfo,
                   discoveryReply->networkDeviceInfos()) {
            qCDebug(dcKostalpico()) << networkDeviceInfo;

            QString title;
            if (networkDeviceInfo.hostName().isEmpty()) {
              title = networkDeviceInfo.address().toString();
            } else {
              title = networkDeviceInfo.hostName() + " (" +
                      networkDeviceInfo.address().toString() + ")";
            }

            QString description;
            if (networkDeviceInfo.macAddressManufacturer().isEmpty()) {
              description = networkDeviceInfo.macAddress();
            } else {
              description = networkDeviceInfo.macAddress() + " (" +
                            networkDeviceInfo.macAddressManufacturer() + ")";
            }

            ThingDescriptor descriptor(info->thingClassId(), title,
                                       description);
            ParamList params;
            params << Param(connectionThingMacAddressParamTypeId,
                            networkDeviceInfo.macAddress());
            descriptor.setParams(params);

            // Check if we already have set up this device
            Things existingThings =
                myThings().filterByParam(connectionThingMacAddressParamTypeId,
                                         networkDeviceInfo.macAddress());
            if (existingThings.count() == 1) {
              qCDebug(dcKostalpico())
                  << "This connection already exists in the system:"
                  << networkDeviceInfo;
              descriptor.setThingId(existingThings.first()->id());
            }

            info->addThingDescriptor(descriptor);
          }
          info->finish(Thing::ThingErrorNoError);
        });
  }
}

void IntegrationPluginKostalpico::setupThing(ThingSetupInfo *info) {
  // A thing is being set up. Use info->thing() to get details of the thing, do
  // the required setup (e.g. connect to the device) and call info->finish()
  // when done.
  Thing *thing = info->thing();
  qCDebug(dcKostalpico()) << "Setup thing" << info->thing();

  if (thing->thingClassId() == connectionThingClassId) {

    MacAddress mac = MacAddress(
        thing->paramValue(connectionThingMacAddressParamTypeId).toString());
    if (!mac.isValid()) {
      info->finish(Thing::ThingErrorInvalidParameter,
                   QT_TR_NOOP("The given MAC address is not valid."));
      return;
    }
    NetworkDeviceMonitor *monitor =
        hardwareManager()->networkDeviceDiscovery()->registerMonitor(mac);
    connect(info, &ThingSetupInfo::aborted, monitor, [monitor, this]() {
      hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(monitor);
    });

    // Handle reconfigure
    if (m_kostalConnections.values().contains(thing)) {
      KostalPicoConnection *connection = m_kostalConnections.key(thing);
      m_kostalConnections.remove(connection);
      connection->deleteLater();
    }

    // Create the connection
    KostalPicoConnection *connection =
        new KostalPicoConnection(hardwareManager()->networkManager(),
                                 monitor->networkDeviceInfo().address(), thing);

    // (possible TODO, depends if API changes with versions -> Verify the
    // version ?)
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

      // Convert the rawdata to an xml document -> see if no Error

      QXmlStreamReader *xmlDoc = new QXmlStreamReader(data);
      if (xmlDoc->hasError()) {
        qCWarning(dcKostalpico()) << "Failed to parse XML data" << data;
        info->finish(Thing::ThingErrorHardwareFailure,
                     QT_TR_NOOP("The data received from the device could not "
                                "be processed because the format is unknown."));
        return;
      }

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
    qCWarning(dcKostalpico())
        << "Connection busy. Skipping refresh cycle for host"
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

    QXmlStreamReader *xmlDoc = new QXmlStreamReader(data);
    if (xmlDoc->hasError()) {
      qCWarning(dcKostalpico())
          << "Failed to parse XML data" << data << ":" << xmlDoc->error();
      return;
    }

    if (myThings()
            .filterByParentId(connectionThing->id())
            .filterByThingClassId(kostalpicoThingClassId)
            .isEmpty()) {
      qCDebug(dcKostalpico()) << "Creating Inverter";
      QString thingDescription = connectionThing->name();
      ThingDescriptor descriptor(kostalpicoThingClassId, "Kostal Pico MP Plus",
                                 thingDescription, connectionThing->id());
      ParamList params;
      params.append(Param(kostalpicoThingIdParamTypeId, "Smartmeter"));
      descriptor.setParams(params);
      emit autoThingsAppeared(ThingDescriptors() << descriptor);
    }

    // All devices
    updateCurrentPower(connection);
    updateTotalEnergyProduced(connection);
  });
}

// Consumption
// Look for Value in <Measurement Value='XYZ' Unit='W'
// Type='GridConsumedPower'/>
void IntegrationPluginKostalpico::updateCurrentPower(
    KostalPicoConnection *connection) {
  Thing *parentThing = m_kostalConnections.value(connection);

  KostalNetworkReply *currentPower = connection->getMeasurement();
  connect(
      currentPower, &KostalNetworkReply::finished, this,
      [this, currentPower, parentThing]() {
        if (currentPower->networkReply()->error() != QNetworkReply::NoError) {
          return;
        }
        QByteArray data = currentPower->networkReply()->readAll();

        QXmlStreamReader *xmlDoc = new QXmlStreamReader(data);
        if (xmlDoc->hasError()) {
          qCWarning(dcKostalpico())
              << "Failed to parse XML data" << data << ":" << xmlDoc->error();
          return;
        }
        // TODO XML LOGIC
        QString currentPower = "";
        bool entryFound = false;
        while (!xmlDoc->atEnd() || entryFound) {
          // has nextStartElement
          if (xmlDoc->readNextStartElement()) {
            if (xmlDoc->name() == "Measurement" &&
                xmlDoc->attributes().hasAttribute("Type")) {
              if (xmlDoc->attributes().value("Type") == "GridConsumedPower") {
                // if no Value -> no measurement update
                if (xmlDoc->attributes().hasAttribute("Value")) {
                  currentPower = xmlDoc->attributes().value("Value").toString();
                } else {
                  break;
                }
              } else {
                xmlDoc->skipCurrentElement();
              }
            } else {
              xmlDoc->skipCurrentElement();
            }
          }
        }

        if (entryFound) {
          Thing *kostalPicoInverter =
              myThings()
                  .filterByParentId(parentThing->id())
                  .filterByThingClassId(kostalpicoThingClassId)
                  .first();
          if (kostalPicoInverter != nullptr) {
            kostalPicoInverter->setStateValue(kostalpicoCurrentPowerStateTypeId,
                                              currentPower);
          }
        }
      });
}
// Production
// Look for
//<Yields>
//<Yield Type='Produced' Slot='Total' Unit='Wh'>
//  <YieldValue Value='2547230' TimeStamp='2022-06-22T17:30:00'/>
//</Yield>
void IntegrationPluginKostalpico::updateTotalEnergyProduced(
    KostalPicoConnection *connection) {
  Thing *parentThing = m_kostalConnections.value(connection);

  KostalNetworkReply *totalEnergyConsumed = connection->getYields();
  connect(totalEnergyConsumed, &KostalNetworkReply::finished, this,
          [this, totalEnergyConsumed, parentThing]() {
            if (totalEnergyConsumed->networkReply()->error() !=
                QNetworkReply::NoError) {
              return;
            }
            QByteArray data = totalEnergyConsumed->networkReply()->readAll();

            QXmlStreamReader *xmlDoc = new QXmlStreamReader(data);
            if (xmlDoc->hasError()) {
              qCWarning(dcKostalpico()) << "Failed to parse XML data" << data
                                        << ":" << xmlDoc->error();
              return;
            }
            // TODO XML LOGIC
            bool entryFound = false;
            QString totalProducedEnergy = "";
            while (!xmlDoc->atEnd() || entryFound) {
              // Look for Yields
              if (xmlDoc->readNextStartElement()) {
                if (xmlDoc->name() == "Yields") {
                  // Look for Yield -> Type Produced and Slot Total
                  if (xmlDoc->name() == "Yield" &&
                      xmlDoc->attributes().hasAttribute("Type") &&
                      xmlDoc->attributes().hasAttribute("Slot")) {
                    if (xmlDoc->attributes().value("Type") == "Produced" &&
                        xmlDoc->attributes().value("Slot") == "Total") {

                      // If YieldValue has no Value -> skip this time no need to
                      // look in the rest of the xml
                      if (xmlDoc->name() == "YieldValue") {
                        if (xmlDoc->attributes().hasAttribute("Value")) {
                          totalProducedEnergy =
                              xmlDoc->attributes().value("Value").toString();
                          entryFound = true;
                          break;
                        } else {
                          break;
                        }
                      } else {
                        xmlDoc->skipCurrentElement();
                      }

                    } else {
                      xmlDoc->skipCurrentElement();
                    }

                  } else {
                    xmlDoc->skipCurrentElement();
                  }
                }
              } else {
                xmlDoc->skipCurrentElement();
              }
            }
            if (entryFound) {
              Thing *kostalPicoInverter =
                  myThings()
                      .filterByParentId(parentThing->id())
                      .filterByThingClassId(kostalpicoThingClassId)
                      .first();
              if (kostalPicoInverter != nullptr)
                kostalPicoInverter->setStateValue(
                    kostalpicoTotalEnergyProducedStateTypeId,
                    totalProducedEnergy);
            }
          });
}
