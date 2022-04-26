import nymea
from bimmer_connected.account import ConnectedDriveAccount

accountsMap = {}
vehiclesMap = {}

pollTimer = None

def init():
    logger.log("Initializing Bimmerconnected plugin")


def startPairing(info):
    info.finish(nymea.ThingErrorNoError)


def confirmPairing(info, username, secret):
    try:
        account = ConnectedDriveAccount(username, secret, Regions.REST_OF_WORLD)
        account.update_vehicle_states();
        info.finish(nymea.ThingErrorNoError)
        pluginStorage().beginGroup(info.thingId)
        pluginStorage().setValue("username", username)
        pluginStorage().setValue("password", secret)
        pluginStorage().endGroup();
        del account

    except Exception as e:
        logger.warn("Error setting up BMW account:", str(e))
        info.finish(nymea.ThingErrorAuthenticationFailure)

def setupThing(info):
    # Setup for the account
    if info.thing.thingClassId == accountThingClassId:
        logger.log("SetupThing for account:", info.thing.name)

        pluginStorage().beginGroup(info.thing.id)
        username = pluginStorage().value("username")
        password = pluginStorage().value("password");
        pluginStorage().endGroup()

        try:
            account = ConnectedDriveAccount(username, secret, Regions.REST_OF_WORLD)
            account.update_vehicle_states();
            # Login went well, finish the setup
            info.finish(nymea.ThingErrorNoError)
        except Exception as e:
            # Login error
            logger.warn("Error setting up BMW account:", str(e))
            info.finish(nymea.ThingErrorAuthenticationFailure, str(e))
            return

        # Mark the account as logged-in and connected
        info.thing.setStateValue(accountLoggedInStateTypeId, True)
        info.thing.setStateValue(accountConnectedStateTypeId, True)

        accountsMap[info.thing] = account

        logger.log('Found {} vehicles: {}'.format(
                   len(account.vehicles),
                   ','.join([v.name for v in account.vehicles])))

        thingDescriptors = []
        for vehicle in account.vehicles:
            logger.log('VIN: {}'.format(vehicle.vin))
            logger.log('Mileage: {}'.format(vehicle.status.mileage))
            logger.log('Vehicle data:')
            logger.log(to_json(vehicle, indent=4))

            found = False
            for thing in myThings():
                if thing.thingClassId == vehicleThingClassId and thing.paramValue(vehicleThingVinParamTypeId) == vehicle.vin:
                    found = True
                    break;
            if found:
                continue

            logger.log("Adding new vehicle to the system with parent", info.thing.id)
            thingDescriptor = nymea.ThingDescriptor(vehicleThingClassId, "BMW {} ({})".format(vehicle.name, vehicle.vin[-7:]), parentId=info.thing.id)
            thingDescriptor.params = [
                nymea.Param(vehicleThingVinParamTypeId, vehicle.vin)
            ]
            thingDescriptors.append(thingDescriptor)

        autoThingsAppeared(thingDescriptors)

        # If no poll timer is set up yet, start it now
        logger.log("Creating polltimer @ setupThing")
        global pollTimer
        if pollTimer is None:
            pollTimer = nymea.PluginTimer(60 * 5, pollService)
            logger.log("timer interval @ setupThing", pollTimer.interval)
        return

    # Setup for the vehicles
    if info.thing.thingClassId == vehicleThingClassId:
        logger.log("SetupThing for vehicle:", info.thing.name)

        vehiclesMap[info.thing.paramValue(vehicleThingVinParamTypeId)] = info.thing

        thing.setStateValue(vehicleBatteryLevelStateTypeId, vehicle.properties.chargingState.chargePercentage)
        thing.setStateValue(vehiclePluggedInStateTypeId, vehicle.properties.chargingState.isChargerConnected)
        thing.setStateValue(vehicleChargingStateStateTypeId, "charging" if vehicle.properties.chargingState.state == "CHARGING" else "idle")

        info.finish(nymea.ThingErrorNoError)


def pollService():
    logger.log("Polling BMW Connect")
    for thing in myThings():
        if thing.thingClassId == accountThingClassId:
            try:
                account.update_vehicle_states();
            except:
                logger.warn("Error refreshing vehicle states for account", thing.name)

    for vehicle in account.vehicles:
        logger.log("updating %s" % vehicle.vin)
        if vehicle.vin not in vehiclesMap:
            thingDescriptor = nymea.ThingDescriptor(vehicleThingClassId, "BMW {} ({})".format(vehicle.name, vehicle.vin[-7:]), parentId=info.thing.id)
            thingDescriptor.params = [
                nymea.Param(vehicleThingVinParamTypeId, vehicle.vin)
            ]
            thingDescriptors.append(thingDescriptor)
            autoThingsAppeared(thingDescriptors)
            continue;

        thing = vehiclesMap[vehicle.vin]
        thing.setStateValue(vehicleBatteryLevelStateTypeId, vehicle.properties.chargingState.chargePercentage)
        thing.setStateValue(vehiclePluggedInStateTypeId, vehicle.properties.chargingState.isChargerConnected)
        thing.setStateValue(vehicleChargingStateStateTypeId, "charging" if vehicle.properties.chargingState.state == "CHARGING" else "idle")


def thingRemoved(thing):
    global pollTimer
    del vehiclesMap[thing.paramValue(vehicleThingVinParamTypeId)]
    if len(myThings()) == 0 and pollTimer is not None:
        pollTimer = None
