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

#include "simplesmartconsumerinterface.h"
#include "extern-plugininfo.h"

SimpleSmartconsumerInterface::SimpleSmartconsumerInterface(bool inverted, QObject *parent) :
    QObject{parent},
    m_inverted{inverted}
{

}

bool SimpleSmartconsumerInterface::turnOnDevice(bool turnOn)
{
    if (!isValid())
        return false;

    bool toGpio{turnOn};
    if (m_inverted) {
        toGpio = !turnOn;
    }

    if (!m_gpio->setValue(toGpio ? Gpio::ValueHigh : Gpio::ValueLow)) {
        qCWarning(dcSimpleSmartconsumer()) << "Could not switch GPIO";
        return false;
    }

    emit activatedChanged(turnOn);

    return true;
}

bool SimpleSmartconsumerInterface::setup(int gpioNumber, bool gpioEnabled)
{
    if (gpioNumber < 0) {
        m_gpio = nullptr;
        return false;
    }
    bool toGpio{gpioEnabled};
    if (m_inverted) {
        toGpio = !gpioEnabled;
    }
    m_gpio = setupGpio(gpioNumber, toGpio);
    if (!m_gpio) {
        qCWarning(dcSimpleSmartconsumer()) << "Failed to set up GPIO" << gpioNumber;
        return false;
    }

    emit activatedChanged(gpioEnabled);

    return true;
}

bool SimpleSmartconsumerInterface::isValid() const
{
    if (m_gpio) {
        return true;
    }
    return false;
}

Gpio *SimpleSmartconsumerInterface::setupGpio(int gpioNumber, bool initialValue)
{
    if (gpioNumber < 0) {
        qCWarning(dcSimpleSmartconsumer()) << "Invalid gpio number for setting up gpio" << gpioNumber;
        return nullptr;
    }

    // Create and configure gpio
    Gpio *gpio = new Gpio(gpioNumber, this);
    if (!gpio->exportGpio()) {
        qCWarning(dcSimpleSmartconsumer()) << "Could not export gpio" << gpioNumber;
        gpio->deleteLater();
        return nullptr;
    }

    if (!gpio->setDirection(Gpio::DirectionOutput)) {
        qCWarning(dcSimpleSmartconsumer()) << "Failed to configure gpio" << gpioNumber << "as output";
        gpio->deleteLater();
        return nullptr;
    }

    // Set the pin to the initial value
    if (!gpio->setValue(initialValue ? Gpio::ValueHigh : Gpio::ValueLow)) {
        qCWarning(dcSimpleSmartconsumer()) << "Failed to set initial value" << initialValue << "for gpio" << gpioNumber;
        gpio->deleteLater();
        return nullptr;
    }

    return gpio;
}
