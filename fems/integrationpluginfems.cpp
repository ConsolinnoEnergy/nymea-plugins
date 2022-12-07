/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
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

#include "integrationpluginfems.h"
#include "network/networkaccessmanager.h"
#include "network/networkdevicediscovery.h"
#include "plugininfo.h"
#include "plugintimer.h"

#include <QDebug>
#include <QJsonDocument>
#include <QPointer>
#include <QUrl>
#include <QUrlQuery>
#include <math.h>

static const QString DATA_ACCESS_STRING_FEMS = "value";
static const QString SUM_STATE = "_sum/State";

static const QString ESS_SOC = "_sum/EssSoc";
static const QString ESS_ACTIVE_POWER = "_sum/EssActivePower";
static const QString ESS_ACTIVE_POWER_L1 = "_sum/EssActivePowerL1";
static const QString ESS_ACTIVE_POWER_L2 = "_sum/EssActivePowerL2";
static const QString ESS_ACTIVE_POWER_L3 = "_sum/EssActivePowerL3";

static const QString ESS_ACTIVE_CHARGE_ENERGY = "_sum/EssActiveChargeEnergy";
static const QString ESS_ACTIVE_DISCHARGE_ENERGY =
    "_sum/EssActiveDischargeEnergy";

static const QString ESS_CAPACITY = "ess0/Capacity";

static const QString GRID_ACTIVE_POWER = "_sum/GridActivePower";
static const QString GRID_ACTIVE_POWER_L1 = "_sum/GridActivePowerL1";
static const QString GRID_ACTIVE_POWER_L2 = "_sum/GridActivePowerL1";
static const QString GRID_ACTIVE_POWER_L3 = "_sum/GridActivePowerL1";

static const QString GRID_BUY_ACTIVE_ENERGY = "_sum/GridBuyActiveEnergy";
static const QString GRID_SELL_ACTIVE_ENERGY = "_sum/GridSellActiveEnergy";
static const QString GRID_PRODUCTION_ACTIVE_ENERGY =
    "_sum/ProductionActiveEnergy";
static const QString GRID_PRODUCTION_ACTIVE_AC_ENERGY =
    "_sum/ProductionAcActiveEnergy";
static const QString GRID_PRODUCTION_ACTIVE_DC_ENERGY =
    "_sum/ProductionDcActiveEnergy";
static const QString GRID_CONSUMPTION_ACTIVE_ENERGY =
    "_sum/ConsumptionActiveEnergy";

static const QString PRODCUTION_ACTIVE_POWER = "_sum/ProductionActivePower";
static const QString PRODUCTION_ACTIVE_AC_POWER =
    "_sum/ProductionAcActivePower";
static const QString PRODUCTION_ACTIVE_DC_POWER =
    "_sum/ProductionDcActualPower";
static const QString PRODUCTION_ACTIVE_AC_POWER_L1 =
    "_sum/ProductionAcActivePowerL1";
static const QString PRODUCTION_ACTIVE_AC_POWER_L2 =
    "_sum/ProductionAcActivePowerL2";
static const QString PRODUCTION_ACTIVE_AC_POWER_L3 =
    "_sum/ProductionAcActivePowerL3";

static const QString CONSUMPTION_ACTIVE_POWER = "_sum/ConsumptionActivePower";
static const QString CONSUMPTION_ACTIVE_AC_POWER_L1 =
    "_sum/ConsumptionActivePowerL1";
static const QString CONSUMPTION_ACTIVE_AC_POWER_L2 =
    "_sum/ConsumptionActivePowerL2";
static const QString CONSUMPTION_ACTIVE_AC_POWER_L3 =
    "_sum/ConsumptionActivePowerL3";

// EITHER  USE METER 0, 1 or 2 depending on MeterType

static const QString CURRENT_PHASE_1 = "CurrentL1";
static const QString CURRENT_PHASE_2 = "CurrentL2";
static const QString CURRENT_PHASE_3 = "CurrentL3";
static const QString FREQUENCY = "Frequency";

static const QString METER_0 = "meter0";
static const QString METER_1 = "meter1";
static const QString METER_2 = "meter2";
static const QString SKIP = "Skipping_No_Meter_Found";

IntegrationPluginFems::IntegrationPluginFems(QObject *parent)
    : IntegrationPlugin(parent) {}

void IntegrationPluginFems::init() {
  // Initialisation can be done here.
  meter = METER_0;
  batteryState = "idle";
  qCDebug(dcFems()) << "Plugin initialized.";
}

void IntegrationPluginFems::setupThing(ThingSetupInfo *info) {

  Thing *thing = info->thing();
  qCDebug(dcFems()) << "Setting up" << thing;

  if (thing->thingClassId() == connectionThingClassId) {

    QHostAddress address(
        thing->paramValue(connectionThingAddressParamTypeId).toString());

    // Handle reconfigure
    if (m_femsConnections.values().contains(thing)) {
      FemsConnection *connection = m_femsConnections.key(thing);
      m_femsConnections.remove(connection);
      connection->deleteLater();
    }

    // Create the connection
    FemsConnection *connection = new FemsConnection(
        hardwareManager()->networkManager(), address, thing,
        thing->paramValue(connectionThingUserParamTypeId).toString(),
        thing->paramValue(connectionThingPasswordParamTypeId).toString(),
        thing->paramValue(connectionThingEdgeParamTypeId).toBool(),
        thing->paramValue(connectionThingPortParamTypeId).toString());
    FemsNetworkReply *reply = connection->isAvailable();
    connect(reply, &FemsNetworkReply::finished, info, [=] {
      QByteArray data = reply->networkReply()->readAll();
      if (reply->networkReply()->error() != QNetworkReply::NoError) {
        // no URL bc URL contains uname and pwd
        // qcWarning(dcFems() << "Network request error:") <<
        // reply->networkReply()->error() <<
        // reply->networkReply()->errorString();
        if (reply->networkReply()->error() ==
            QNetworkReply::ContentNotFoundError) {
          info->finish(
              Thing::ThingErrorHardwareNotAvailable,
              QT_TR_NOOP("The device does not reply to our requests. Please "
                         "verify that the FEMS API is enabled on the device."));
        } else {
          info->finish(Thing::ThingErrorHardwareNotAvailable,
                       QT_TR_NOOP("The device is not reachable."));
        }
        return;
      }

      // Convert the rawdata to a JSON document
      QJsonParseError error;
      QJsonDocument jsonDoc = QJsonDocument::fromJson(data, &error);

      if (error.error != QJsonParseError::NoError) {
        qCWarning(dcFems()) << "Failed to parse JSON data" << data << ":"
                            << error.errorString() << data;
        info->finish(Thing::ThingErrorHardwareFailure,
                     QT_TR_NOOP("The data received from the device could not "
                                "be processed because the format is unknown."));
        reply->deleteLater();
        connection->deleteLater();
        return;
      }

      QVariantMap responseMap = jsonDoc.toVariant().toMap();
      qint8 status = responseMap.value("value", -1).toInt();
      if (status >= 0) {
        // 0 == ok, 1 == info, 2 == Warning, 3 == fault (todo change to strings)
        thing->setStateValue(connectionStatusStateTypeId, status);
      }
      qCDebug(dcFems()) << "Fems Status"
                        << responseMap.value("value").toString();
      // STATE Connection
      m_femsConnections.insert(connection, thing);
      info->finish(Thing::ThingErrorNoError);
      thing->setStateValue("connected", true);
      reply->deleteLater();
      connection->deleteLater();
    });
  } else if ((thing->thingClassId() == meterThingClassId) ||
             (thing->thingClassId() == batteryThingClassId)) {
    Thing *parentThing = myThings().findById(thing->parentId());
    if (!parentThing) {
      qCWarning(dcFems()) << "Could not find the parent for" << thing;
      info->finish(Thing::ThingErrorHardwareNotAvailable);
      parentThing->deleteLater();
      return;
    }
    FemsConnection *connection = m_femsConnections.key(parentThing);
    if (!connection) {
      qCWarning(dcFems()) << "Could not find the parent connection for"
                          << thing;
      info->finish(Thing::ThingErrorHardwareNotAvailable);
      connection->deleteLater();
      parentThing->deleteLater();
      thing->deleteLater();
      return;
    }
    info->finish(Thing::ThingErrorNoError);
    connection->deleteLater();
    parentThing->deleteLater();
    thing->deleteLater();
  } else {
    Q_ASSERT_X(false, "setupThing",
               QString("Unhandled thingClassId: %1")
                   .arg(thing->thingClassId().toString())
                   .toUtf8());
  }
  thing->deleteLater();
}

void IntegrationPluginFems::postSetupThing(Thing *thing) {

  qCDebug(dcFems()) << "Post setup" << thing->name();

  if (thing->thingClassId() == connectionThingClassId) {
    if (!m_connectionRefreshTimer) {
      hardwareManager()->pluginTimerManager()->registerTimer(10);
      connect(m_connectionRefreshTimer, &PluginTimer::timeout, this, [this]() {
        foreach (FemsConnection *connection, m_femsConnections.keys()) {
          refreshConnection(connection);
        }
      });
      m_connectionRefreshTimer->start();
    }

    FemsConnection *connection = m_femsConnections.key(thing);
    if (connection) {
      refreshConnection(connection);
    }
    connection->deleteLater();
  }
}

void IntegrationPluginFems::thingRemoved(Thing *thing) {
  if (thing->thingClassId() == connectionThingClassId) {
    FemsConnection *connection = m_femsConnections.key(thing);
    m_femsConnections.remove(connection);
    connection->deleteLater();
  }

  if (myThings().filterByThingClassId(connectionThingClassId).isEmpty()) {
    hardwareManager()->pluginTimerManager()->unregisterTimer(
        m_connectionRefreshTimer);
    m_connectionRefreshTimer = nullptr;
  }
}

void IntegrationPluginFems::executeAction(ThingActionInfo *info) {
  Q_UNUSED(info)
}

// Poll data again
void IntegrationPluginFems::refreshConnection(FemsConnection *connection) {
  if (connection->busy()) {
    qCWarning(dcFems()) << "Connection busy. Skipping refresh cycle for host"
                        << connection->address().toString();
    return;
  }
  // 3 things, parent, and battery as well as meter but only parent connection
  // nec. GET Meter and Battery ID Later and update param info
  Thing *connectionThing = m_femsConnections.value(connection);
  if (!connectionThing)
    return;
  this->updateSumState(connection);
  this->updateMeters(connection);
  this->updateStorages(connection);
}

void IntegrationPluginFems::updateStorages(FemsConnection *connection) {

  Thing *parentThing = m_femsConnections.value(connection);
  // Get everything from the sum that is ESS related.
  // most states are available at the "sum"
  // Since FEMS/OpenEMS can have mutltiple ess it can be risky to get multiple
  // ess but normally only 1 ess (ess0) ist just start and try ess0 -> if not
  // available -> SKIP However sum should supply enough data

  // ChargingState -> this should be done seperately when comparing charing and
  // discharging energy idle, charging, discharging

  // ChargingEnergy(Nymea)
  // EssActiveChargeEnergy(OpenEMS)
  FemsNetworkReply *essActiveChargeEnergy =
      connection->getFemsDataPoint(ESS_ACTIVE_CHARGE_ENERGY);

  connect(essActiveChargeEnergy, &FemsNetworkReply::finished, this, [=]() {
    if (this->connectionError(essActiveChargeEnergy)) {
      essActiveChargeEnergy->deleteLater();
      return;
    }
    QByteArray data = essActiveChargeEnergy->networkReply()->readAll();
    QJsonParseError error;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(data, &error);

    // Check JSON Reply
    bool jsonE = this->jsonError(data);
    if (jsonE) {
      qCWarning(dcFems()) << "Meter: Failed to parse JSON data" << data << ":"
                          << error.errorString();
      essActiveChargeEnergy->deleteLater();
      return;
    }
    QVariant *var = new QVariant((this->getValueOfRequestedData(&jsonDoc)));
    // GET "value" of data
    this->addValueToThing(parentThing, batteryThingClassId,
                          batteryChargingEnergyStateTypeId, var, DOUBLE, 0);
    var = NULL;
    delete var;
    essActiveChargeEnergy->deleteLater();
    this->checkBatteryState(parentThing);
  });

  // DischargingEnergy(Nymea)
  // EssActiveDischargeEnergy(OpenEMS)
  FemsNetworkReply *essActiveDischargeEnergy =
      connection->getFemsDataPoint(ESS_ACTIVE_DISCHARGE_ENERGY);

  connect(essActiveDischargeEnergy, &FemsNetworkReply::finished, this, [=]() {
    if (this->connectionError(essActiveDischargeEnergy)) {
      essActiveDischargeEnergy->deleteLater();
      return;
    }
    QByteArray data = essActiveDischargeEnergy->networkReply()->readAll();
    QJsonParseError error;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(data, &error);

    // Check JSON Reply
    bool jsonE = this->jsonError(data);
    if (jsonE) {
      qCWarning(dcFems()) << "Meter: Failed to parse JSON data" << data << ":"
                          << error.errorString();
      essActiveDischargeEnergy->deleteLater();
      return;
    }
    // GET "value" of data
    QVariant *var = new QVariant((this->getValueOfRequestedData(&jsonDoc)));
    this->addValueToThing(parentThing, batteryThingClassId,
                          batteryDischarginEnergyStateTypeId, var, DOUBLE, 0);
    var = NULL;
    delete var;
    essActiveDischargeEnergy->deleteLater();
    this->checkBatteryState(parentThing);
  });

  // CurrentPower(Nymea)
  // EssActivePower(OpenEMS)

  FemsNetworkReply *essActivePower =
      connection->getFemsDataPoint(ESS_ACTIVE_POWER);

  connect(essActivePower, &FemsNetworkReply::finished, this, [=]() {
    if (this->connectionError(essActivePower)) {
      essActivePower->deleteLater();
      return;
    }
    QByteArray data = essActivePower->networkReply()->readAll();
    QJsonParseError error;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(data, &error);

    // Check JSON Reply
    bool jsonE = this->jsonError(data);
    if (jsonE) {
      qCWarning(dcFems()) << "Meter: Failed to parse JSON data" << data << ":"
                          << error.errorString();
      essActivePower->deleteLater();
      return;
    }
    QVariant *var = new QVariant((this->getValueOfRequestedData(&jsonDoc)));
    // GET "value" of data
    this->addValueToThing(parentThing, batteryThingClassId,
                          batteryCurrentPowerStateTypeId, var, DOUBLE, 0);
    var = NULL;
    delete var;
    essActivePower->deleteLater();
  });

  // CurrentPowerA
  // EssActivePowerL1

  FemsNetworkReply *essActivePowerL1 =
      connection->getFemsDataPoint(ESS_ACTIVE_POWER_L1);

  connect(essActivePowerL1, &FemsNetworkReply::finished, this, [=]() {
    if (this->connectionError(essActivePowerL1)) {
     essActivePowerL1-> deleteLater();
      return;
    }
    QByteArray data = essActivePowerL1->networkReply()->readAll();
    QJsonParseError error;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(data, &error);

    // Check JSON Reply
    bool jsonE = this->jsonError(data);
    if (jsonE) {
      qCWarning(dcFems()) << "Meter: Failed to parse JSON data" << data << ":"
                          << error.errorString();
      essActivePowerL1->deleteLater();
      return;
    }
    QVariant *var = new QVariant((this->getValueOfRequestedData(&jsonDoc)));
    // GET "value" of data
    this->addValueToThing(parentThing, batteryThingClassId,
                          batteryCurrentPowerAStateTypeId, var, DOUBLE, 0);
    var = NULL;
    delete var;
    essActivePowerL1->deleteLater();
  });

  // CurrentPowerB
  // EssActivePowerL2

  FemsNetworkReply *essActivePowerL2 =
      connection->getFemsDataPoint(ESS_ACTIVE_POWER_L2);

  connect(essActivePowerL2, &FemsNetworkReply::finished, this, [=]() {
    if (this->connectionError(essActivePowerL2)) {
      essActivePowerL2->deleteLater();
      return;
    }
    QByteArray data = essActivePowerL2->networkReply()->readAll();
    QJsonParseError error;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(data, &error);

    // Check JSON Reply
    bool jsonE = this->jsonError(data);
    if (jsonE) {
      qCWarning(dcFems()) << "Meter: Failed to parse JSON data" << data << ":"
                          << error.errorString();
      essActivePowerL2->deleteLater();
      return;
    }
    QVariant *var = new QVariant((this->getValueOfRequestedData(&jsonDoc)));
    // GET "value" of data
    this->addValueToThing(parentThing, batteryThingClassId,
                          batteryCurrentPowerBStateTypeId, var, DOUBLE, 0);
    var = NULL;
    delete var;
    essActivePowerL2->deleteLater();
  });

  // CurrentPowerC
  // EssActivePowerL3

  FemsNetworkReply *essActivePowerL3 =
      connection->getFemsDataPoint(ESS_ACTIVE_POWER_L3);

  connect(essActivePowerL3, &FemsNetworkReply::finished, this, [=]() {
    if (this->connectionError(essActivePowerL3)) {
     essActivePowerL3-> deleteLater();
      return;
    }
    QByteArray data = essActivePowerL3->networkReply()->readAll();
    QJsonParseError error;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(data, &error);

    // Check JSON Reply
    bool jsonE = this->jsonError(data);
    if (jsonE) {
      qCWarning(dcFems()) << "Meter: Failed to parse JSON data" << data << ":"
                          << error.errorString();
      essActivePowerL3->deleteLater();
      return;
    }
    QVariant *var = new QVariant((this->getValueOfRequestedData(&jsonDoc)));
    // GET "value" of data
    this->addValueToThing(parentThing, batteryThingClassId,
                          batteryCurrentPowerCStateTypeId, var, DOUBLE, 0);
    var = NULL;
    delete var;
    essActivePowerL3->deleteLater();
  });

  // Capacity
  // Try ess0/Capacity
  FemsNetworkReply *essCapacity = connection->getFemsDataPoint(ESS_CAPACITY);

  connect(essCapacity, &FemsNetworkReply::finished, this, [=]() {
    if (this->connectionError(essCapacity)) {
      essCapacity->deleteLater();
      return;
    }
    QByteArray data = essCapacity->networkReply()->readAll();
    QJsonParseError error;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(data, &error);

    // Check JSON Reply
    bool jsonE = this->jsonError(data);
    if (jsonE) {
      qCWarning(dcFems()) << "Meter: Failed to parse JSON data" << data << ":"
                          << error.errorString();
      essCapacity->deleteLater();
      return;
    }
    QVariant *var = new QVariant((this->getValueOfRequestedData(&jsonDoc)));
    // GET "value" of data
    this->addValueToThing(parentThing, batteryThingClassId,
                          batteryCapacityStateTypeId, var, DOUBLE, 0);
    var = NULL;
    delete var;
    essCapacity->deleteLater();
  });

  // BatteryLevel (SoC)
  // EssSoc
  FemsNetworkReply *essBatteryLevel = connection->getFemsDataPoint(ESS_SOC);

  connect(essCapacity, &FemsNetworkReply::finished, this, [=]() {
    if (this->connectionError(essBatteryLevel)) {
      essCapacity->deleteLater();
      return;
    }
    QByteArray data = essBatteryLevel->networkReply()->readAll();
    QJsonParseError error;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(data, &error);

    // Check JSON Reply
    bool jsonE = this->jsonError(data);
    if (jsonE) {
      qCWarning(dcFems()) << "Meter: Failed to parse JSON data" << data << ":"
                          << error.errorString();
      essCapacity->deleteLater();
      return;
    }
    QVariant *var = new QVariant((this->getValueOfRequestedData(&jsonDoc)));
    // GET "value" of data
    this->addValueToThing(parentThing, batteryThingClassId,
                          batteryBatteryLevelStateTypeId, var, DOUBLE, 0);
    var = NULL;
    delete var;
    essCapacity->deleteLater();
    // TODO StateOfHealt
    this->calculateStateOfHealth(parentThing);
  });

  // CellTemperature <- mostly not available
  // try but ....ye ess0/Temperature
  FemsNetworkReply *essTemperature =
      connection->getFemsDataPoint("ess0/Temperature");

  connect(essCapacity, &FemsNetworkReply::finished, this, [=]() {
    if (this->connectionError(essTemperature)) {
      essCapacity->deleteLater();
      return;
    }
    QByteArray data = essTemperature->networkReply()->readAll();
    QJsonParseError error;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(data, &error);

    // Check JSON Reply
    bool jsonE = this->jsonError(data);
    if (jsonE) {
      qCWarning(dcFems()) << "Meter: Failed to parse JSON data" << data << ":"
                          << error.errorString();
      essCapacity->deleteLater();
      return;
    }
    QVariant *var = new QVariant((this->getValueOfRequestedData(&jsonDoc)));
    // GET "value" of data
    this->addValueToThing(parentThing, batteryThingClassId,
                          batteryCellTemperatureStateTypeId, var, DOUBLE, 0);
    var = NULL;
    delete var;
    essCapacity->deleteLater();
  });
}

void IntegrationPluginFems::updateSumState(FemsConnection *connection) {
  Thing *parentThing = m_femsConnections.value(connection);
  // Get everything from the sum that is coherend to states.
  // e.g. Fems State of SUM, then if not at fault -> set Battery and Meter
  // states.

  // ChargingEnergy(Nymea)
  // EssActiveChargeEnergy(OpenEMS)
  FemsNetworkReply *sumState = connection->getFemsDataPoint(SUM_STATE);

  connect(sumState, &FemsNetworkReply::finished, this, [=]() {
    if (this->connectionError(sumState)) {
      sumState->deleteLater();
      return;
    }
    QByteArray data = sumState->networkReply()->readAll();
    QJsonParseError error;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(data, &error);

    // Check JSON Reply
    bool jsonE = this->jsonError(data);
    if (jsonE) {
      qCWarning(dcFems()) << "Meter: Failed to parse JSON data" << data << ":"
                          << error.errorString();
      sumState->deleteLater();
      return;
    }
    QVariant *var = new QVariant((this->getValueOfRequestedData(&jsonDoc)));
    // GET "value" of data
    this->addValueToThing(parentThing, connectionThingClassId,
                          connectionStatusStateTypeId, var, MY_INT, 0);

    if (var != nullptr) {
      QVariant *varBool = new QVariant(true);
      // FEMS STATE == FAULT on 3
      if (var->toInt() == 3) {
        varBool->setValue(false);
      }
      this->addValueToThing(parentThing, meterThingClassId,
                            meterConnectedStateTypeId, varBool, MY_BOOLEAN, 0);
      this->addValueToThing(parentThing, batteryThingClassId,
                            batteryConnectedStateTypeId, varBool, MY_BOOLEAN,
                            0);
      varBool = NULL;
      delete varBool;
    }
    var = NULL;
    delete var;
  });
  parentThing->deleteLater();
}

void IntegrationPluginFems::updateMeters(FemsConnection *connection) {

  Thing *parentThing = m_femsConnections.value(connection);
  // Get everything from the sum that is meter related.
  // most states are available at the "sum"
  // Since FEMS/OpenEMS can have mutltiple Meters it can be risky to get
  // multiple meters start with meter0 -> if of type AsymmetricMeter it will
  // have values at L1-L3 phase calls If Symmetric it won't You can try with
  // meter0 and meter1 (should be sufficent) However sum should supply enough
  // data

  // GridActivePower
  FemsNetworkReply *currentGridPowerReply =
      connection->getFemsDataPoint(GRID_ACTIVE_POWER);

  connect(currentGridPowerReply, &FemsNetworkReply::finished, this, [=]() {
    if (this->connectionError(currentGridPowerReply)) {
      currentGridPowerReply->deleteLater();
      return;
    }
    QByteArray data = currentGridPowerReply->networkReply()->readAll();
    QJsonParseError error;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(data, &error);

    // Check JSON Reply
    bool jsonE = this->jsonError(data);
    if (jsonE) {
      qCWarning(dcFems()) << "Meter: Failed to parse JSON data" << data << ":"
                          << error.errorString();
      currentGridPowerReply->deleteLater();
      return;
    }
    QVariant *var = new QVariant((this->getValueOfRequestedData(&jsonDoc)));
    this->addValueToThing(parentThing, meterThingClassId,
                          meterCurrentGridPowerStateTypeId, var, DOUBLE, 0);
    var = NULL;
    delete var;
    currentGridPowerReply->deleteLater();
  });

  // ProductionActivePower
  FemsNetworkReply *currentPowerProduction =
      connection->getFemsDataPoint(PRODCUTION_ACTIVE_POWER);
  connect(currentPowerProduction, &FemsNetworkReply::finished, this, [=]() {
    if (this->connectionError(currentPowerProduction)) {
      currentPowerProduction->deleteLater();
      return;
    }
    QByteArray data = currentPowerProduction->networkReply()->readAll();
    QJsonParseError error;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(data, &error);

    // Check JSON Reply
    bool jsonE = this->jsonError(data);
    if (jsonE) {
      qCWarning(dcFems()) << "Meter: Failed to parse JSON data" << data << ":"
                          << error.errorString();
      currentPowerProduction->deleteLater();
      return;
    }

    QVariant *var = new QVariant((this->getValueOfRequestedData(&jsonDoc)));
    this->addValueToThing(parentThing, meterThingClassId,
                          meterCurrentPowerProductionStateTypeId, var, DOUBLE,
                          0);
    var = NULL;
    delete var;
    currentGridPowerReply->deleteLater();
  });

  // ProductionAcActivePower
  FemsNetworkReply *currentPowerProductionAc =
      connection->getFemsDataPoint(PRODUCTION_ACTIVE_AC_POWER);
  connect(currentPowerProductionAc, &FemsNetworkReply::finished, this, [=]() {
    if (this->connectionError(currentPowerProductionAc)) {
      currentPowerProductionAc->deleteLater();
      return;
    }
    QByteArray data = currentPowerProductionAc->networkReply()->readAll();
    QJsonParseError error;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(data, &error);

    // Check JSON Reply
    bool jsonE = this->jsonError(data);
    if (jsonE) {
      qCWarning(dcFems()) << "Meter: Failed to parse JSON data" << data << ":"
                          << error.errorString();
      currentPowerProductionAc->deleteLater();
      return;
    }

    QVariant *var = new QVariant((this->getValueOfRequestedData(&jsonDoc)));
    this->addValueToThing(parentThing, meterThingClassId,
                          meterCurrentPowerProductionAcStateTypeId, var, DOUBLE,
                          0);
    var = NULL;
    delete var;
    currentPowerProductionAc->deleteLater();
  });

  // ProductionDcActivePower
  FemsNetworkReply *currentPowerProductionDc =
      connection->getFemsDataPoint(PRODUCTION_ACTIVE_DC_POWER);
  connect(currentPowerProductionDc, &FemsNetworkReply::finished, this, [=]() {
    if (this->connectionError(currentPowerProductionDc)) {
      currentPowerProductionDc->deleteLater();
      return;
    }
    QByteArray data = currentPowerProductionDc->networkReply()->readAll();
    QJsonParseError error;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(data, &error);

    // Check JSON Reply
    bool jsonE = this->jsonError(data);
    if (jsonE) {
      qCWarning(dcFems()) << "Meter: Failed to parse JSON data" << data << ":"
                          << error.errorString();
      currentPowerProductionDc->deleteLater();
      return;
    }
    QVariant *var = new QVariant((this->getValueOfRequestedData(&jsonDoc)));
    this->addValueToThing(parentThing, meterThingClassId,
                          meterCurrentPowerProductionDcStateTypeId, var, DOUBLE,
                          0);
    var = NULL;
    delete var;
    currentPowerProductionAc->deleteLater();
  });
  // CurrentPower
  FemsNetworkReply *currentPower =
      connection->getFemsDataPoint(CONSUMPTION_ACTIVE_POWER);
  connect(currentPower, &FemsNetworkReply::finished, this, [=]() {
    if (this->connectionError(currentPower)) {
      currentPower->deleteLater();
      return;
    }
    QByteArray data = currentPower->networkReply()->readAll();
    QJsonParseError error;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(data, &error);

    // Check JSON Reply
    bool jsonE = this->jsonError(data);
    if (jsonE) {
      qCWarning(dcFems()) << "Meter: Failed to parse JSON data" << data << ":"
                          << error.errorString();
      currentPower->deleteLater();
      return;
    }
    QVariant *var = new QVariant((this->getValueOfRequestedData(&jsonDoc)));
    this->addValueToThing(parentThing, meterThingClassId,
                          meterCurrentPowerStateTypeId, var, DOUBLE, 0);
    var = NULL;
    delete var;
    currentPower->deleteLater();
  });
  // ProductionActiveEnergy
  FemsNetworkReply *totalEnergyProduced =
      connection->getFemsDataPoint(GRID_PRODUCTION_ACTIVE_ENERGY);
  connect(totalEnergyProduced, &FemsNetworkReply::finished, this, [=]() {
    if (this->connectionError(totalEnergyProduced)) {
      totalEnergyProduced->deleteLater();
      return;
    }
    QByteArray data = totalEnergyProduced->networkReply()->readAll();
    QJsonParseError error;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(data, &error);

    bool jsonE = this->jsonError(data);
    if (jsonE) {
      qCWarning(dcFems()) << "Meter: Failed to parse JSON data" << data << ":"
                          << error.errorString();
      totalEnergyProduced->deleteLater();
      return;
    }

    QVariant *var = new QVariant((this->getValueOfRequestedData(&jsonDoc)));
    this->addValueToThing(parentThing, meterThingClassId,
                          meterTotalEnergyProducedStateTypeId, var, DOUBLE, -3);
    var = NULL;
    delete var;
    totalEnergyProduced->deleteLater();
  });
  // ConsumptionActiveEnergy
  FemsNetworkReply *totalEnergyConsumed =
      connection->getFemsDataPoint(GRID_CONSUMPTION_ACTIVE_ENERGY);
  connect(totalEnergyConsumed, &FemsNetworkReply::finished, this, [=]() {
    if (this->connectionError(totalEnergyConsumed)) {
      totalEnergyConsumed->deleteLater();
      return;
    }
    QByteArray data = totalEnergyConsumed->networkReply()->readAll();
    QJsonParseError error;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(data, &error);

    // Check JSON Reply
    bool jsonE = this->jsonError(data);
    if (jsonE) {
      qCWarning(dcFems()) << "Meter: Failed to parse JSON data" << data << ":"
                          << error.errorString();
      totalEnergyConsumed->deleteLater();
      return;
    }

    QVariant *var = new QVariant((this->getValueOfRequestedData(&jsonDoc)));
    this->addValueToThing(parentThing, meterThingClassId,
                          meterTotalEnergyConsumedStateTypeId, var, DOUBLE, -3);
    var = NULL;
    delete var;
    totalEnergyConsumed->deleteLater();
  });
  // Grid BUY
  FemsNetworkReply *currentGridBuyEnergy =
      connection->getFemsDataPoint(GRID_BUY_ACTIVE_ENERGY);
  connect(currentGridBuyEnergy, &FemsNetworkReply::finished, this, [=]() {
    if (this->connectionError(currentGridBuyEnergy)) {
      currentGridBuyEnergy->deleteLater();
      return;
    }
    QByteArray data = currentGridBuyEnergy->networkReply()->readAll();
    QJsonParseError error;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(data, &error);

    // Check JSON Reply
    bool jsonE = this->jsonError(data);
    if (jsonE) {
      qCWarning(dcFems()) << "Meter: Failed to parse JSON data" << data << ":"
                          << error.errorString();
      currentGridBuyEnergy->deleteLater();
      return;
    }

    QVariant *var = new QVariant((this->getValueOfRequestedData(&jsonDoc)));
    this->addValueToThing(parentThing, meterThingClassId,
                          meterCurrentGridBuyEnergyStateTypeId, var, DOUBLE,
                          -3);
    var = NULL;
    delete var;
    currentGridBuyEnergy->deleteLater();
  });
  // Grid SELL
  FemsNetworkReply *currentGridSellEnergy =
      connection->getFemsDataPoint(GRID_SELL_ACTIVE_ENERGY);
  connect(currentGridSellEnergy, &FemsNetworkReply::finished, this, [=]() {
    if (this->connectionError(currentGridSellEnergy)) {
      currentGridSellEnergy->deleteLater();
      return;
    }
    QByteArray data = currentGridSellEnergy->networkReply()->readAll();
    QJsonParseError error;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(data, &error);

    // Check JSON Reply
    bool jsonE = this->jsonError(data);
    if (jsonE) {
      qCWarning(dcFems()) << "Meter: Failed to parse JSON data" << data << ":"
                          << error.errorString();
      currentGridSellEnergy->deleteLater();
      return;
    }

    QVariant *var = new QVariant((this->getValueOfRequestedData(&jsonDoc)));
    this->addValueToThing(parentThing, meterThingClassId,
                          meterCurrentGridSellEnergyStateTypeId, var, DOUBLE,
                          -3);
    var = NULL;
    delete var;
    currentGridSellEnergy->deleteLater();
  });

  // GridActivePowerL1
  FemsNetworkReply *currentPowerPhaseA =
      connection->getFemsDataPoint(GRID_ACTIVE_POWER_L1);
  connect(currentPowerPhaseA, &FemsNetworkReply::finished, this, [=]() {
    if (this->connectionError(currentPowerPhaseA)) {
      currentPowerPhaseA->deleteLater();
      return;
    }
    QByteArray data = currentPowerPhaseA->networkReply()->readAll();
    QJsonParseError error;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(data, &error);

    // Check JSON Reply
    bool jsonE = this->jsonError(data);
    if (jsonE) {
      qCWarning(dcFems()) << "Meter: Failed to parse JSON data" << data << ":"
                          << error.errorString();
      currentPowerPhaseA->deleteLater();
      return;
    }

    QVariant *var = new QVariant((this->getValueOfRequestedData(&jsonDoc)));
    this->addValueToThing(parentThing, meterThingClassId,
                          meterCurrentPowerPhaseAStateTypeId, var, DOUBLE, -3);
    var = NULL;
    delete var;
    currentPowerPhaseA->deleteLater();
  });

  // GridActivePowerL2
  FemsNetworkReply *currentPowerPhaseB =
      connection->getFemsDataPoint(GRID_ACTIVE_POWER_L2);
  connect(currentPowerPhaseB, &FemsNetworkReply::finished, this, [=]() {
    if (this->connectionError(currentPowerPhaseB)) {
      currentPowerPhaseB->deleteLater();
      return;
    }
    QByteArray data = currentPowerPhaseB->networkReply()->readAll();
    QJsonParseError error;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(data, &error);

    // Check JSON Reply
    bool jsonE = this->jsonError(data);
    if (jsonE) {
      qCWarning(dcFems()) << "Meter: Failed to parse JSON data" << data << ":"
                          << error.errorString();
      currentPowerPhaseB->deleteLater();
      return;
    }

    QVariant *var = new QVariant((this->getValueOfRequestedData(&jsonDoc)));
    this->addValueToThing(parentThing, meterThingClassId,
                          meterCurrentPowerPhaseBStateTypeId, var, DOUBLE, -3);
    var = NULL;
    delete var;
    currentPowerPhaseB->deleteLater();
  });

  // GridActivePowerL3
  FemsNetworkReply *currentPowerPhaseC =
      connection->getFemsDataPoint(GRID_ACTIVE_POWER_L3);
  connect(currentPowerPhaseC, &FemsNetworkReply::finished, this, [=]() {
    if (this->connectionError(currentPowerPhaseC)) {
      currentPowerPhaseC->deleteLater();
      return;
    }
    QByteArray data = currentPowerPhaseC->networkReply()->readAll();
    QJsonParseError error;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(data, &error);

    // Check JSON Reply
    bool jsonE = this->jsonError(data);
    if (jsonE) {
      qCWarning(dcFems()) << "Meter: Failed to parse JSON data" << data << ":"
                          << error.errorString();
      currentPowerPhaseC->deleteLater();
      return;
    }

    QVariant *var = new QVariant((this->getValueOfRequestedData(&jsonDoc)));
    this->addValueToThing(parentThing, meterThingClassId,
                          meterCurrentPowerPhaseCStateTypeId, var, DOUBLE, -3);
    var = NULL;
    delete var;
    currentPowerPhaseC->deleteLater();
  });

  // HERE TEST CONNECTION! if Meter asymmetric -> check meter0 first -> if
  // normally connection available -> test meter0 if no conn. test meter1 up to
  // meter2 else -> just skip
  bool shouldNotSkip = !(this->meter == SKIP);
  if (shouldNotSkip) {
    QString PhaseA = this->meter + "/" + CURRENT_PHASE_1;
    QString PhaseB = this->meter + "/" + CURRENT_PHASE_2;
    QString PhaseC = this->meter + "/" + CURRENT_PHASE_3;
    QString Frequency = this->meter + "/" + FREQUENCY;
    // CurrentL1
    FemsNetworkReply *currentPhaseA = connection->getFemsDataPoint(PhaseA);
    connect(currentPhaseA, &FemsNetworkReply::finished, this, [=]() {
      if (this->connectionError(currentPhaseA)) {
        this->changeMeterString();
        currentPhaseA->deleteLater();
        return;
      }
      QByteArray data = currentPhaseA->networkReply()->readAll();
      QJsonParseError error;
      QJsonDocument jsonDoc = QJsonDocument::fromJson(data, &error);

      // Check JSON Reply
      bool jsonE = this->jsonError(data);
      if (jsonE) {
        qCWarning(dcFems()) << "Meter: Failed to parse JSON data" << data << ":"
                            << error.errorString();
        this->changeMeterString();
        currentPhaseA->deleteLater();
        return;
      }
      QVariant *var = new QVariant((this->getValueOfRequestedData(&jsonDoc)));
      this->addValueToThing(parentThing, meterThingClassId,
                            meterCurrentPhaseAStateTypeId, var, DOUBLE, -3);
      var = NULL;
      delete var;
      currentPhaseA->deleteLater();
    });
    // Current L2
    FemsNetworkReply *currentPhaseB = connection->getFemsDataPoint(PhaseB);
    connect(currentPhaseB, &FemsNetworkReply::finished, this, [=]() {
      if (this->connectionError(currentPhaseB)) {
        this->changeMeterString();
        currentPhaseB->deleteLater();
        return;
      }
      QByteArray data = currentPhaseB->networkReply()->readAll();
      QJsonParseError error;
      QJsonDocument jsonDoc = QJsonDocument::fromJson(data, &error);

      // Check JSON Reply
      bool jsonE = this->jsonError(data);
      if (jsonE) {
        qCWarning(dcFems()) << "Meter: Failed to parse JSON data" << data << ":"
                            << error.errorString();
        this->changeMeterString();
        currentPhaseB->deleteLater();
        return;
      }

      QVariant *var = new QVariant((this->getValueOfRequestedData(&jsonDoc)));
      this->addValueToThing(parentThing, meterThingClassId,
                            meterCurrentPhaseBStateTypeId, var, DOUBLE, -3);
      var = NULL;
      delete var;
      currentPhaseB->deleteLater();
    });
    // Current L3
    FemsNetworkReply *currentPhaseC = connection->getFemsDataPoint(PhaseC);
    connect(currentPhaseC, &FemsNetworkReply::finished, this, [=]() {
      if (this->connectionError(currentPhaseC)) {
        this->changeMeterString();
        currentPhaseC->deleteLater();
        return;
      }
      QByteArray data = currentPhaseC->networkReply()->readAll();
      QJsonParseError error;
      QJsonDocument jsonDoc = QJsonDocument::fromJson(data, &error);

      // Check JSON Reply
      bool jsonE = this->jsonError(data);
      if (jsonE) {
        qCWarning(dcFems()) << "Meter: Failed to parse JSON data" << data << ":"
                            << error.errorString();
        this->changeMeterString();
        currentPhaseC->deleteLater();
        return;
      }
      QVariant *var = new QVariant((this->getValueOfRequestedData(&jsonDoc)));
      this->addValueToThing(parentThing, meterThingClassId,
                            meterCurrentPhaseCStateTypeId, var, DOUBLE, -3);
      var = NULL;
      delete var;
      currentPhaseC->deleteLater();
    });

    // Frequency
    FemsNetworkReply *frequency = connection->getFemsDataPoint(Frequency);
    connect(frequency, &FemsNetworkReply::finished, this, [=]() {
      if (this->connectionError(frequency)) {
        this->changeMeterString();
        frequency->deleteLater();
        return;
      }
      QByteArray data = frequency->networkReply()->readAll();
      QJsonParseError error;
      QJsonDocument jsonDoc = QJsonDocument::fromJson(data, &error);

      // Check JSON Reply
      bool jsonE = this->jsonError(data);
      if (jsonE) {
        qCWarning(dcFems()) << "Meter: Failed to parse JSON data" << data << ":"
                            << error.errorString();
        this->changeMeterString();
        frequency->deleteLater();
        return;
      }
      QVariant *var = new QVariant((this->getValueOfRequestedData(&jsonDoc)));
      this->addValueToThing(parentThing, meterThingClassId,
                            meterFrequencyStateTypeId, var, DOUBLE, -3);
      var = NULL;
      delete var;
      frequency->deleteLater();
    });
  }
  parentThing->deleteLater();
}

bool IntegrationPluginFems::connectionError(FemsNetworkReply *reply) {
  return reply->networkReply()->error() != QNetworkReply::NoError;
}

bool IntegrationPluginFems::jsonError(QByteArray data) {
  QJsonParseError error;
  QJsonDocument jsonDoc = QJsonDocument::fromJson(data, &error);
  return error.error != QJsonParseError::NoError;
}
// JSON of FEMS contains value behind "value"
QString IntegrationPluginFems::getValueOfRequestedData(QJsonDocument *json) {
  return json->toVariant().toMap().value(DATA_ACCESS_STRING_FEMS).toString();
}

void IntegrationPluginFems::addValueToThing(Thing *parentThing,
                                            ThingClassId identifier,
                                            StateTypeId stateName,
                                            const QVariant *value,
                                            ValueType valueType, int scale) {

  Thing *child = this->GetThingByParentAndClassId(parentThing, identifier);
  if (child != nullptr) {
    this->addValueToThing(child, stateName, value, valueType, scale);
    child->deleteLater();
  } else {
    delete child;
  }
}

void IntegrationPluginFems::addValueToThing(Thing *childThing,
                                            StateTypeId stateName,
                                            const QVariant *value,
                                            ValueType valueType, int scale) {

  // void setStateValue(const QString &stateName, const QVariant &value);
  if (value != nullptr) {
    if (valueType == DOUBLE) {
      double doubleValue = (value->toDouble()) * pow(10, scale);
      childThing->setStateValue(stateName, doubleValue);
    } else if (valueType == QSTRING) {
      QString myString = value->toString();
      childThing->setStateValue(stateName, myString);
    } else if (valueType == MY_BOOLEAN) {
      bool booleanValue = value->toBool();
      childThing->setStateValue(stateName, booleanValue);
    } else if (valueType == MY_INT) {
      int intValue = (value->toInt()) * pow(10, scale);
      childThing->setStateValue(stateName, intValue);
    }
  }
}

void IntegrationPluginFems::changeMeterString() {

  if (this->meter == METER_0) {
    this->meter = METER_1;
  } else if (this->meter == METER_1) {
    this->meter = METER_2;
  } else if (this->meter == METER_2) {
    this->meter = SKIP;
  } else {
    this->meter = SKIP;
  }
}

Thing *
IntegrationPluginFems::GetThingByParentAndClassId(Thing *parentThing,
                                                  ThingClassId identifier) {
  Things valueToAddThings = myThings()
                                .filterByParentId(parentThing->id())
                                .filterByThingClassId(identifier);
  if (valueToAddThings.count() == 1) {
    return valueToAddThings.first();
  }
  return nullptr;
}

void IntegrationPluginFems::checkBatteryState(Thing *parentThing) {
  // ChargingState -> this should be done seperately when comparing charing and
  // discharging energy idle, charging, discharging
  Thing *thing = GetThingByParentAndClassId(parentThing, batteryThingClassId);
  if (thing != nullptr) {
    QVariant chargingEnergy =
        thing->stateValue(batteryChargingEnergyStateTypeId);
    QVariant dischargingEnergy =
        thing->stateValue(batteryDischarginEnergyStateTypeId);
    int charging = chargingEnergy.toInt();
    int discharging = dischargingEnergy.toInt();

    if (charging != 0 && charging > discharging) {
      this->batteryState = "charging";
    } else if (discharging != 0 && discharging > charging) {
      this->batteryState = "discharging";
    } else if (discharging == charging) {
      this->batteryState = "idle";
    }
    QVariant var = this->batteryState;
    this->addValueToThing(thing, batteryChargingStateStateTypeId, &var, QSTRING,
                          0);
  }
  if (thing != nullptr) {
    thing->deleteLater();
  } else {
    delete thing;
  }
}

void IntegrationPluginFems::calculateStateOfHealth(Thing *parentThing) {
  // StateOfHealth -> when reaching 10% ESS -> Tell the System Critical ESS
  // State

  Thing *thing = GetThingByParentAndClassId(parentThing, batteryThingClassId);
  if (thing != nullptr) {
    QVariant socStateValue = thing->stateValue(batteryBatteryLevelStateTypeId);
    int soc = socStateValue.toInt();
    bool critical = false;
    if (soc <= 10) {
      critical = true;
    }
    QVariant var = critical ? "true" : "false";
    ;
    this->addValueToThing(thing, batteryBatteryCriticalStateTypeId, &var,
                          MY_BOOLEAN, 0);
    thing->deleteLater();
  } else {
    delete thing;
  }
}
