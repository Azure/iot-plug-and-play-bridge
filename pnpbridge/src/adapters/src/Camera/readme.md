# Sample Camera Adapter

## Create Discovery Adapter
To enable the Camera Adapter for this preview, the Discovery Adapter will need to be declared. A sample of this is located in CameraIotPnpAPIs.cpp.  The sample uses the camera-health-monitor identity.

```C
DISCOVERY_ADAPTER CameraPnpAdapter = {
    "camera-health-monitor",
    CameraPnpStartDiscovery,
    CameraPnpStopDiscovery
};
```

And enable the discovery by adding the CameraPnpAdapter to the Discovery Manifest:

```C
PDISCOVERY_ADAPTER DISCOVERY_ADAPTER_MANIFEST[] = {
    &CameraPnpAdapter
};
```

## Create PnP Adapter
Next step is to declare a PnP Adapter, also declared in CameraIotPnpAPIs.cpp:

```C
PNP_ADAPTER CameraPnpInterface = {
    "camera-health-monitor",
    CameraPnpInterfaceInitialize,
    CameraPnpInterfaceBind,
    CameraPnpInterfaceRelease,
    CameraPnpInterfaceShutdown
};
```

Similarly, enable the PnP Adapter by adding the CameraPnpInterface declaration to the PnP Adapter Manifest:

```C
PPNP_ADAPTER PNP_ADAPTER_MANIFEST[] = {
    &CameraPnpInterface
};
```

## Configuration
Once the Discovery and PnP Adapters have been declared in the respective manifests, the configuration json must be updated to describe the camera device.

```JSON
  "Devices": [
    {
      "_comment": "Standard Video Camera connected to a PC, for this sample, only one camera may be connected.",
      "MatchFilters": {
        "MatchType": "Exact",
        "MatchParameters": {
          "HardwareId": "UVC_Webcam_00"
        }
      },
      "InterfaceId": "http://windows.com/camera_health_monitor/1.0.0",
      "PnpParameters": {
        "Identity": "camera-health-monitor"
      }
    }
  ],
  "DiscoveryAdapters": {
    "EnabledAdapters": {
    },
    "Parameters": [
      {
        "Identity": "camera-health-monitor",
        "_comment1": "Standard Video Camera"
      }
    ]
  },
  "PnpAdapters": {
    "EnabledAdapters": {
    },
    "Parameters": [
      {
        "Identity": "camera-health-monitor",
        "_comment1": "Standard Video Camera"
      }
    ]
  }

```
The sample JSON configuration above describes a single camera connected to the PC and the HardwareId is hardcoded to be UVC_Webcam_00.

## Supported Features
- When the PnPBridge with the Sample Camera Adapter is executed on Windows 10 version 19H1 and later builds, camera health telemetry will be published every 15 seconds.
- Take photo operation is supported in the Sample Camera Adapter for any version of Windows 10 RS5 and later.
    - Photo operation can execute concurrently with other camera operations since the sample opens the camera in shared mode (shared mode allows concurrent access to the camera with other applications/processes).

## Limitations / Assumptions
- Only one camera must be connected to the PC.  The HardwareId is hardcoded to a fake device Id (UVC_Webcam_00).
- Only the camera health statistics from the first active stream is tracked.
- For photo uploads, a blob storage is assumed to have been created and provisioned.