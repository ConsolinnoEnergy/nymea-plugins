/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*
* Copyright 2013 - 2020, nymea GmbH
* Contact: contact@nymea.io
*
* This file is part of nymea.
* This project including source code and documentation is protected by
* copyright law, and remains the property of nymea GmbH. All rights, including
* reproduction, publication, editing and translation, are reserved. The use of
* this project is subject to the terms of a license agreement to be concluded
* with nymea GmbH in accordance with the terms of use of nymea GmbH, available
* under https://nymea.io/license
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
* For any further details and any questions please contact us under
* contact@nymea.io or see our FAQ/Licensing Information on
* https://nymea.io/license/faq
*
* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "plugininfo.h"
#include "integrationpluginaskoma.h"

#include <QUdpSocket>
#include <QHostAddress>
#include <network/networkdevicediscovery.h>
#include <hardwaremanager.h>
#include <QJsonDocument>

IntegrationPluginAskoma::IntegrationPluginAskoma()
{
}

void IntegrationPluginAskoma::discoverThings(ThingDiscoveryInfo *info)
{
    if (!hardwareManager()->networkDeviceDiscovery()->available()) 
    {
        qCWarning(dcAskoma()) << "The network discovery is not available on this platform.";
        info->finish(Thing::ThingErrorUnsupportedFeature, QT_TR_NOOP("The network device discovery is not available."));
        return;
    }

    if(this->m_pluginTimer != nullptr)
    {
        qCDebug(dcAskoma()) << "Discovery: Stopping plugin timer.";
        this->m_pluginTimer->stop();
    }

    // IP Scan. Which IPs are available?
    NetworkDeviceDiscoveryReply *discoveryReply = hardwareManager()->networkDeviceDiscovery()->discover();
    
    // A network device was found.
    connect(discoveryReply, &NetworkDeviceDiscoveryReply::networkDeviceInfoAdded, this, [=](const NetworkDeviceInfo &networkDeviceInfo)
    {        
        qCDebug(dcAskoma()) << "Discovery: Checking network device:" << networkDeviceInfo;

        // Try to send a request and see what happens.
        QUrl url;
        url.setScheme("http");
        url.setHost(networkDeviceInfo.address().toString());
        url.setPath("/getpar.json");

        QNetworkRequest request(url);

        QNetworkReply *reply =  hardwareManager()->networkManager()->get(request);
        
        this->l_pendingReplies.append(reply);

        connect(reply, &QNetworkReply::finished, this, [=]()
        {
            this->l_pendingReplies.removeAll(reply);
            reply->deleteLater();

            // Check HTTP reply
            if (reply->error() != QNetworkReply::NoError) 
            {
                qCDebug(dcAskoma()) << "Discovery: Checked" << networkDeviceInfo.address().toString()
                                    << "and a HTTP error occurred:" << reply->errorString() << "Continue...";
                return;
            }

            QByteArray data = reply->readAll();

            // Check JSON
            QJsonParseError error;
            QJsonDocument jsonDoc = QJsonDocument::fromJson(data, &error);

            if (error.error != QJsonParseError::NoError) 
            {
                qCDebug(dcAskoma()) << "Discovery: Checked" << networkDeviceInfo.address().toString()
                                    << "and received invalid JSON data:" << error.errorString() << "Continue...";
                return;
            }

            if (!jsonDoc.isObject()) 
            {
                qCDebug(dcAskoma()) << "Discovery: Response JSON is not an Object" << networkDeviceInfo.address().toString() << "Continue...";
                return;
            }

            QVariantMap map = jsonDoc.toVariant().toMap();

            if (map.contains("DATETIME") && map.contains("MODBUS_PAR_ID") && map.contains("MODBUS_PAR_TYPE")) 
            {
                // Ok, seems to be a Askoheat+ we are talking to... add to the discovery results...
                qCDebug(dcAskoma()) << "Discovery: --> Found Askoheat+ on" << networkDeviceInfo;
                this->m_discoveryResults.append(networkDeviceInfo);
            }
        });
    });

    connect(discoveryReply, &NetworkDeviceDiscoveryReply::finished, this, [=]()
    {
        QTimer::singleShot(3000, this, [=]()
        {
            qCDebug(dcAskoma()) << "Discovery: Grace period timer triggered.";
            
            discoveryReply->deleteLater();
            
            // Remove all pending replies.
            foreach (QNetworkReply *reply, this->l_pendingReplies) 
            {
                reply->abort();
                this->l_pendingReplies.removeAll(reply);
                reply->deleteLater();
            }  

            foreach (const NetworkDeviceInfo &networkDeviceInfo, this->m_discoveryResults) 
            {
                QString title;
                if (networkDeviceInfo.hostName().isEmpty()) 
                {
                    title = "Askoheat+ (" + networkDeviceInfo.address().toString() + ")";
                } 
                else 
                {
                    title = networkDeviceInfo.hostName() + " (" + networkDeviceInfo.address().toString() + ")";
                }

                QString description;
                if (networkDeviceInfo.macAddressManufacturer().isEmpty()) 
                {
                    description = networkDeviceInfo.macAddress();
                } 
                else 
                {
                    description = networkDeviceInfo.macAddress() + " (" + networkDeviceInfo.macAddressManufacturer() + ")";
                }

                ThingDescriptor descriptor(askoheatThingClassId, title, description);
                
                // Check if we already have set up this device
                Things existingThings = myThings().filterByParam(askoheatThingMacAddressParamTypeId, networkDeviceInfo.macAddress());
                if (existingThings.count() == 1) 
                {
                    qCDebug(dcAskoma()) << "This connection already exists in the system:" << networkDeviceInfo;
                    descriptor.setThingId(existingThings.first()->id());
                }
                
                ParamList params;
                params << Param(askoheatThingIpAddressParamTypeId, networkDeviceInfo.address().toString());
                params << Param(askoheatThingMacAddressParamTypeId, networkDeviceInfo.macAddress());
                descriptor.setParams(params);

                info->addThingDescriptor(descriptor);
            }

            this->m_discoveryResults.clear();
            this->m_askoheats.clear();

            info->finish(Thing::ThingErrorNoError);          
        });
    });
}

void IntegrationPluginAskoma::setupThing(ThingSetupInfo *info)
{
    Thing *thing = info->thing();
    qCDebug(dcAskoma()) << "Setup" << thing << thing->params();
    
    if(thing->thingClassId() == askoheatThingClassId) 
    {
        QString MacAddress_string = thing->paramValue(askoheatThingMacAddressParamTypeId).toString();
        QString IpAddress_string = thing->paramValue(askoheatThingIpAddressParamTypeId).toString();

        MacAddress macAddress = MacAddress(MacAddress_string);
        QHostAddress hostAddress = QHostAddress(IpAddress_string);
        
        QString hostAddress_string = hostAddress.toString();

        Askoheat *askoheat;

        if (this->m_askoheats.contains(thing))
        {
            qCDebug(dcAskoma()) << "Setup after reconfiguration";
            askoheat = this->m_askoheats.take(thing);
            askoheat->m_askomaMacAddress = MacAddress_string;
            askoheat->m_askomaHostAddress = hostAddress_string;
        }
        else
        {
            askoheat = new Askoheat(MacAddress_string, hostAddress_string);
        }
                
        // Make sure we have a valid mac address, otherwise no monitor and no auto searching is possible.
        // Testing for null is necessary, because registering a monitor with a zero mac adress will cause a segfault.
        if (macAddress.isNull()) 
        {
            qCWarning(dcAskoma()) << "Failed to set up Askoma heating rod because the MAC address is not valid:" << MacAddress_string;
            info->finish(Thing::ThingErrorInvalidParameter, QT_TR_NOOP("The MAC address is not vaild. Please reconfigure the device to fix this."));
            return;
        }
        
        /* First we check if the ema endpoint is reachable. This endpoint has to be available. */
        QUrl url;
        url.setScheme("http");
        url.setHost(hostAddress_string);
        url.setPath("/getema.json");

        QNetworkRequest request(url);

        QNetworkReply *reply = hardwareManager()->networkManager()->get(request);

        connect(reply, &QNetworkReply::finished, this, [=]()
        {
            reply->deleteLater();
            // Check HTTP reply
            if (reply->error() != QNetworkReply::NoError) 
            {
                qCDebug(dcAskoma()) << "Setup: Requested ema data and a HTTP error occurred:" << reply->errorString();
                thing->setStateValue(askoheatConnectedStateTypeId, false);
                info->finish(Thing::ThingErrorHardwareFailure);
                return;
            }

            qCDebug(dcAskoma()) << "Setup: ema endpoint is available.";

            /* Read the firmware version. */
            QUrl par_url;
            par_url.setScheme("http");
            par_url.setHost(hostAddress_string);
            par_url.setPath("/getpar.json");

            QNetworkRequest par_request(par_url);

            QNetworkReply *par_reply = hardwareManager()->networkManager()->get(par_request);

            connect(par_reply, &QNetworkReply::finished, this, [=]()
            {
                par_reply->deleteLater();

                if(par_reply->error() != QNetworkReply::NoError) 
                {
                    qCDebug(dcAskoma()) << "Setup: Requested par data and a HTTP error occurred:" << par_reply->errorString();
                }

                QByteArray data = par_reply->readAll();

                // Check JSON
                QJsonParseError error;
                QJsonDocument jsonDoc = QJsonDocument::fromJson(data, &error);

                if (error.error != QJsonParseError::NoError) 
                {
                    qCDebug(dcAskoma()) << "Setup: received invalid JSON data:" << error.errorString();
                    return;
                }

                if (!jsonDoc.isObject()) 
                {
                    qCDebug(dcAskoma()) << "Setup: Response JSON is not an Object";
                    return;
                }

                QVariantMap map = jsonDoc.toVariant().toMap();

                if(map.contains("MODBUS_PAR_SOFTWARE_VERSION"))
                {

                    QString firmware_version = map["MODBUS_PAR_SOFTWARE_VERSION"].toString();

                    qCDebug(dcAskoma()) << "Setup: Received parameter (PTH) data from Askoheat+";
                    qCDebug(dcAskoma()) << "Firmware version: " << firmware_version;

                    askoheat->m_firmwareVersion = firmware_version;

                    thing->setStateValue(askoheatFirmwareVersionStateTypeId, askoheat->m_firmwareVersion);
                }
            }); 

            /* Now we check if the pth endpoint is available. This endpoint is only available on newer firmware versions. */
            QUrl pth_url;
            pth_url.setScheme("http");
            pth_url.setHost(hostAddress_string);
            pth_url.setPath("/getpth.json");

            QNetworkRequest pth_request(pth_url);

            qCDebug(dcAskoma()) << "Setup: requesting " << pth_url;

            QNetworkReply *pth_reply =  hardwareManager()->networkManager()->get(pth_request);

            connect(pth_reply, &QNetworkReply::finished, this, [=]()
            {
                pth_reply->deleteLater();

                if (pth_reply->error() != QNetworkReply::NoError) 
                {
                    qCDebug(dcAskoma()) << "Setup: Requested pth data and a HTTP error occurred:" << pth_reply->errorString();
                    askoheat->m_pthAvailable = false;
                }
                else
                {
                    qCDebug(dcAskoma()) << "Setup: pth endpoint is available.";
                    askoheat->m_pthAvailable = true;
                }

                /* Write all the current variables. */
                thing->setStateValue(askoheatStatusStateTypeId, askoheat->m_status);

                thing->setStateValue(askoheatHeater1ActiveStateTypeId, (askoheat->m_status & 1));
                thing->setStateValue(askoheatHeater2ActiveStateTypeId, (askoheat->m_status & 2));
                thing->setStateValue(askoheatHeater3ActiveStateTypeId, (askoheat->m_status & 4));
                thing->setStateValue(askoheatPumpActiveStateTypeId, (askoheat->m_status & 8));
                thing->setStateValue(askoheatValveActiveStateTypeId, (askoheat->m_status & 16));
                thing->setStateValue(askoheatHeaterCurrentFlowStateTypeId, (askoheat->m_status & 32));
                thing->setStateValue(askoheatHeatpumpRequestActiveStateTypeId, (askoheat->m_status & 64));
                thing->setStateValue(askoheatEmergencyModeActiveStateTypeId, (askoheat->m_status & 128));

                thing->setStateValue(askoheatLegionellaProtectionActiveStateTypeId, (askoheat->m_status & 256));
                thing->setStateValue(askoheatAnalogInputActiveStateTypeId, (askoheat->m_status & 512));
                thing->setStateValue(askoheatLoadSetpointActiveStateTypeId, (askoheat->m_status & 1024));
                thing->setStateValue(askoheatLoadFeedinActiveStateTypeId, (askoheat->m_status & 2048));
                thing->setStateValue(askoheatAutoHeaterOffActiveStateTypeId, (askoheat->m_status & 4096));
                thing->setStateValue(askoheatPumpRelayFollowUpTimeActiveStateTypeId, (askoheat->m_status & 8192));
                thing->setStateValue(askoheatTemperatureLimitReachedStateTypeId, (askoheat->m_status & 16384));
                thing->setStateValue(askoheatAnyErrorOccuredStateTypeId, (askoheat->m_status & 32768));

                thing->setStateValue(askoheatCurrentPowerStateTypeId, askoheat->m_heaterLoad);
                thing->setStateValue(askoheatHeaterLoadStateTypeId, askoheat->m_heaterLoad);
                thing->setStateValue(askoheatHeaterStepStateTypeId, askoheat->m_heaterStep);
                thing->setStateValue(askoheatSetpointValueStateTypeId, askoheat->m_loadSetpointValue);
                thing->setStateValue(askoheatHeatingPowerStateTypeId, askoheat->m_loadFeedinValue);
                thing->setStateValue(askoheatEmergencyModeStateTypeId, askoheat->m_emergencyMode);
                thing->setStateValue(askoheatHeatpumpRequestStateTypeId, askoheat->m_heatPumpRequest); 
                thing->setStateValue(askoheatAnalogInputStateTypeId, askoheat->m_analogInputFloat);
                thing->setStateValue(askoheatTemperatureSensor0StateTypeId, askoheat->m_temperatureSensor0);
                thing->setStateValue(askoheatTemperatureSensor1StateTypeId, askoheat->m_temperatureSensor1);
                thing->setStateValue(askoheatTemperatureSensor2StateTypeId, askoheat->m_temperatureSensor2);
                thing->setStateValue(askoheatTemperatureSensor3StateTypeId, askoheat->m_temperatureSensor3);
                thing->setStateValue(askoheatTemperatureSensor4StateTypeId, askoheat->m_temperatureSensor4);
                thing->setStateValue(askoheatTotalEnergyConsumedStateTypeId, askoheat->m_totalEnergyConsumed);

                info->thing()->setStateValue(askoheatPowerStateTypeId, askoheat->m_power);
                info->thing()->setStateValue(askoheatHeatingPowerStateTypeId, askoheat->m_heatingPower.toUInt());

                this->m_askoheats.insert(info->thing(), askoheat);

                info->finish(Thing::ThingErrorNoError);
            });

        });
    }
}

void IntegrationPluginAskoma::postSetupThing(Thing *thing)
{
    if (thing->thingClassId() == askoheatThingClassId) 
    {
        if (!this->m_pluginTimer) 
        {
            qCDebug(dcAskoma()) << "Starting plugin timer...";

            this->m_pluginTimer = hardwareManager()->pluginTimerManager()->registerTimer(2);
    
            connect(this->m_pluginTimer, &PluginTimer::timeout, this, [this, thing] 
            {
                Askoheat *askoheat = this->m_askoheats.value(thing, nullptr);
                if (askoheat == nullptr)
                {
                    qCWarning(dcAskoma()) << "Plugin timer timeout: no Askoheat object for thing!";
                    return;
                }

                QUrl ema_url;
                ema_url.setScheme("http");
                ema_url.setHost(askoheat->m_askomaHostAddress);
                ema_url.setPath("/getema.json");

                QNetworkRequest ema_request(ema_url);

                QNetworkReply *ema_reply =  hardwareManager()->networkManager()->get(ema_request);

                connect(ema_reply, &QNetworkReply::finished, this, [=]()
                {
                    askoheat->onGetEMA(ema_reply, thing);
                });

                if(askoheat->m_pthAvailable)
                {
                    QUrl pth_url;
                    pth_url.setScheme("http");
                    pth_url.setHost(askoheat->m_askomaHostAddress);
                    pth_url.setPath("/getpth.json");

                    QNetworkRequest pth_request(pth_url);

                    QNetworkReply *pth_reply =  hardwareManager()->networkManager()->get(pth_request);

                    connect(pth_reply, &QNetworkReply::finished, this, [=]()
                    {
                        askoheat->onGetPTH(pth_reply, thing);
                    });
                }

                if(askoheat->m_power)
                {
                    if(askoheat->m_heatingPower.toUInt() == 0)
                    {
                        qCDebug(dcAskoma()) << "Plugin timer: Current heating power is 0 W. No put request necessary.";
                    }
                    else
                    {
                        this->setHeatingPower(thing);
                    }
                }
            });
        }

        this->m_pluginTimer->stop();
        this->m_pluginTimer->start();
    }
}

void IntegrationPluginAskoma::thingRemoved(Thing *thing)
{
    qCDebug(dcAskoma()) << "Remove: Removing ASKOHEAT+.";
    
    if (thing->thingClassId() == askoheatThingClassId)
    {
        this->m_askoheats.take(thing)->deleteLater();
    }

    if (myThings().isEmpty() && this->m_pluginTimer) 
    {
        qCDebug(dcAskoma()) << "Remove: Stopping plugin timer...";
        hardwareManager()->pluginTimerManager()->unregisterTimer(this->m_pluginTimer);
        this->m_pluginTimer = nullptr;
    }
}

void IntegrationPluginAskoma::executeAction(ThingActionInfo *info)
{
    if (info->thing()->thingClassId() == askoheatThingClassId)
    {
        Askoheat *askoheat = this->m_askoheats.value(info->thing(), nullptr);

        if (askoheat == nullptr)
        {
            qCWarning(dcAskoma()) << "Execute action: no Askoheat object for thing!";
            info->thing()->setStateValue(askoheatConnectedStateTypeId, false);
            info->finish(Thing::ThingErrorNoError);
        }
        
        if (info->action().actionTypeId() == askoheatHeaterStepActionTypeId)
        {   
            qCWarning(dcAskoma()) << "Unsupported action setHeaterStep called but not executed.";
            info->finish(Thing::ThingErrorNoError);
        }
        else if (info->action().actionTypeId() == askoheatSetpointValueActionTypeId)
        {
            qCWarning(dcAskoma()) << "Unsupported action loadSetpointValue called but not executed.";
            info->finish(Thing::ThingErrorNoError);
        }
        else if (info->action().actionTypeId() == askoheatFeedinValueActionTypeId)
        {
            qCWarning(dcAskoma()) << "Unsupported action loadFeedinValue called but not executed.";
            info->finish(Thing::ThingErrorNoError);
        }
        else if (info->action().actionTypeId() == askoheatHeatingPowerActionTypeId)
        {
            if (askoheat->m_power)
            {
                QString heatingPower = info->action().paramValue(askoheatHeatingPowerActionHeatingPowerParamTypeId).toString();
                info->thing()->setStateValue(askoheatHeatingPowerStateTypeId, heatingPower.toUInt());
                info->finish(Thing::ThingErrorNoError);
                
                qCDebug(dcAskoma()) << "Executing action: set heating power: " << heatingPower.toUInt() << " [W]";

                if (heatingPower == askoheat->m_heatingPower)
                {
                    return;
                }
                
                askoheat->m_heatingPower = heatingPower;

                this->setHeatingPower(info->thing());
            }
            else
            {
                qCWarning(dcAskoma()) << "Heating power not set because power is turned off.";
                info->finish(Thing::ThingErrorNoError);
            }
        }
        else if(info->action().actionTypeId() == askoheatPowerActionTypeId)
        {
            askoheat->m_power = info->action().paramValue(askoheatPowerActionPowerParamTypeId).toBool();
            info->thing()->setStateValue(askoheatPowerStateTypeId, askoheat->m_power);
            info->finish(Thing::ThingErrorNoError); 

            qCDebug(dcAskoma()) << "Set power" << askoheat->m_power;
                
            if(!askoheat->m_power)
            {
                askoheat->m_heatingPower = "0";
                info->thing()->setStateValue(askoheatHeatingPowerStateTypeId, 0);

                this->setHeatingPower(info->thing());
            }
        }
    }
}

void IntegrationPluginAskoma::setHeatingPower(Thing *thing)
{
    Askoheat *askoheat = this->m_askoheats.value(thing);
    
    // Write setpoint value
    QJsonDocument doc;
    QJsonObject obj;
    obj["MODBUS_EMA_LOAD_SETPOINT_VALUE"] = askoheat->m_heatingPower;
    doc.setObject(obj);

    QUrl url;
    url.setScheme("http");
    url.setHost(askoheat->m_askomaHostAddress);
    url.setPath("/");

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::KnownHeaders::ContentTypeHeader, "application/json");
    QByteArray data = doc.toJson(QJsonDocument::JsonFormat::Compact);

    qCDebug(dcAskoma()) << "Writing heating power: " << askoheat->m_heatingPower.toUInt() << " [W]";

    QNetworkReply *reply = hardwareManager()->networkManager()->put(request, data);

    connect(reply, &QNetworkReply::finished, this, [=]()
    {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) 
        {
            qCWarning(dcAskoma()) << "Writing heating power failed due to a HTTP error:" << reply->errorString();
        }
        else
        {
            qCDebug(dcAskoma()) << "Writing heating power finished successfully.";
        }
    });
}
