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

#ifndef INTEGRATIONPLUGINEMH_H
#define INTEGRATIONPLUGINEMH_H

#include "integrations/integrationplugin.h"
#include <jsoncpp/json/json.h>
#include <curl/curl.h>
#include <plugintimer.h>

class IntegrationPluginEmh : public IntegrationPlugin
{
    Q_OBJECT

    Q_PLUGIN_METADATA(IID "io.nymea.IntegrationPlugin" FILE "integrationpluginemh.json")
    Q_INTERFACES(IntegrationPlugin)

public:
    explicit IntegrationPluginEmh();

    void init() override;
    void discoverThings(ThingDiscoveryInfo *info) override;
    void setupThing(ThingSetupInfo *info) override;
    void postSetupThing(Thing *thing) override;
    void thingRemoved(Thing *thing) override;

private:

    PluginTimer *m_timer = nullptr;
    CURL *curl;
    //std::string baseUrl;
    long statusCode;

    Json::Value responseStringToJson(std::string response);
    CURLcode sendCurlRequest(std::string baseUrl, std::string resource, std::string digestUser, std::string digestPass, std::string &response, long &statusCode);

};

size_t writeCallback(char *ptr, size_t size, size_t nmemb, std::string *data);

#endif // INTEGRATIONPLUGINEMH_H
