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

#include "integrationpluginsimplesmartconsumer.h"
#include "plugininfo.h"

IntegrationPluginSimpleSmartconsumer::IntegrationPluginSimpleSmartconsumer()
{

}

void IntegrationPluginSimpleSmartconsumer::init()
{
    // TODO: Load possible system configurations for gpio pairs depending on well knwon platforms
}

void IntegrationPluginSimpleSmartconsumer::discoverThings(ThingDiscoveryInfo *info)
{
    // Check if GPIOs are available on this platform
    if (!Gpio::isAvailable()) {
        qCWarning(dcSimpleSmartconsumer()) << "There are no GPIOs available on this plattform";
        //: Error discovering SG ready GPIOs
        return info->finish(Thing::ThingErrorHardwareNotAvailable, QT_TR_NOOP("No GPIOs available on this system."));
    }

    // FIXME: provide once system configurations are loaded

    info->finish(Thing::ThingErrorNoError);
}

void IntegrationPluginSimpleSmartconsumer::setupThing(ThingSetupInfo *info)
{
    Thing *thing = info->thing();
    qCDebug(dcSimpleSmartconsumer()) << "Setup" << thing->name() << thing->params();

    // Check if GPIOs are available on this platform
    if (!Gpio::isAvailable()) {
        qCWarning(dcSimpleSmartconsumer()) << "There are no GPIOs available on this plattform";
        //: Error discovering GPIOs
        return info->finish(Thing::ThingErrorHardwareNotAvailable, QT_TR_NOOP("No GPIOs found on this system."));
    }

    if (thing->thingClassId() == simpleSmartconsumerThingClassId) {
        if (m_simpleSmartconsumers.contains(thing)) {
            // Rreconfigure...
            SimpleSmartconsumerInterface *interface = m_simpleSmartconsumers.take(thing);
            delete interface;
            // Continue with normal setup...
        }

        int gpioNumber = thing->paramValue(simpleSmartconsumerThingGpioNumberParamTypeId).toUInt();
        bool inverted = thing->paramValue(simpleSmartconsumerThingInvertedParamTypeId).toBool();
        int consumption = thing->paramValue(simpleSmartconsumerThingConsumptionParamTypeId).toUInt();
        //bool pvSurplus = thing->setting(simpleSmartconsumerSettingsPvSurplusParamTypeId).toBool();
        bool initialValue = thing->stateValue(simpleSmartconsumerPowerStateTypeId).toBool();

        SimpleSmartconsumerInterface *simpleSmartconsumer = new SimpleSmartconsumerInterface(gpioNumber, gpioNumber2, this);(int gpioNumber, bool inverted, int consumption, QObject *parent = nullptr)
        if (!simpleSmartconsumer->setup(gpioEnabled1, gpioEnabled2)) {
            qCWarning(dcSimpleSmartconsumer()) << "Setup" << thing << "failed because the GPIO could not be set up correctly.";
            //: Error message if SG ready GPIOs setup failed
            info->finish(Thing::ThingErrorHardwareFailure, QT_TR_NOOP("Failed to set up the GPIO hardware interface."));
            return;
        }

        // Intially set values according to relais states
        thing->setStateValue(simpleSmartconsumerGpio1StateStateTypeId, simpleSmartconsumer->gpio1()->value() == Gpio::ValueHigh);
        thing->setStateValue(simpleSmartconsumerGpio2StateStateTypeId, simpleSmartconsumer->gpio2()->value() == Gpio::ValueHigh);
        if (  simpleSmartconsumer->sgReadyMode() == SimpleSmartconsumerInterface::SimpleSmartconsumerModeOff ) {
             thing->setStateValue(simpleSmartconsumerSimpleSmartconsumerModeStateTypeId, "Off");
        } else if (simpleSmartconsumer->sgReadyMode() == SimpleSmartconsumerInterface::SimpleSmartconsumerModeLow) {
             thing->setStateValue(simpleSmartconsumerSimpleSmartconsumerModeStateTypeId, "Low");
        } else if (simpleSmartconsumer->sgReadyMode() == SimpleSmartconsumerInterface::SimpleSmartconsumerModeHigh) {
              thing->setStateValue(simpleSmartconsumerSimpleSmartconsumerModeStateTypeId, "High");
        } else {
              thing->setStateValue(simpleSmartconsumerSimpleSmartconsumerModeStateTypeId, "Standard");
        }

        // Reflect the SG states on change
        connect(simpleSmartconsumer, &SimpleSmartconsumerInterface::sgReadyModeChanged, this, [thing, simpleSmartconsumer](SimpleSmartconsumerInterface::SimpleSmartconsumerMode mode){
            Q_UNUSED(mode)
            thing->setStateValue(simpleSmartconsumerGpio1StateStateTypeId, simpleSmartconsumer->gpio1()->value() == Gpio::ValueHigh);
            thing->setStateValue(simpleSmartconsumerGpio2StateStateTypeId, simpleSmartconsumer->gpio2()->value() == Gpio::ValueHigh);
            if ( mode == SimpleSmartconsumerInterface::SimpleSmartconsumerModeOff ) {
               thing->setStateValue(simpleSmartconsumerSimpleSmartconsumerModeStateTypeId, "Off");
            } else if (mode == SimpleSmartconsumerInterface::SimpleSmartconsumerModeLow) {
                thing->setStateValue(simpleSmartconsumerSimpleSmartconsumerModeStateTypeId, "Low");
            } else if (mode == SimpleSmartconsumerInterface::SimpleSmartconsumerModeHigh) {
                thing->setStateValue(simpleSmartconsumerSimpleSmartconsumerModeStateTypeId, "High");
            } else {
                thing->setStateValue(simpleSmartconsumerSimpleSmartconsumerModeStateTypeId, "Standard");
            }
        });
        

        m_simpleSmartconsumers.insert(thing, simpleSmartconsumer);
        info->finish(Thing::ThingErrorNoError);
        return;
    }
}

void IntegrationPluginSimpleSmartconsumer::postSetupThing(Thing *thing)
{
    Q_UNUSED(thing)
}

void IntegrationPluginSimpleSmartconsumer::thingRemoved(Thing *thing)
{
    if (m_simpleSmartconsumers.contains(thing)) {
        SimpleSmartconsumerInterface *simpleSmartconsumer = m_simpleSmartconsumers.take(thing);
        delete simpleSmartconsumer;
    }
}

void IntegrationPluginSimpleSmartconsumer::executeAction(ThingActionInfo *info)
{
    Thing *thing = info->thing();
    if (thing->thingClassId() == simpleSmartconsumerThingClassId) {

        SimpleSmartconsumerInterface *simpleSmartconsumer = m_simpleSmartconsumers.value(thing);
        if (!simpleSmartconsumer || !simpleSmartconsumer->isValid()) {
            qCWarning(dcSimpleSmartconsumer()) << "Failed to execute action. There is no interface available for" << thing;
            info->finish(Thing::ThingErrorHardwareNotAvailable);
            return;
        }

        // FIXME: the modes have timing constrains we need to take care off.

        if (info->action().actionTypeId() == simpleSmartconsumerSimpleSmartconsumerModeActionTypeId) {
            QString sgReadyModeString = info->action().paramValue(simpleSmartconsumerSimpleSmartconsumerModeActionSimpleSmartconsumerModeParamTypeId).toString();
            qCDebug(dcSimpleSmartconsumer()) << "Set SG ready mode from" << thing << "to" << sgReadyModeString;
            SimpleSmartconsumerInterface::SimpleSmartconsumerMode mode;
            if (sgReadyModeString == "Off") {
                mode = SimpleSmartconsumerInterface::SimpleSmartconsumerModeOff;
            } else if (sgReadyModeString == "Low") {
                mode = SimpleSmartconsumerInterface::SimpleSmartconsumerModeLow;
            } else if (sgReadyModeString == "High") {
                mode = SimpleSmartconsumerInterface::SimpleSmartconsumerModeHigh;
            } else {
                mode = SimpleSmartconsumerInterface::SimpleSmartconsumerModeStandard;
            }

            if (!simpleSmartconsumer->setSimpleSmartconsumerMode(mode)) {
                qCWarning(dcSimpleSmartconsumer()) << "Failed to set the sg ready mode on" << thing << "to" << sgReadyModeString;
                info->finish(Thing::ThingErrorHardwareFailure);
                return;
            }

            info->finish(Thing::ThingErrorNoError);
        }
    }
}

