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

#include "askoheat.h"
#include <QJsonDocument>

Askoheat::Askoheat(QString MacAddress, QString HostAddress)
{
    this->m_askomaMacAddress = MacAddress;
    this->m_askomaHostAddress = HostAddress;
}

void Askoheat::onGetEMA(QNetworkReply *reply, Thing *thing)
{
    reply->deleteLater();

    // Check HTTP reply
    if (reply->error() != QNetworkReply::NoError) 
    {
        qCDebug(dcAskoma()) << "Update: Requested EMA data and a HTTP error occurred:" << reply->errorString();
        thing->setStateValue(askoheatConnectedStateTypeId, false);
        this->setDefaultValues(thing);
        return;
    }

    QByteArray data = reply->readAll();

    // Check JSON
    QJsonParseError error;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(data, &error);

    if (error.error != QJsonParseError::NoError) 
    {
        qCDebug(dcAskoma()) << "Update: received invalid JSON data:" << error.errorString();
        return;
    }

    if (!jsonDoc.isObject()) 
    {
        qCDebug(dcAskoma()) << "Update: Response JSON is not an Object";
        return;
    }

    QVariantMap map = jsonDoc.toVariant().toMap();

    if (map.contains("MODBUS_EMA_STATUS")) 
    {
        // Ok, seems to be a Askoheat+ we are talking to...
        qCDebug(dcAskoma()) << "Update: Received energy manager (EMA) data from Askoheat+";
        
        quint16 new_status           = map["MODBUS_EMA_STATUS"].toUInt();
        quint16 new_heaterLoad       = map["MODBUS_EMA_HEATER_LOAD"].toUInt();
        quint16 new_heaterStep       = map["MODBUS_EMA_SET_HEATER_STEP"].toUInt();
        qint16 new_loadSetpointValue = map["MODBUS_EMA_LOAD_SETPOINT_VALUE"].toInt();
        qint16 new_loadFeedinValue   = map["MODBUS_EMA_LOAD_FEEDIN_VALUE"].toInt();
        quint16 new_emergencyMode    = map["MODBUS_EMA_EMERGENCY_MODE"].toUInt();
        quint16 new_heatPumpRequest  = map["MODBUS_EMA_HEAT_PUMP_REQUEST"].toUInt();
        float new_currentPower       = map["MODBUS_EMA_HEATER_LOAD"].toDouble();
        float new_analogInputFloat   = map["MODBUS_EMA_ANALOG_INPUT_FLOAT"].toDouble();
        float new_temperatureSensor0 = map["MODBUS_EMA_TEMPERATURE_FLOAT_SENSOR0"].toDouble();
        float new_temperatureSensor1 = map["MODBUS_EMA_TEMPERATURE_FLOAT_SENSOR1"].toDouble();
        float new_temperatureSensor2 = map["MODBUS_EMA_TEMPERATURE_FLOAT_SENSOR2"].toDouble();
        float new_temperatureSensor3 = map["MODBUS_EMA_TEMPERATURE_FLOAT_SENSOR3"].toDouble();
        float new_temperatureSensor4 = map["MODBUS_EMA_TEMPERATURE_FLOAT_SENSOR4"].toDouble();

        qCDebug(dcAskoma()) << "Status: " << new_status << " []";
        qCDebug(dcAskoma()) << "Heater load: " << new_heaterLoad << " [W]";
        qCDebug(dcAskoma()) << "Heater step:" << new_heaterStep << " []";
        qCDebug(dcAskoma()) << "Setpoint value: " << new_loadSetpointValue << " [W]";
        qCDebug(dcAskoma()) << "Feedin value: " << new_loadFeedinValue << " [W]";
        qCDebug(dcAskoma()) << "Emergency mode: " << new_emergencyMode << " []";
        qCDebug(dcAskoma()) << "Heatpump request: " << new_heatPumpRequest << " []";
        qCDebug(dcAskoma()) << "Analog input float: " << new_analogInputFloat << " [V]";
        qCDebug(dcAskoma()) << "Temperature sensor 0: " << new_temperatureSensor0 << " [°C]";
        qCDebug(dcAskoma()) << "Temperature sensor 1: " << new_temperatureSensor1 << " [°C]";
        qCDebug(dcAskoma()) << "Temperature sensor 2: " << new_temperatureSensor2 << " [°C]";
        qCDebug(dcAskoma()) << "Temperature sensor 3: " << new_temperatureSensor3 << " [°C]";
        qCDebug(dcAskoma()) << "Temperature sensor 4: " << new_temperatureSensor4 << " [°C]";

        if(this->m_status != new_status)
        {
            this->m_status = new_status;
            thing->setStateValue(askoheatStatusStateTypeId, this->m_status);

            thing->setStateValue(askoheatHeater1ActiveStateTypeId, (this->m_status & 1));
            thing->setStateValue(askoheatHeater2ActiveStateTypeId, (this->m_status & 2));
            thing->setStateValue(askoheatHeater3ActiveStateTypeId, (this->m_status & 4));
            thing->setStateValue(askoheatPumpActiveStateTypeId, (this->m_status & 8));
            thing->setStateValue(askoheatValveActiveStateTypeId, (this->m_status & 16));
            thing->setStateValue(askoheatHeaterCurrentFlowStateTypeId, (this->m_status & 32));
            thing->setStateValue(askoheatHeatpumpRequestActiveStateTypeId, (this->m_status & 64));
            thing->setStateValue(askoheatEmergencyModeActiveStateTypeId, (this->m_status & 128));

            thing->setStateValue(askoheatLegionellaProtectionActiveStateTypeId, (this->m_status & 256));
            thing->setStateValue(askoheatAnalogInputActiveStateTypeId, (this->m_status & 512));
            thing->setStateValue(askoheatLoadSetpointActiveStateTypeId, (this->m_status & 1024));
            thing->setStateValue(askoheatLoadFeedinActiveStateTypeId, (this->m_status & 2048));
            thing->setStateValue(askoheatAutoHeaterOffActiveStateTypeId, (this->m_status & 4096));
            thing->setStateValue(askoheatPumpRelayFollowUpTimeActiveStateTypeId, (this->m_status & 8192));
            thing->setStateValue(askoheatTemperatureLimitReachedStateTypeId, (this->m_status & 16384));
            thing->setStateValue(askoheatAnyErrorOccuredStateTypeId, (this->m_status & 32768));
        }
        
        if(this->m_currentPower != new_currentPower)
        {
            this->m_currentPower = new_currentPower;
            thing->setStateValue(askoheatCurrentPowerStateTypeId, this->m_currentPower);
        }
        if(this->m_heaterLoad != new_heaterLoad)
        {
            this->m_heaterLoad = new_heaterLoad;
            thing->setStateValue(askoheatHeaterLoadStateTypeId, this->m_heaterLoad);
        }
        if(this->m_heaterStep != new_heaterStep)
        {
            this->m_heaterStep = new_heaterStep;
            thing->setStateValue(askoheatHeaterStepStateTypeId, this->m_heaterStep);
        }
        if(this->m_loadSetpointValue != new_loadSetpointValue)
        {
            this->m_loadSetpointValue = new_loadSetpointValue;
            thing->setStateValue(askoheatSetpointValueStateTypeId, this->m_loadSetpointValue);
        }
        if(this->m_loadFeedinValue != new_loadFeedinValue)
        {
            this->m_loadFeedinValue = new_loadFeedinValue;
            thing->setStateValue(askoheatHeatingPowerStateTypeId, this->m_loadFeedinValue);
        }
        if(this->m_emergencyMode != new_emergencyMode)
        {
            this->m_emergencyMode = new_emergencyMode;
            thing->setStateValue(askoheatEmergencyModeStateTypeId, this->m_emergencyMode);
        }
        if(this->m_heatPumpRequest != new_heatPumpRequest)
        {
            this->m_heatPumpRequest = new_heatPumpRequest;
            thing->setStateValue(askoheatHeatpumpRequestStateTypeId, this->m_heatPumpRequest); 
        }
        if(this->m_analogInputFloat != new_analogInputFloat)
        {
            this->m_analogInputFloat = new_analogInputFloat;
            thing->setStateValue(askoheatAnalogInputStateTypeId, this->m_analogInputFloat);
        }
        if(this->m_temperatureSensor0 != new_temperatureSensor0)
        {
            this->m_temperatureSensor0 = new_temperatureSensor0;
            thing->setStateValue(askoheatTemperatureSensor0StateTypeId, this->m_temperatureSensor0);
        }
        if(this->m_temperatureSensor1 != new_temperatureSensor1)
        {
            this->m_temperatureSensor1 = new_temperatureSensor1;
            thing->setStateValue(askoheatTemperatureSensor1StateTypeId, this->m_temperatureSensor1);
        }
        if(this->m_temperatureSensor2 != new_temperatureSensor2)
        {
            this->m_temperatureSensor2 = new_temperatureSensor2;
            thing->setStateValue(askoheatTemperatureSensor2StateTypeId, this->m_temperatureSensor2);
        }
        if(this->m_temperatureSensor3 != new_temperatureSensor3)
        {
            this->m_temperatureSensor3 = new_temperatureSensor3;
            thing->setStateValue(askoheatTemperatureSensor3StateTypeId, this->m_temperatureSensor3);
        }
        if(this->m_temperatureSensor4 != new_temperatureSensor4)
        {
            this->m_temperatureSensor4 = new_temperatureSensor4;
            thing->setStateValue(askoheatTemperatureSensor4StateTypeId, this->m_temperatureSensor4);
        }
                
        thing->setStateValue(askoheatConnectedStateTypeId, true);
    }
}

void Askoheat::onGetPTH(QNetworkReply *reply, Thing *thing)
{
    reply->deleteLater();

    // Check HTTP reply
    if (reply->error() != QNetworkReply::NoError) 
    {
        qCDebug(dcAskoma()) << "Update: Requested PTH data and a HTTP error occurred:" << reply->errorString();
        thing->setStateValue(askoheatConnectedStateTypeId, false);
        this->setDefaultValues(thing);
        return;
    }

    QByteArray data = reply->readAll();

    // Check JSON
    QJsonParseError error;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(data, &error);

    if (error.error != QJsonParseError::NoError) 
    {
        qCDebug(dcAskoma()) << "Update: received invalid JSON data:" << error.errorString();
        return;
    }

    if (!jsonDoc.isObject()) 
    {
        qCDebug(dcAskoma()) << "Update: Response JSON is not an Object";
        return;
    }

    QVariantMap map = jsonDoc.toVariant().toMap();

    if(map.contains("POWER_CONSUMPTION"))
    {
        map = map.value("POWER_CONSUMPTION").toMap();

        if (map.contains("CONSUMPTION_KW_H")) 
        {
            float consumption_kWh = map["CONSUMPTION_KW_H"].toDouble();

            qCDebug(dcAskoma()) << "Update: Received power to heat (PTH) data from Askoheat+";
            qCDebug(dcAskoma()) << "Total energy consumption: " << consumption_kWh << " [kWh]";

            if(this->m_totalEnergyConsumed != consumption_kWh)
            {
                this->m_totalEnergyConsumed = consumption_kWh;
                thing->setStateValue(askoheatTotalEnergyConsumedStateTypeId, this->m_totalEnergyConsumed);
            }

            thing->setStateValue(askoheatConnectedStateTypeId, true);
        }
    }
}

void Askoheat::setDefaultValues(Thing *thing)
{
    qCDebug(dcAskoma()) << "Setting askoheat state values to default.";
    
    quint16 new_status           = 0;
    quint16 new_heaterLoad       = 0;
    quint16 new_heaterStep       = 0;
    qint16 new_loadSetpointValue = 0;
    qint16 new_loadFeedinValue   = 0;
    quint16 new_emergencyMode    = 0;
    quint16 new_heatPumpRequest  = 0;
    float new_currentPower       = 0.0;
    float new_analogInputFloat   = 0.0;
    float new_temperatureSensor0 = 0.0;
    float new_temperatureSensor1 = 0.0;
    float new_temperatureSensor2 = 0.0;
    float new_temperatureSensor3 = 0.0;
    float new_temperatureSensor4 = 0.0;

    if(this->m_status != new_status)
    {
        this->m_status = new_status;
        thing->setStateValue(askoheatStatusStateTypeId, this->m_status);

        thing->setStateValue(askoheatHeater1ActiveStateTypeId, (this->m_status & 1));
        thing->setStateValue(askoheatHeater2ActiveStateTypeId, (this->m_status & 2));
        thing->setStateValue(askoheatHeater3ActiveStateTypeId, (this->m_status & 4));
        thing->setStateValue(askoheatPumpActiveStateTypeId, (this->m_status & 8));
        thing->setStateValue(askoheatValveActiveStateTypeId, (this->m_status & 16));
        thing->setStateValue(askoheatHeaterCurrentFlowStateTypeId, (this->m_status & 32));
        thing->setStateValue(askoheatHeatpumpRequestActiveStateTypeId, (this->m_status & 64));
        thing->setStateValue(askoheatEmergencyModeActiveStateTypeId, (this->m_status & 128));

        thing->setStateValue(askoheatLegionellaProtectionActiveStateTypeId, (this->m_status & 256));
        thing->setStateValue(askoheatAnalogInputActiveStateTypeId, (this->m_status & 512));
        thing->setStateValue(askoheatLoadSetpointActiveStateTypeId, (this->m_status & 1024));
        thing->setStateValue(askoheatLoadFeedinActiveStateTypeId, (this->m_status & 2048));
        thing->setStateValue(askoheatAutoHeaterOffActiveStateTypeId, (this->m_status & 4096));
        thing->setStateValue(askoheatPumpRelayFollowUpTimeActiveStateTypeId, (this->m_status & 8192));
        thing->setStateValue(askoheatTemperatureLimitReachedStateTypeId, (this->m_status & 16384));
        thing->setStateValue(askoheatAnyErrorOccuredStateTypeId, (this->m_status & 32768));
    }
    
    if(this->m_currentPower != new_currentPower)
    {
        this->m_currentPower = new_currentPower;
        thing->setStateValue(askoheatCurrentPowerStateTypeId, this->m_currentPower);
    }
    if(this->m_heaterLoad != new_heaterLoad)
    {
        this->m_heaterLoad = new_heaterLoad;
        thing->setStateValue(askoheatHeaterLoadStateTypeId, this->m_heaterLoad);
    }
    if(this->m_heaterStep != new_heaterStep)
    {
        this->m_heaterStep = new_heaterStep;
        thing->setStateValue(askoheatHeaterStepStateTypeId, this->m_heaterStep);
    }
    if(this->m_loadSetpointValue != new_loadSetpointValue)
    {
        this->m_loadSetpointValue = new_loadSetpointValue;
        thing->setStateValue(askoheatSetpointValueStateTypeId, this->m_loadSetpointValue);
    }
    if(this->m_loadFeedinValue != new_loadFeedinValue)
    {
        this->m_loadFeedinValue = new_loadFeedinValue;
        thing->setStateValue(askoheatHeatingPowerStateTypeId, this->m_loadFeedinValue);
    }
    if(this->m_emergencyMode != new_emergencyMode)
    {
        this->m_emergencyMode = new_emergencyMode;
        thing->setStateValue(askoheatEmergencyModeStateTypeId, this->m_emergencyMode);
    }
    if(this->m_heatPumpRequest != new_heatPumpRequest)
    {
        this->m_heatPumpRequest = new_heatPumpRequest;
        thing->setStateValue(askoheatHeatpumpRequestStateTypeId, this->m_heatPumpRequest); 
    }
    if(this->m_analogInputFloat != new_analogInputFloat)
    {
        this->m_analogInputFloat = new_analogInputFloat;
        thing->setStateValue(askoheatAnalogInputStateTypeId, this->m_analogInputFloat);
    }
    if(this->m_temperatureSensor0 != new_temperatureSensor0)
    {
        this->m_temperatureSensor0 = new_temperatureSensor0;
        thing->setStateValue(askoheatTemperatureSensor0StateTypeId, this->m_temperatureSensor0);
    }
    if(this->m_temperatureSensor1 != new_temperatureSensor1)
    {
        this->m_temperatureSensor1 = new_temperatureSensor1;
        thing->setStateValue(askoheatTemperatureSensor1StateTypeId, this->m_temperatureSensor1);
    }
    if(this->m_temperatureSensor2 != new_temperatureSensor2)
    {
        this->m_temperatureSensor2 = new_temperatureSensor2;
        thing->setStateValue(askoheatTemperatureSensor2StateTypeId, this->m_temperatureSensor2);
    }
    if(this->m_temperatureSensor3 != new_temperatureSensor3)
    {
        this->m_temperatureSensor3 = new_temperatureSensor3;
        thing->setStateValue(askoheatTemperatureSensor3StateTypeId, this->m_temperatureSensor3);
    }
    if(this->m_temperatureSensor4 != new_temperatureSensor4)
    {
        this->m_temperatureSensor4 = new_temperatureSensor4;
        thing->setStateValue(askoheatTemperatureSensor4StateTypeId, this->m_temperatureSensor4);
    }
}