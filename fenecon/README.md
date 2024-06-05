# Fenecon
## FEMS
--------------------------------

This plugin reads data from the FEMS system.
You will receive information about the *status* of the FEMS system,
your production/consumption as well as status of your *battery*, if you have one.
Additionally your *meter* data is being fetched. Primarly showing you if you are feeding in the system,
or receive Power from the grid.
Please ensure that the REST api (Read only is enough) on your FEMS system is enabled.
This should be a default setting.
If not, please ask your installer to enable the REST Api.
If the connection should not work, update your FEMS REST Api configuration,
and set the default port to 8084.


## Supported Things

* Fenecon Home 10?
* Fenecon Home 20 & 30?

## Requirements

* 

## More information

* [Fenecon Dokumente - Modbus/TCP - Lese-/Schreibzugriff](https://docs.fenecon.de/de/_/latest/fems/fems-app/OEM_App_Modbus_TCP.html)