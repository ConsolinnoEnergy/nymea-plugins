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

#ifndef SIMPLESMARTCONSUMERINTERFACE_H
#define SIMPLESMARTCONSUMERINTERFACE_H

#include <QObject>

#include "gpio.h"

class SimpleSmartconsumerInterface : public QObject
{
    Q_OBJECT
public:
    explicit SimpleSmartconsumerInterface(int gpioNumber, bool inverted, int consumption, QObject *parent = nullptr);

    SgReadyMode sgReadyMode() const;
    bool turnOnDevice(bool turnOn);

    bool setup(bool gpio1Enabled, bool gpio2Enabled);
    bool isValid() const;

    Gpio *gpio() const;

signals:
    void activatedChanged(bool activated);

private:
    bool m_activated = false;
    bool m_inverted = false;
    int m_gpioNumber = -1;
    int m_consumption = -1;

    Gpio *m_gpio = nullptr;

    Gpio *setupGpio(int gpioNumber, bool initialValue);
};

#endif // SIMPLESMARTCONSUMERINTERFACE_H
