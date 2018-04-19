/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                                         *
 *  Copyright (C) 2018 Bernhard Trinnes <bernhard.trinnes@guh.io>          *
 *                                                                         *
 *  This file is part of nymea.                                            *
 *                                                                         *
 *  This library is free software; you can redistribute it and/or          *
 *  modify it under the terms of the GNU Lesser General Public             *
 *  License as published by the Free Software Foundation; either           *
 *  version 2.1 of the License, or (at your option) any later version.     *
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

#ifndef DEVICEPLUGINDWEETIO_H
#define DEVICEPLUGINDWEETIO_H

#include "devicemanager.h"
#include "plugin/deviceplugin.h"

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QHostAddress>
#include <QNetworkReply>

#include "plugintimer.h"

class DevicePluginDweetio : public DevicePlugin
{
    Q_OBJECT

    Q_PLUGIN_METADATA(IID "io.nymea.DevicePlugin" FILE "deviceplugindweetio.json")
    Q_INTERFACES(DevicePlugin)

public:
    explicit DevicePluginDweetio();
    ~DevicePluginDweetio();

    void init() override;
    DeviceManager::DeviceSetupStatus setupDevice(Device *device) override;
    void deviceRemoved(Device *device) override;
    DeviceManager::DeviceError executeAction(Device *device, const Action &action) override;
    void postSetupDevice(Device *device) override;

private:
    PluginTimer *m_pluginTimer = nullptr;
    QHash<QNetworkReply *, Device *> m_postReplies;
    QHash<QNetworkReply *, Device *> m_getReplies;

    QHash<QNetworkReply *, ActionId> m_asyncActions;

    void postContent(const QString &content, Device *device, const Action &action);
    void processPostReply(const QVariantMap &data, Device *device);
    void processGetReply(const QVariantMap &data, Device *device);
    void setConnectionStatus(bool status, Device *device);

    void getRequest(Device *device);
private slots:
    void onNetworkReplyFinished();
    void onPluginTimer();
};

#endif // DEVICEPLUGINDWEETIO_H