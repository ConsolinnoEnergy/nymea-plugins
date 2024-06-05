/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*
* Copyright 2022 - 2023, Consolinno Energy GmbH
* Contact: info@consolinno.de
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
* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef ASKOHEAT_H
#define ASKOHEAT_H

#include <network/networkaccessmanager.h>
#include <integrations/integrationplugin.h>
#include "extern-plugininfo.h"
#include <QObject>

class Askoheat : public QObject
{
public:
    Askoheat(QString MacAddress, QString HostAddress);
    
    void onGetEMA(QNetworkReply *reply, Thing *thing);
    void onGetPTH(QNetworkReply *reply, Thing *thing);

    QString m_askomaMacAddress = "";
    QString m_askomaHostAddress = "";

    bool m_pthAvailable = false;
    bool m_power = false;
    float m_totalEnergyConsumed = 0.0;
    quint16 m_status = 0;
    quint16 m_heaterLoad = 0;
    quint16 m_heaterStep = 0;
    qint16 m_loadSetpointValue = 0;
    qint16 m_loadFeedinValue = 0;
    quint16 m_emergencyMode = 0;
    quint16 m_heatPumpRequest = 0;
    float m_analogInputFloat = 0.0;
    float m_temperatureSensor0 = 0.0;
    float m_temperatureSensor1 = 0.0;
    float m_temperatureSensor2 = 0.0;
    float m_temperatureSensor3 = 0.0;
    float m_temperatureSensor4 = 0.0;
    QString m_firmwareVersion = "0.0.0";
    QString m_heatingPower = "0";
};

#endif//ASKOHEAT_H