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

#include "plugininfo.h"
#include "integrationpluginfems.h"
#include "network/networkaccessmanager.h"
#include "network/networkdevicediscovery.h"

#include <QUrl>
#include <QDebug>
#include <QPointer>
#include <QUrlQuery>
#include <QJsonDocument>
#include <math.h>



IntegrationPluginFems::IntegrationPluginFems(QObject *parent) : IntegrationPlugin(parent)
{

}

void IntegrationPluginFems::init()
{
    // Initialisation can be done here.
    qCDebug(dcFems()) << "Plugin initialized.";
}

void IntegrationPluginFems::setupThing(ThingSetupInfo *info)
{


    Thing *thing = info->thing();
    qCDebug(dcFronius()) << "Setting up" << thing;

    if (thing->thingClassId() == connectionThingClassId) {

        QHostAddress address(thing->paramValue(connectionThingAddressParamTypeId).toString());

        // Handle reconfigure
        if (m_froniusConnections.values().contains(thing)) {
            FemsConnection *connection = m_femsConnections.key(thing);
            m_femsConnections.remove(connection);
            connection->deleteLater();
        }

        // Create the connection
        FemsConnection *connection =
                new FemsConnection(hardwareManager()->networkManager(),
                                   address, thing, thing->paramValue(connectionThingUserParamTypeId).toString(),
                                   thing->paramValue(connectionThingPasswordParamTypeId).toString(),
                                   thing->paramValue(connectionThingEdgeParamTypeId).toBool(),
                                   thing->paramValue(connectionThingPortParamTypeId).toString());
        FemsNetworkReply *reply = connection->isAvailable();
        connect(reply, &FemsNetworkReply::finished, info,[=]{

            QByteArray data = reply->networkReply()->readAll();
            if(reply->networkReply()->error() != QNetworkReply::NoError){
                //no URL bc URL contains uname and pwd
                qcWarning(dcFems() << "Network request error:") << reply->networkReply()->error() << reply->networkReply()->errorString() << connection->address().toIPv4Address().toString();
                if (reply->networkReply()->error() == QNetworkReply::ContentNotFoundError) {
                    info->finish(Thing::ThingErrorHardwareNotAvailable, QT_TR_NOOP("The device does not reply to our requests. Please verify that the FEMS API is enabled on the device."));
                } else {
                    info->finish(Thing::ThingErrorHardwareNotAvailable, QT_TR_NOOP("The device is not reachable."));
                }
                return;
            }

        // Convert the rawdata to a JSON document
        QJsonParseError error;
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data, &error);
        if (error.error != QJsonParseError::NoError) {
            qCWarning(dcFronius()) << "Failed to parse JSON data" << data << ":" << error.errorString() << data;
            info->finish(Thing::ThingErrorHardwareFailure, QT_TR_NOOP("The data received from the device could not be processed because the format is unknown."));
            return;
        }

        QVariantMap responseMap = jsonDoc.toVariant().toMap();
        QInt status = responseMap.value("value", -1).toInt();
        if(status >= 0){
            // 0 == ok, 1 == info, 2 == Warning, 3 == fault (todo change to strings)
            thing->setStateValue(connectionStatusStateTypeId, status);
        }
        qCDebug(dcFems()) << "Fems Status" << responseMap.value("value").toString();
        //STATE Connection
        m_femsConnections.insert(connection, thing);
        info->finish(Thing::ThingErrorNoError);
        thing->setStateValue("connected", true);



 });
} else if((thing->thingClassId() == meterThingClassId) ||
          (thing->thingClassId() == batteryThingClassId)) {
        Thing *parentThing = myThings.findById(thing->parentId());
        if(!parentThing){
            qCWarning(dcFems())<<"Could not find the parent for" << thing;
            info->finish(Thing::ThingErrorHardwareNotAvailable);
            return;
        }
        FemsConnection *connection = m_femsConnections.key(parentThing);
        if(!connection){
            qCWarning(dcFems()) << "Could not find the parent connection for" <<thing;
            info->finish(Thing::ThingErrorHardwareNotAvailable);
            return;
        }
        info->finish(Thing::ThingErrorNoError);
    } else {
        Q_ASSERT_X(false, "setupThing", QString("Unhandled thingClassId: %1").arg(thing->thingClassId().toString()).toUtf8());
    }

}


void IntegrationPluginFems::postSetupThing(Thing *thing){

     qCDebug(dcFems()) << "Post setup" << thing->name();


     if(thing->thingClassId() == connectionThingClassId){
         if(!m_connectionRefreshTimer){
             m_connectionRefreshTimer = hardwareManager()->pluginTimerManager()->registerTimer(10);
             connect(m_connectionRefreshTimer, &PluginTimer::timeout, this, [this](){

                 foreach(FemsConnection *connection, m_femsConnections.keys()){
                     refreshConnection(connection);
                 }

             });
             m_connectionRefreshTimer->start();

         }

         FemsConnection *connection = m_femsConnections.key(thing);
         if(connection){
             refreshConnection(connection);
         }
     }
}


void IntegrationPluginFems::thingRemoved(Thing *thing)
{
    if (thing->thingClassId() == connectionThingClassId) {
        FemsConnection *connection = m_femsConnections.key(thing);
        m_femsConnections.remove(connection);
        connection->deleteLater();
    }

    if (myThings().filterByThingClassId(connectionThingClassId).isEmpty()) {
        hardwareManager()->pluginTimerManager()->unregisterTimer(m_connectionRefreshTimer);
        m_connectionRefreshTimer = nullptr;
    }
}

void IntegrationPluginFems::executeAction(ThingActionInfo *info)
{
      Q_UNUSED(info)
}

//Poll data again
void IntegrationPluginFems::refreshConnection(FemsConnection * connection){
    if (connection->busy()){
        qCWarning(dcFems()) << "Connection busy. Skipping refresh cycle for host" << connection->address().toString();
        return;
    }
    //3 things, parent, and battery as well as meter but only parent connection nec.
    //GET Meter and Battery ID Later and update param info
    Thing *connectionThing = m_femsConnections.value(connection);
    if (!connectionThing)
        return;
    updateSumState(connection);
    updateMeters(connection);
    updateStorages(connection);

}

void IntegrationPluginFems::updateMeters(FemsConnection *connection){

    Thing *parentThing = m_femsConnections.value(connection);
    //Get everything from the sum that is meter related.
    //most states are available at the "sum"
    //Since FEMS/OpenEMS can have mutltiple Meters it can be risky to get multiple meters
    //start with meter0 -> if of type AsymmetricMeter it will have values at L1-L3 phase calls
    //If Symmetric it won't
    //You can try with meter0 and meter1 (should be sufficent)
    //However sum should supply enough data

    //GridActivePower
    FemsNetworkReply *currentGridPowerReply = connection->getFemsDataPoint(GRID_ACTIVE_POWER);

    connect(currentGridPowerReply, &FemsNetworkReply::finished, this, [=](){

        if(this->connectionError(currentGridPowerReply)){
            return;
        }
        QByteArray data = currentGridPowerReply->networkReply()->readAll();
        QJsonParseError error;
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data, &error);

        //Check JSON Reply
        if(this->jsonError(currentGridPowerReply, &data){
        qCWarning(dcFronius()) << "Meter: Failed to parse JSON data" << data << ":" << error.errorString();
                return;
    }
       //GET "value" of data
       this->addValueToThing(meterThingClassId,
                             meterCurrentGridPowerStateTypeId,
                             this->getValueOfRequestedData(jsonDoc), DOUBLE,0);

    });

    //ProductionActivePower
    FemsNetworkReply *currentPowerProduction = connection -> getFemsDataPoint(PRODCUTION_ACTIVE_POWER);
    connect(currentPowerProduction, &FemsNetworkReply::finished, this, [=](){

        if(this->connectionError(currentPowerProduction)){
            return;
        }
        QByteArray data = currentPowerProduction->networkReply()->readAll();
        QJsonParseError error;
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data, &error);

        //Check JSON Reply
        if(this->jsonError(currentPowerProduction, &data){
        qCWarning(dcFronius()) << "Meter: Failed to parse JSON data" << data << ":" << error.errorString();
                return;
    }
       //GET "value" of data
       this->addValueToThing(meterThingClassId,
                             meterCurrentPowerProductionStateTypeId,
                             this->getValueOfRequestedData(jsonDoc), DOUBLE,0);

    });

    //ProductionAcActivePower
    FemsNetworkReply *currentPowerProductionAc = connection -> getFemsDataPoint(PRODUCTION_ACTIVE_AC_POWER);
    //ProductionDcActivePower
    FemsNetworkReply *currentPowerProductionDc = connection -> getFemsDataPoint(PRODUCTION_ACTIVE_DC_POWER);;
    //CurrentPower
    FemsNetworkReply *currentPower =  connection -> getFemsDataPoint(CONSUMPTION_ACTIVE_POWER);
    //ProductionActiveEnergy
    FemsNetworkReply *totalEnergyProduced = connection -> getFemsDataPoint(GRID_PRODUCTION_ACTIVE_ENERGY);
    //ConsumptionActiveEnergy
    FemsNetworkReply *totalEnergyConsumed = connection -> getFemsDataPoint(GRID_CONSUMPTION_ACTIVE_ENERGY);
    //Grid BUY
    FemsNetworkReply *currentGridBuyEnergy = connection -> getFemsDataPoint(GRID_BUY_ACTIVE_ENERGY);
    //Grid SELL
    FemsNetworkReply *currentGridSellEnergy = connection -> getFemsDataPoint(GRID_SELL_ACTIVE_ENERGY);


    //GridActivePowerL1
    FemsNetworkReply *currentPowerPhaseA = connection -> getFemsDataPoint(GRID_ACTIVE_POWER_L1);
    //GridActivePowerL2
    FemsNetworkReply *currentPowerPhaseB = connection -> getFemsDataPoint(GRID_ACTIVE_POWER_L2);
    //GridActivePowerL3
    FemsNetworkReply *currentPhaseC = connection -> getFemsDataPoint(GRID_ACTIVE_POWER_L3);

    //HERE TEST CONNECTION! if Meter asymmetric -> check meter0 first -> if normally connection available -> test meter0 if no conn. test meter1
    //up to meter2 else -> just skip
    if(!(this->MeterType == SKIP)){
        QString PhaseA = this->meter+"/"+CURRENT_PHASE_1;
        QString PhaseB = this->meter+"/"+CURRENT_PHASE_2;
        QString PhaseC = this->meter+"/"+CURRENT_PHASE_3;
        QString Frequency = this->meter+"/"+FREQUENCY;
    //CurrentL1
    FemsNetworkReply *currentPhaseA = connection -> getFemsDataPoint(PhaseA);
    //Current L2
    FemsNetworkReply *currentPhaseB = connection -> getFemsDataPoint(
                this->getStringByMeterType(PhaseB);
    //Current L3
    FemsNetworkReply *currentPhaseC = connection -> getFemsDataPoint(
                this->getStringByMeterType(PhaseC));
    //Frequency
    FemsNetworkReply *frequency = connection -> getFemsDataPoint(
                this->getStringByMeterType(Frequency));
    }





}

    bool IntegrationPluginFems::connectionError(FemsNetworkReply* reply){
        return reply->networkReply()->error() != QNetworkReply::NoError;
    }

    bool IntegrationPluginFems::jsonError(FemsNetworkReply *reply, QByteArray *data){
        QJsonParseError error;
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data, &error);
        return error.error != QJsonParseError::NoError;

    }
    //JSON of FEMS contains value behind "value"
    QString IntegrationPluginFems::getValueOfRequestedData(QJsonDocument *json){
            return json->toVariant().toMap().value(DATA_ACCESS_STRING_FEMS).toString();
    }


    void IntegrationPluginFems::addValueToThing(ThingClassId identifier,
                                                const QString &stateName,
                                                const QVariant &value,
                                                ValueType valueType, int scale){

    Things *valueToAddThings = myThings().filterByParentId(parentThing->id()).filterByThingClassId(identifier);
        if(valueToAddThings->count() == 1){
         Thing *valueToAddThing = valueToAddThings->first();
            switch(valueType){
                case QSTRING:
                //ignore Scaling
                valueToAddThing->setStateValue(stateName, value);
                break;
                case DOUBLE:
                double doubleValue = value.toDouble() * pow(10, scale);
                valueToAddThing -> setStateValue(stateName, doubleValue);
                break;
                case BOOL:
                valueToAddThing -> setStateValue(stateName, value.toBool());
                break;
                case INT:
                int intValue = value.toInt() * pow(10,scale);
                valueToAddThing -> setStateValue(stateName,intValue);
                break;
            }
        }

    }
