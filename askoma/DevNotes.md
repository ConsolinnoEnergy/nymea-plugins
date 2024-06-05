# TODO

## Important
- \[Done\] Fix hems app not updating when registers change.
- \[Done\] Fix hems app not updating on init.
- \[Done\] Parse Status.
- \[Done\] Prevent disconnects caused by a write happening to close to a read.
- \[Done\] Make the logic that handles writes work for multiple heating rods.
- \[Done\] Implement On Off Action.
- \[Done\] Implement Smartmeterconsumer interface.
- Questions for the Askoma Support:.
    - What do we set in which register when the device is "off". -> Load Setpoint 0 Watt.
    - Load setpoint value -> should it be legal for this value to be 0? -> Yes, this means we disconnect the control.
    - What is the order of priorities of different modbus registers? Do modbus registers have higher/lower priority compared to other inputs.
    - As an energymanager - how can we best control, should we use loadFeedinValue at all when we want to manage the rod based on our own calculations -> use loadSetpointValue.
    - What happens to setpoint or feedin value when the other control the rod throught the feedin value and vice versa? Do we have to set e.g. setpoint value to something outside of the valid range (e.g. 0) in order to control the rod with feedinvalue... -> the input resulting in the highest heating power is valid.
    - How often can we write to registers? -> Doesn't matter as the heating rod protects itself.
 
## Nice to have#
- When the python script fails to create a header or cpp file then the build should fail instead of continuing and then silently fail later.
- Find a way to display 9999 degrees Celsius as something like "Sensor not connected".

# Status
## Know Issues
- \[Fixed\] Interface is not updated when values change.
- \[Fixed\] Values in interface are different from the values that are read and logged from the device.

# Docs
## Implementation decisions
- While attempting to assemble all build artifacts in specific folders we gave up trying to create a qMake shadow build and instead ignored the artifacts and folders in the .gitignore file.

# Questions and Issues
- How are Makefiles in subfolders generated?
