# Common helper utilities for PnP Sample

This directory contains helper routines used by the bridge to implement PnP functionality when using the Azure IoT Hub SDK.

For reference the files are:

* `pnp_device_client` header and .c file implement a function to help create a `IOTHUB_DEVICE_CLIENT_HANDLE` and `IOTHUB_MODULE_CLIENT_HANDLE`.  The `IOTHUB_DEVICE_CLIENT_HANDLE` and `IOTHUB_MODULE_CLIENT_HANDLE` are client handles for IoTHub device or module that can be used for device/module <=> IoTHub communication.

    For IoT devices, creating a `IOTHUB_DEVICE_CLIENT_HANDLE` that works with PnP requires a few additional steps, most importantly defining the device's PnP ModelId, which is
    defined by the PnP Bridge's Root Interface Model Id and retrieved from the configuration file.

    For IoT edge modules, creating a `IOTHUB_MODULE_CLIENT_HANDLE` that works with PnP requires a few additional steps, including:
    - Populating the device's PnP ModelId, which is
    defined by the PnP Bridge's Root Interface Model Id and retrieved from the edge runtime's environment through an environment variable `PNP_BRIDGE_ROOT_MODEL_ID`.
    - Populating the edge module's connection string retrieved from the edge runtime's environment through an environment variable `IOTHUB_DEVICE_CONNECTION_STRING`.
    - Enabling or disabling IoT hub tracing by retrieving this information from the edge runtime's environment through an environment variable `PNP_BRIDGE_HUB_TRACING_ENABLED`.

* `pnp_dps` header and .c file implement functions to help provision a device handle through the device provisioning service.

* `pnp_protocol` header and .c file implement functions to help with serializing and de-serializing the PnP convention. As an example of their usefulness, PnP properties are sent between the device and IoTHub using a specific JSON convention over the device twin. Functions in this header perform some of the tedious parsing and JSON string generation that is offloaded from the PnP Bridge.

    The functions are agnostic to the underlying transport handle used.   The `pnp_protocol` logic does not need to change for `IOTHUB_DEVICE_CLIENT_LL_HANDLE`, `IOTHUB_MODULE_CLIENT_HANDLE`, `IOTHUB_MODULE_CLIENT_LL_HANDLE` or `IOTHUB_DEVICE_CLIENT_HANDLE`

* `pnp_bridge_client` header defines wrapper functions around IoTHub SDK APIs tp bifurcate device and module client calls at compilation. This also allows for easy development and maintenance of PnP Bridge adapters