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

SimpleSmartconsumerInterface::SimpleSmartconsumerInterface(int gpioNumber, QObject *parent) :
    QObject(parent),
    m_gpioNumber(gpioNumber)
{

}

SgReadyInterface::SgReadyMode SgReadyInterface::sgReadyMode() const
{
    return m_sgReadyMode;
}

bool SimpleSmartconsumerInterface::turnOnDevice(bool turnOn)
{
    if (!isValid())
        return false;

    /* https://www.waermepumpe.de/normen-technik/sg-ready/
     *
     * Off: 1,0
     * Low: 0,0
     * Standard: 0,1
     * High: 1,1
     */

    QPair<bool, bool> gpioSettings;
    switch (sgReadyMode) {
    case SgReadyModeOff:
        gpioSettings.first = true;
        gpioSettings.second = false;
        break;
    case SgReadyModeLow:
        gpioSettings.first = false;
        gpioSettings.second = false;
        break;
    case SgReadyModeStandard:
        gpioSettings.first = false;
        gpioSettings.second = true;
        break;
    case SgReadyModeHigh:
        gpioSettings.first = true;
        gpioSettings.second = true;
        break;
    }

    if (!m_gpio1->setValue(gpioSettings.first ? Gpio::ValueHigh : Gpio::ValueLow)) {
        qCWarning(dcSimpleSmartconsumer()) << "Could not switch GPIO 1 for setting" << sgReadyMode;
        return false;
    }

    if (!m_gpio2->setValue(gpioSettings.second ? Gpio::ValueHigh : Gpio::ValueLow)) {
        qCWarning(dcSimpleSmartconsumer()) << "Could not switch GPIO 2 for setting" << sgReadyMode;
        return false;
    }

    if (m_sgReadyMode != sgReadyMode) {
        m_sgReadyMode = sgReadyMode;
        emit sgReadyModeChanged(m_sgReadyMode);
    }

    return true;
}

bool SimpleSmartconsumerInterface::setup(bool gpioEnabled)
{
    m_gpio = setupGpio(m_gpioNumber, gpioEnabled);
    if (!m_gpio) {
        qCWarning(dcSgReady()) << "Failed to set up SG ready interface gpio 1" << m_gpioNumber1;
        return false;
    }

    emit activatedChanged(m_sgReadyMode);

    return true;
}

bool SimpleSmartconsumerInterface::isValid() const
{
    return m_gpioNumber1 >= 0 && m_gpioNumber2 >= 0 && m_gpio1 && m_gpio2;
}

Gpio *SimpleSmartconsumerInterface::gpio() const
{
    return m_gpio;
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
