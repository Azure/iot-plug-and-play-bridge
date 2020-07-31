# Sample Camera Adapter

## Create PnP Adapter
Declare a PnP Adapter, also declared in CameraPnPBridgeInterface.cpp and implement the interfaces:

```C
PNP_ADAPTER CameraPnpInterface = {
    "camera-pnp-adapter",         // identity
    Camera_CreatePnpAdapter,      // createAdapter
    Camera_CreatePnpComponent,    // createPnpComponent
    Camera_StartPnpComponent,     // startPnpComponent
    Camera_StopPnpComponent,      // stopPnpComponent
    Camera_DestroyPnpComponent,   // destroyPnpComponent
    Camera_DestroyPnpAdapter      // destroyAdapter
};
```

Enable the PnP Adapter by adding the CameraPnpInterface declaration to the PnP Adapter Manifest:

```C
PPNP_ADAPTER PNP_ADAPTER_MANIFEST[] = {
    &CameraPnpInterface
};
```

## Configuration
Once the PnP Bridge Adapter has been declared in the respective manifests, the configuration json must be updated to describe the camera device.

```JSON
{
  "pnp_bridge_interface_components": [
    {
      "_comment": "Component - Standard Video Camera connected to a PC, for now the first camera only.",
          "pnp_bridge_component_name": "CameraComponent",
          "pnp_bridge_adapter_id": "camera-pnp-adapter",
          "pnp_bridge_adapter_config": {
              "hardware_id": "UVC_Webcam_00"
          }
    }
  ]
}

```

The sample JSON configuration above describes a single camera connected to the PC and the HardwareId is hardcoded to be UVC_Webcam_00.

## Supported Features

- When the PnPBridge with the Sample Camera Adapter is executed on Windows 10 version 19H1 and later builds, camera PnP telemetry will be published every 15 seconds.
- Take photo operation is supported in the Sample Camera Adapter for any version of Windows 10 RS5 and later.
    - Photo operation can execute concurrently with other camera operations since the sample opens the camera in shared mode (shared mode allows concurrent access to the camera with other applications/processes).

## Limitations / Assumptions

- Only one camera must be connected to the PC.  The HardwareId is hardcoded to a fake device Id (UVC_Webcam_00).
- Only the camera PnP statistics from the first active stream is tracked.
- For photo uploads, a blob storage is assumed to have been created and provisioned.