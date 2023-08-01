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

#include "plugininfo.h"
#include "integrationplugineebus.h"
#include "network/networkdevicediscovery.h"
#include <network/networkaccessmanager.h>
#include <plugintimer.h>
#include <QtNetwork>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QTimer>
#include <QString>
#include <QNetworkAccessManager>
#include <QtCore/qglobal.h>
#include <QtCore/QRandomGenerator>
#include "once.h"
//Include qdbus
#include <QtDBus>
#include <QDBusConnection>


IntegrationPluginEebus::IntegrationPluginEebus()
{
}

void IntegrationPluginEebus::init()
{
    // Initialisation can be done here.
    qCDebug(dcEebus()) << "Plugin initialized.";
    // qCWarning(dcEebus()) << "Plugin EEBUS initialized. qCWarning.";
}

// --------------------------------------------------
// ----------------- discoverThings -----------------
// --------------------------------------------------

void IntegrationPluginEebus::discoverThings(ThingDiscoveryInfo *info)
{
    qCDebug(dcEebus()) << "Discover Things.";

    m_sockets = new QTcpSocket(this);
    m_sockets->connectToHost(ip, port);

    if (!m_sockets->waitForConnected()){return info->finish(Thing::ThingErrorThingClassNotFound);}

    // the actual discovery is wrapped inside the request for the ownSKI 
    sendOwnSkiRequest();
    readOwnSkiResponse(info);

}

// --------------- ownSKI ----------------------

void IntegrationPluginEebus::sendOwnSkiRequest()
{
    QJsonObject jsonRpc;
    jsonRpc["jsonrpc"] = "2.0";
    jsonRpc["id"] = 1;
    jsonRpc["method"] = "security/ownSKI";
    jsonRpc["params"] = {};

    QJsonDocument requestDoc(jsonRpc);
    qCDebug(dcEebus()) << "discover: ownSKI: Sending request: " << requestDoc;
    m_sockets->write(requestDoc.toJson());
}

void IntegrationPluginEebus::readOwnSkiResponse(ThingDiscoveryInfo *info)
{
    Once::connect(m_sockets, &QTcpSocket::readyRead, this, [this, info]()
            {
        QByteArray responseData = m_sockets->readAll();
        // qCDebug(dcEebus()) << "discover: ownSKI: Response data: " << responseData;
        QString ownSki = handleOwnSkiResponse(responseData); 

        // everything for ownSKI is done. Now make next async call
        sendDiscoveryRequest();
        readDiscoveryResponse(info,ownSki);
        });
}


QString IntegrationPluginEebus::handleOwnSkiResponse(const QByteArray &responseData)
{
    QJsonDocument responseDoc = QJsonDocument::fromJson(responseData);
    // qCDebug(dcEebus()) << "discover: ownSKI: Response doc: " << responseDoc;

    if (!responseDoc.isNull())
    {
        return responseDoc["result"].toString();
    }
    return "";
}

// --------------- discovery ----------------------

void IntegrationPluginEebus::sendDiscoveryRequest()
{
    QJsonObject jsonRpc;
    jsonRpc["jsonrpc"] = "2.0";
    jsonRpc["id"] = "1";
    jsonRpc["method"] = "security/discovered_devices";
    jsonRpc["params"] = {};

    QJsonDocument requestDoc(jsonRpc);
    qCDebug(dcEebus()) << "discover: Sending request: " << requestDoc;
    m_sockets->write(requestDoc.toJson());
}

void IntegrationPluginEebus::readDiscoveryResponse(ThingDiscoveryInfo *info, QString ownSki)
{
    Once::connect(m_sockets, &QTcpSocket::readyRead, this, [this, info, ownSki]()
            {
        QByteArray responseData = m_sockets->readAll();
        // qCDebug(dcEebus()) << "discover: Response data: " << responseData;

        handleDiscoveryResponse(responseData, info, ownSki); 

        info->finish(Thing::ThingErrorNoError);
        });
}

void IntegrationPluginEebus::handleDiscoveryResponse(const QByteArray &responseData, ThingDiscoveryInfo *info, QString ownSki)
{
    QJsonDocument responseDoc = QJsonDocument::fromJson(responseData);
    // qCDebug(dcEebus()) << "discover: Response doc: " << responseDoc;

    if (!responseDoc.isNull())
    {
        // qCDebug(dcEebus()) << "discover: !doc.isNull()";
        QJsonObject root = responseDoc.object();

        if (root["result"].isArray())
        {
            QJsonArray responseArray = root["result"].toArray();
            // qCDebug(dcEebus()) << "discover: result: " << responseArray;

            foreach (const QJsonValue &value, responseArray)
            {
                if (value.isObject())
                {
                    processDiscoveryObject(value.toObject(), info, ownSki);
                }
            }
        }
    }

}

void IntegrationPluginEebus::processDiscoveryObject(const QJsonObject &responseObject, ThingDiscoveryInfo *info, QString ownSki)
{
    // qCDebug(dcEebus()) << "discover: object: " << responseObject;
    // qCDebug(dcEebus()) << "discover: with keys: " << responseObject.keys();

    QString ski = responseObject.value("ski").toString();
    QString shipid = responseObject.value("shipid").toString();
    QString ipAddress = responseObject.value("ipAddress").toString();
    QString type = responseObject.value("type").toString();
    QString model = responseObject.value("model").toString();
    QString brand = responseObject.value("brand").toString();

    QString title = "EEBUS";
    QString description = brand + ", " + model + " (" + type + "), IP: " + ipAddress;

    ThingDescriptor descriptor(wallboxThingClassId, title, description);

    Things existingThings = myThings().filterByParam(wallboxThingIdentifierParamTypeId, ski);

    if (existingThings.count() == 1)
    {
        qCDebug(dcEebus()) << "This thing already exists in the system." << existingThings.first() << ski;
        descriptor.setThingId(existingThings.first()->id());
    }

    ParamList params;

    params << Param(wallboxThingShipidParamTypeId, shipid);
    params << Param(wallboxThingSkiParamTypeId, ski);
    params << Param(wallboxThingIpaddressParamTypeId, ipAddress);
    params << Param(wallboxThingOwnskiParamTypeId, ownSki);

    descriptor.setParams(params);
    info->addThingDescriptor(descriptor);
}

// --------------------------------------------------
// ----------------- setupThing ---------------------
// --------------------------------------------------

void IntegrationPluginEebus::setupThing(ThingSetupInfo *info)
{
    // A thing is being set up. Use info->thing() to get details of the thing, do
    // the required setup (e.g. connect to the device) and call info->finish() when done.

    qCDebug(dcEebus()) << "setupThing : Setup thing" << info->thing();

    m_sockets = new QTcpSocket(this);
    m_sockets->connectToHost(ip, port);
    // Check that connection was successful.
    if (!m_sockets->waitForConnected())
    {
        return info->finish(Thing::ThingErrorThingClassNotFound);
    }

    // TODO I commented this out. check if it is needed and remove it not.
    // connect(m_sockets, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::error), [=](QAbstractSocket::SocketError error)
    //         { qCDebug(dcEebus()) << "setupThing : Socket-Fehler: " << error; });

    // 0. enter pre-trust on the wallbox, using the provided ownSki
    // 1. send a request for trusting the device
    // 2. wait some time
    // 3. if the device is trusted, the identifier will be obtained
    // 4. if the device is not trusted, the identifier will be empty and we exit with error

    sendTrustSkiRequest(info);
    readTrustSkiResponse(info);
    // QThread::sleep(3);


}

// --------------- trust SKI ----------------------

void IntegrationPluginEebus::sendTrustSkiRequest(ThingSetupInfo *info)
{
    QJsonObject jsonRpc;
    jsonRpc["jsonrpc"] = "2.0";
    jsonRpc["id"] = 1;
    jsonRpc["method"] = "security/trustSKI";
    QJsonObject params;
    params["SKI"] = info->thing()->paramValue(wallboxThingSkiParamTypeId).toString();
    jsonRpc["params"] = params;

    QJsonDocument requestDoc(jsonRpc);
    qCDebug(dcEebus()) << "setupThing : trustSKI : Sending request: " << requestDoc;
    m_sockets->write(requestDoc.toJson());
}

void IntegrationPluginEebus::readTrustSkiResponse(ThingSetupInfo *info)
{
    // make a one shot connection to the socket
    Once::connect(m_sockets, &QTcpSocket::readyRead, this, [this,info]()
                  {
        // the call always returns ok, independent of whether trust was accepted or not.
        // Thus, we cannot use this for anything.
        QByteArray responseData = m_sockets->readAll();
        qCDebug(dcEebus()) << "discover: Response data: " << responseData;

        // once the device is trusted, we can get the identifier
        sendGetIdentifierRequest(info);
        readGetIdentifierResponse(info);

        });
}


// --------------- get Identifier ----------------------

void IntegrationPluginEebus::sendGetIdentifierRequest(ThingSetupInfo *info)
{
    QJsonObject jsonRpc;
    jsonRpc["jsonrpc"] = "2.0";
    jsonRpc["id"] = 1;
    jsonRpc["method"] = "emobility/getIdofEVSE";
    QJsonObject params;
    params["evse_shipid"] = info->thing()->paramValue(wallboxThingShipidParamTypeId).toString();
    jsonRpc["params"] = params;

    QJsonDocument requestDoc(jsonRpc);
    qCDebug(dcEebus()) << "setupThing : Identifier: Sending request: " << requestDoc;
    m_sockets->write(requestDoc.toJson());
}

void IntegrationPluginEebus::readGetIdentifierResponse(ThingSetupInfo *info)
{
    // make a one shot connection to the socket
    Once::connect(m_sockets, &QTcpSocket::readyRead, this, [this, info]()
                  {

        QByteArray responseData = m_sockets->readAll();
        
        handleGetIdentifierResponse(responseData, info);
        info->finish(Thing::ThingErrorNoError); });
}


void IntegrationPluginEebus::handleGetIdentifierResponse(const QByteArray &responseData, ThingSetupInfo *info)
{
    QJsonDocument responseDoc = QJsonDocument::fromJson(responseData);

    if (!responseDoc.isNull())
    {
        QJsonObject responseObject = responseDoc.object();

        if (responseObject.contains("result"))
        {
            QJsonObject resultObject = responseObject.value("result").toObject();
            if (resultObject.contains("shipid")) {
                QString identifier = resultObject.value("shipid").toString();
                qCDebug(dcEebus()) << "setupThing : identifier: write identifier: " << identifier;
                info->thing()->setParamValue(wallboxThingIdentifierParamTypeId, identifier);            
            } else if (resultObject.contains("error")) {
                qCDebug(dcEebus()) << "setupThing : identifier: no identifier found";
                info->finish(Thing::ThingErrorSetupFailed);
            } else {
                qCDebug(dcEebus()) << "setupThing : identifier: unknown response";
                info->finish(Thing::ThingErrorSetupFailed);
            }
        }
    }
}

// --------------------------------------------------
// ----------------- postSetupThing -----------------
// --------------------------------------------------

void IntegrationPluginEebus::postSetupThing(Thing *thing)
{
    if (!m_timer)
    {
        m_timer = hardwareManager()->pluginTimerManager()->registerTimer(5);
        connect(m_timer, &PluginTimer::timeout, [this]()
                {
                    foreach (Thing *t, myThings()) {
                        if (t->thingClassId() == wallboxThingClassId) 
                        {
                            // Refreshing the values
                            refreshValues(t);
                        }
                    } });
    }

    if (m_sockets->state() == QAbstractSocket::ConnectedState)
    {
        qCDebug(dcEebus()) << "postSetup : Connection established.";

        connect(m_sockets, &QTcpSocket::disconnected, this, [this, thing]()
                {
                    qCDebug(dcEebus()) << "postSetup : Connection interrupted.";
                    // Perform here your appropriate action, to react to the disconnection
                    if (m_retryConnection < 4)
                    {
                        m_sockets->connectToHost(ip, port);
                        m_retryConnection++;
                        QThread::sleep(1000);
                    }
                    else
                    {
                        /*do nothing*/
                    }

                    if (!m_sockets->waitForConnected())
                    {
                        qCDebug(dcEebus()) << "postSetup : Error while establishing a connection: " << m_sockets->errorString();
                        thing->setStateValue(wallboxConnectedStateTypeId, false);
                    } });
    }
    else
    {
        qCDebug(dcEebus()) << "postSetup : Error while establishing a connection: " << m_sockets->errorString();
    }
}

void IntegrationPluginEebus::refreshValues(Thing *thing)
{
    createRequestMonitorPowerConsumption(thing);

    createRequestMonitorBattery(thing);

    createRequestMonitorLPCValue(thing);
}

// --------------------------------------------------
// ----------------- createRequests ------------------
// --------------------------------------------------

/*
 *  This function creates a request to get the current power consumption of the wallbox.
 *  The request is sent to the wallbox and the response is parsed.
 *  The current power consumption is written to the state wallboxCurrentPowerStateTypeId.
 */
void IntegrationPluginEebus::createRequestMonitorPowerConsumption(Thing *thing)
{

    // Create a json-rpc-object for request
    QJsonObject jsonRpc;
    jsonRpc["jsonrpc"] = "2.0";
    jsonRpc["id"] = 1;
    jsonRpc["method"] = "emobility/getMPC";
    QJsonObject params;
    params["Identifier"] = thing->paramValue(wallboxThingIdentifierParamTypeId).toString(); // chargerID
    jsonRpc["params"] = params;

    // create a json-document
    QJsonDocument requestJsonDoc(jsonRpc);
    m_sockets->write(requestJsonDoc.toJson());

    connect(m_sockets, &QTcpSocket::readyRead, this, [this, thing]()
            {
        QByteArray responseData = m_sockets->readAll();


        // TODO Check if the following bug still exists: 
        // The response data may have an extra entry that has to be removed.
        // "{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":{\"current\":[0E0,0E0,0E0],\"power\":[0E0,0E0,0E0],\"voltage\":[0E0,0E0,0E0]}}
        //  {\"jsonrpc\":\"2.0\",\"id\":1,\"error\":{\"code\":-32000,\"data\":\"unknown function\",\"message\":\"unknown function\"}}"
        QString responseString = QString::fromUtf8(responseData);
        responseString=responseString.left(responseString.indexOf("}{\"jsonrpc\"")+1);
        QJsonDocument responseDoc = QJsonDocument::fromJson(responseString.toUtf8());         // parse Data

        if (!responseDoc.isNull())
        {
            // if data exists the connectable is connected
            thing->setStateValue(wallboxConnectedStateTypeId, true);

            if (responseDoc.isObject())
            {

                QJsonObject responseObject = responseDoc.object();
                qCDebug(dcEebus()) << "MPC : parsed responseObject : " << responseObject;

                QJsonArray powerArray = responseObject.value("result").toObject().value("power").toArray();
                // QJsonArray currentArray = responseObject.value("result").toObject().value("current").toArray();
                // QJsonArray voltageArray = responseObject.value("result").toObject().value("voltage").toArray();

                float power1 = powerArray[0].toDouble();
                float power2 = powerArray[1].toDouble();
                float power3 = powerArray[2].toDouble();
                qCDebug(dcEebus()) << "MPC : Power (1,2,3): " << power1 << power2 << power3;

                thing->setStateValue(wallboxCurrentPowerStateTypeId, power1 + power2 + power3);
                // for testing use this instead of the line above
                // thing->setStateValue(wallboxCurrentPowerStateTypeId, voltageArray[0].toDouble());  
            }
        } });
}

/*
 *  This function creates a request to get the current state of charge of the battery.
 *  The request is sent to the wallbox and the response is parsed.
 *  The current state of charge is written to the state wallboxCurrentStateOfChargeStateTypeId.
 */
void IntegrationPluginEebus::createRequestMonitorBattery(Thing *thing)
{
    // Create a json-rpc-object for request
    QJsonObject jsonRpc;
    jsonRpc["jsonrpc"] = "2.0";
    jsonRpc["method"] = "emobility/getBatteryInfo";
    QJsonObject params;
    params["identifier"] = thing->paramValue(wallboxThingIdentifierParamTypeId).toString(); // chargerId
    jsonRpc["params"] = params;
    jsonRpc["id"] = 1;

    QJsonDocument doc(jsonRpc);
    m_sockets->write(doc.toJson());
    connect(m_sockets, &QTcpSocket::readyRead, this, [this]()
            {
        QByteArray data = m_sockets->readAll();
        // qCDebug(dcEebus()) << "RMB : Empfangene Daten: " << data;
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (!doc.isNull())
        {
            if (doc.isObject())
            {
                QJsonObject obj = doc.object();
                if (obj.contains("NominalCapacity") && obj["NominalCapacity"].isString())
                {
                    QString nominalCapacity = obj["NominalCapacity"].toString();
                    qCDebug(dcEebus()) << "MonitorBattery : nominalCapacity" << nominalCapacity;
                }

                if (obj.contains("StateOfCharge") && obj["StateOfCharge"].isDouble())
                {
                    double stateOfCharge = obj["StateOfCharge"].toDouble();
                    qCDebug(dcEebus()) << "MonitorBattery : stateOfCharge" << stateOfCharge;
                }
            }
        } });
}

void IntegrationPluginEebus::createRequestMonitorLPCValue(Thing *thing)
{
    std::string sDbusService = "de.consolinno.fnnstb.iec61850";
    std::string sDbusPath = "/de/consolinno/fnnstb/iec61850/cls/actpow_ggio001/1";
    std::string sDbusInterface = "de.consolinno.fnnstb.iec61850.cls.actpow_ggio001";

    //Load current p-lim for consumption limit from iec server / only on hems with integrated iec server
    QDBusInterface iface(sDbusService.c_str(), sDbusPath.c_str(), sDbusInterface.c_str(), QDBusConnection::systemBus());
    //Get DBUS Property amout in format (xtixx) / struct, with first x as float value of current consumption limit
    QVariant reply = iface.property("AnOut_mxVal_f");
    if (reply.isValid()) {
        //Got power limit from dbus
        qCDebug(dcEebus()) << "Reply: " << reply.toFloat();
        m_consumptionLimit = reply.toFloat();
    } else {
        qCWarning(dcEebus()) << "Error getting consumption limit from dbus";
    }

    // //Add signal handler for consumption limit with same name as property on iface
    // qCDebug(dcEebus()) << "Signal subscribe: " << "sDbusService" << sDbusService.c_str() << "; "<< "sDbusPath" << sDbusPath.c_str() << "; " << "sDbusInterface" << sDbusInterface.c_str() << "AnOut_mxVal_f";
    
    // if(!QDBusConnection::systemBus().connect("", sDbusPath.c_str(), sDbusInterface.c_str() ,"AnOut_mxVal_f", this, SLOT(onConsumptionLimitChanged(qlonglong)))){
    //     qCWarning(dcEebus()) << "Error subscribing to consumption limit signal from iec server";
    // }else{
    //     qCDebug(dcEebus()) << "Subscribed to consumption limit signal";
    // }

    qCDebug(dcEebus()) << "Setting Power Limit " << m_consumptionLimit;
    thing->setStateValue(wallboxCurrentPowerLimitStateTypeId, m_consumptionLimit);
}


void IntegrationPluginEebus::onConsumptionLimitChanged(qlonglong consumptionLimit){
    //Echo to debug log, function "onConsumptionLimitChanged" is called
    qCDebug(dcEebus()) << "onConsumptionLimitChanged called";

    //set new consumption limit
    m_consumptionLimit = consumptionLimit;

//    if (m_energyManager->rootMeter()) {
//        qCDebug(dcEebus()) << "onConsumptionLimitChanged called and root meter is set";
//        qCDebug(dcEebus()) << "Using root meter" << m_energyManager->rootMeter();
//        //set new consumption limit
//        m_consumptionLimit = consumptionLimit;
//        // TODO: check that we indeed dont need to call evaluate() here. See the energy-plugin repo
//        // evaluate(); # we do not take any further action here. The evaluation is done in the evaluateAvailableUseCases() function
//    } else {
//        qCDebug(dcEebus()) << "onConsumptionLimitChanged called and root meter is not set";
//        qCWarning(dcEebus()) << "There is no root meter configured. Optimization will not be available until a root meter has been declared in the energy experience.";
//    }

    // TODO: check if we indeed dont need to call evaluateAvailableUseCases() here. See the energy-plugin repo.
    // evaluateAvailableUseCases();
}


// -------------------------------------------------
// ----------------- executeAction -----------------
// -------------------------------------------------

void IntegrationPluginEebus::executeAction(ThingActionInfo *info)
{
    // An action is being executed. Use info->action() to get details about the action,
    // do the required operations (e.g. send a command to the network) and call info->finish() when done.

    qCDebug(dcEebus()) << "executeAction : Executing action for thing" << info->thing() << info->action().actionTypeId().toString() << info->action().params();

    Action action = info->action();
//    Thing *thing = info->thing();

    info->finish(Thing::ThingErrorNoError);
}

// --------------------------------------------------
// ----------------- thingRemoved -------------------
// --------------------------------------------------

void IntegrationPluginEebus::thingRemoved(Thing *thing)
{
    // A thing is being removed from the system. Do the required cleanup
    // (e.g. disconnect from the device) here.

    qCDebug(dcEebus()) << "thingRemoved : Remove thing" << thing;

    m_sockets->deleteLater();
}
