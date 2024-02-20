/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*
* Copyright 2013 - 2021, nymea GmbH
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

#include "integrationpluginemh.h"
#include "plugininfo.h"
#include <iostream>

IntegrationPluginEmh::IntegrationPluginEmh()
{
    
}

void IntegrationPluginEmh::init()
{
    qCDebug(dcEMH()) << "Plugin initialized.";
}

void IntegrationPluginEmh::discoverThings(ThingDiscoveryInfo *info)
{
    qCDebug(dcEMH()) << "Discovering EMH devices";

    // init curl
    curl = curl_easy_init();

    // set base url
    std::string emhSmgwIp = info->params().paramValue(emhHanInterfaceDiscoveryIpSMGWParamTypeId).toString().toStdString();
    std::string baseUrl = "https://" + emhSmgwIp + ":443/json";

    // set digest auth params
    std::string digestUser = info->params().paramValue(emhHanInterfaceDiscoveryDigestUserParamTypeId).toString().toStdString();
    std::string digestPass = info->params().paramValue(emhHanInterfaceDiscoveryDigestPassParamTypeId).toString().toStdString();

    // send request
    std::string strInformationResponse;
    CURLcode res = sendCurlRequest(baseUrl, "/systeminformation", digestUser, digestPass, strInformationResponse, statusCode);

    if (res == CURLE_OK) {
        Json::Value jsonInformationResponse = responseStringToJson(strInformationResponse);
        // get smgw id from response
        std::string smgwID = jsonInformationResponse.get("huid", "unknown").asString();
        std::string userID = jsonInformationResponse.get("user-id", "unknown").asString();

        if (smgwID == "unknown" || userID == "unknown") {
            //discovery not succesful
            QString errMes = QString::fromStdString("EMH SMGW not responding correctly (response: "+strInformationResponse+")");
            qCWarning(dcEMH()) << errMes;
            info->finish(Thing::ThingErrorHardwareNotAvailable, QT_TR_NOOP(errMes));
        }
        else {
            // request meter list
            std::string strMeterListResponse;
            CURLcode meterRes = sendCurlRequest(baseUrl, "/metering/origin", digestUser, digestPass, strMeterListResponse, statusCode);
            if (meterRes == CURLE_OK) {
                // check if there are meters present
                Json::Value jsonMeterListResponse = responseStringToJson(strMeterListResponse);
                if (jsonMeterListResponse.empty()) {
                    // no meters are connected to the SMGW -> discovery not succesful
                    QString errMes = QString::fromStdString("No Meters are connected/configured at SMGW " + smgwID + " for user ID " + userID);
                    qCWarning(dcEMH()) << errMes;
                    info->finish(Thing::ThingErrorHardwareNotAvailable, QT_TR_NOOP(errMes));
                }
                else {
                    // discovery succesful
                    ParamList thingParams;
                    thingParams << Param(emhHanInterfaceThingDigestUserParamTypeId, info->params().paramValue(emhHanInterfaceDiscoveryDigestUserParamTypeId));
                    thingParams << Param(emhHanInterfaceThingDigestPassParamTypeId, info->params().paramValue(emhHanInterfaceDiscoveryDigestPassParamTypeId));
                    thingParams << Param(emhHanInterfaceThingIpSMGWParamTypeId, info->params().paramValue(emhHanInterfaceDiscoveryIpSMGWParamTypeId));
                    
                    thingParams << Param(emhHanInterfaceThingSmgwIDParamTypeId, QString::fromStdString(smgwID));
                    thingParams << Param(emhHanInterfaceThingUserIDParamTypeId, QString::fromStdString(userID));
                    // extract meters and build discovery objects per meter
                    for (const auto& meterID : jsonMeterListResponse) {
                        ThingDescriptor descriptor (
                            info->thingClassId(),
                            QString::fromStdString(meterID.asString()),
                            QString::fromStdString("connected via SMGW " + smgwID)
                        );
                        thingParams << Param(emhHanInterfaceThingMeterIDParamTypeId, QString::fromStdString(meterID.asString()));
                        descriptor.setParams(thingParams);
                        info->addThingDescriptor(descriptor);
                    }
                    info->finish(Thing::ThingErrorNoError);
                }
            }
            else {
                // discovery not succesful
                std::string curlError = curl_easy_strerror(res);
                QString errMes = QString::fromStdString(
                    "Meters cannot be read out: " 
                    + curlError 
                    + "[curlCode: "
                    + std::to_string(res)
                    + ", statusCode: "
                    + std::to_string(statusCode)
                    + "]");
                qCWarning(dcEMH()) << errMes;
                return info->finish(Thing::ThingErrorHardwareNotAvailable, QT_TR_NOOP(errMes));
            }
        }
        
    }
    else {
        // catch certain status codes (e.g. 401)
        if (statusCode == 401) {
            QString errMes = QString::fromStdString(
                "Wrong username and/or password: [statusCode: "
                + std::to_string(statusCode)
                + " unauthorized]");
            qCWarning(dcEMH()) << errMes;
            return info->finish(Thing::ThingErrorHardwareNotAvailable, QT_TR_NOOP(errMes));
        }
        // discovery not succesful, for curl error codes see: https://curl.se/libcurl/c/libcurl-errors.html
        std::string curlError = curl_easy_strerror(res);
        QString errMes = QString::fromStdString(
            "EMH SMGW is not reachable: " 
            + curlError 
            + "[curlCode: "
            + std::to_string(res)
            + ", statusCode: "
            + std::to_string(statusCode)
            + "]");
        qCWarning(dcEMH()) << errMes;
        info->finish(Thing::ThingErrorHardwareNotAvailable, QT_TR_NOOP(errMes));
    }

}

void IntegrationPluginEmh::setupThing(ThingSetupInfo *info)
{
    Thing *thing = info->thing();
    qCDebug(dcEMH()) << "SetupThing" << thing->name() << thing->params();
    // poll every second
	m_timer = hardwareManager()->pluginTimerManager()->registerTimer(1);

    // set up curl
    curl = curl_easy_init();

    if (thing->thingClassId() == emhHanInterfaceThingClassId) {
        connect(m_timer, &PluginTimer::timeout, thing, [this, thing](){

            // set base url
            std::string emhSmgwIp = thing->paramValue(emhHanInterfaceThingIpSMGWParamTypeId).toString().toStdString();
            std::string baseUrl = "https://" + emhSmgwIp + ":443/json";

            // set digest auth params
            std::string digestUser = thing->paramValue(emhHanInterfaceThingDigestUserParamTypeId).toString().toStdString();
            std::string digestPass = thing->paramValue(emhHanInterfaceThingDigestPassParamTypeId).toString().toStdString();

            // set meter ID 
            std::string meterID = thing->paramValue(emhHanInterfaceThingMeterIDParamTypeId).toString().toStdString();
            
            // send request
            std::string strMeteringResponse;
            CURLcode res = sendCurlRequest(baseUrl, "/metering/origin/" + meterID, digestUser, digestPass, strMeteringResponse, statusCode);

            if (res == CURLE_OK) {
                // set connected to true
                thing->setStateValue(emhHanInterfaceConnectedStateTypeId, true);
                // parse metering values
                Json::Value jsonMeteringResponse = responseStringToJson(strMeteringResponse);

                if (!jsonMeteringResponse.empty()) {
                    std::string power = jsonMeteringResponse.get("0100100700ff", "-1 W").asString();
                    thing->setStateValue(emhHanInterfaceCurrentPowerStateTypeId, stod(power.erase(power.find(" W"), 2)));

                    std::string energyConsumed = jsonMeteringResponse.get("0100010800ff", "-1 kWh").asString();
                    thing->setStateValue(emhHanInterfaceTotalEnergyConsumedStateTypeId, stod(energyConsumed.erase(energyConsumed.find(" kWh"), 4)));

                    std::string energyProduced = jsonMeteringResponse.get("0100020800ff", "-1 kWh").asString();
                    thing->setStateValue(emhHanInterfaceTotalEnergyProducedStateTypeId, stod(energyProduced.erase(energyProduced.find(" kWh"), 4)));

                    //info->finish(Thing::ThingErrorNoError);
                }
            }
            else {
                // device not rechable, set connected to false
                thing->setStateValue(emhHanInterfaceConnectedStateTypeId, false);
                std::string curlError = curl_easy_strerror(res);
                QString errMes = QString::fromStdString(
                    "Issue while reading metering values: " 
                    + curlError 
                    + "[curlCode: "
                    + std::to_string(res)
                    + ", statusCode: "
                    + std::to_string(statusCode)
                    + "]");
                qCWarning(dcEMH()) << errMes;
                //info->finish(Thing::ThingErrorHardwareNotAvailable, QT_TR_NOOP(errMes));
            }
        });

        info->finish(Thing::ThingErrorNoError);
    }

}

void IntegrationPluginEmh::postSetupThing(Thing *thing)
{
    Q_UNUSED(thing)
}

void IntegrationPluginEmh::thingRemoved(Thing *thing)
{
    qCDebug(dcEMH()) << "thingRemoved : Remove thing" << thing;

    curl_easy_cleanup(curl);

	if (m_timer) {
        qCDebug(dcEMH()) << "Stopping refresh timer";
        hardwareManager()->pluginTimerManager()->unregisterTimer(m_timer);
        m_timer = nullptr;
    }
}

Json::Value IntegrationPluginEmh::responseStringToJson(std::string response) {
    Json::Value jsonReponse;
    Json::CharReaderBuilder builder;
    Json::CharReader* reader = builder.newCharReader();
    
    std::string errs;
    bool parsingSuccessful = reader->parse(response.c_str(), response.c_str() + response.size(), &jsonReponse, &errs);
    delete reader;

    if (!parsingSuccessful) {
        std::cerr << "Failed to parse JSON: " << errs << std::endl;
    } //else {
        //std::cout << "Parsed JSON response succesfully" << std::endl;
    //}
    return jsonReponse;
}

CURLcode IntegrationPluginEmh::sendCurlRequest(std::string baseUrl, std::string resource, std::string digestUser, std::string digestPass, std::string &response, long &statusCode) {

    //std::cout << "url: " << baseUrl << resource << std::endl;
    //std::cout << "user/pw: " << digestUser << digestPass << std::endl;

    // set http timeout of 5 seconds
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);

    // do not verify ssl certificates
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);

    // consider status codes >=400 as request fails and do not return CURLE_OK 
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);

    // set request url
    std::string requestUrl = baseUrl + resource;
    curl_easy_setopt(curl, CURLOPT_URL, requestUrl.c_str());
    
    // set response handling
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    // set digest auth params
    curl_easy_setopt(curl, CURLOPT_USERNAME, digestUser.c_str());
    curl_easy_setopt(curl, CURLOPT_PASSWORD, digestPass.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_DIGEST);
    // send request 
    CURLcode res = curl_easy_perform(curl);

    // get status code
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &statusCode);

    return res;
}

size_t writeCallback(char *ptr, size_t size, size_t nmemb, std::string *data) {
    data->append(ptr, size * nmemb);
    return size * nmemb;
}