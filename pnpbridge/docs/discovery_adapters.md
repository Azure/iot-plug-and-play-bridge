# Discovery Adapters

[Definition -> Purpose of it]

DISCOVERY_ADAPTER EnvironmentSensorDiscovery = {
    .Identity = "environment-sensor-sample-discovery-adapter",
    .StartDiscovery = EnvironmentSensor_StartDiscovery,
    .StopDiscovery = EnvironmentSensor_StopDiscovery
};

There is only one instance of a discovery adapter create by pnpbridge, which is uniquely identified by .Identity. Here are few of the common properties that apply to the callbacks:

1. The StartDiscovery/StopDiscovery are called sequentially for each adapter i.e If there is adapter A and adapter B, B's StartDiscovery will be called after A's is complete. The order is not guaranteed. 
2. Failure to initialize any adapter is fatal and will cause pnpbridge start failure.
3. If StartDiscovery/StopDiscovery blocks then the pnpbridge will not be able to make forward progress. No adapters should block in these callbacks.
4. If StartDiscovery fails then StopDiscovery is not called.

**.Identity**

Unique string identifier for a discovery adapter

**.StartDiscovery**

This method will be invoked during discovery adapter intialization.

**.StopDiscovery**

This method will be invoked during discovery adapter cleanup.

**DiscoveryAdapter_ReportDevice**

When a new device is discovered, the adapter should call DiscoveryAdapter_ReportDevice API with a PNPMESSAGE for reporting the device to pnpbridge

**Constructing a PNPMESSAGE**

A PNPMESSAGE is a device discovery message that a discovery adapter will report using the DiscoveryAdapter_ReportDevice API. This message is backed by the PNPMEMORY. When the DiscoveryAdapter_ReportDevice returns, the adapter should release reference
on the PNPMESSAGE.

**Per-Device DiscoveryAdapter Parameters**

Every device can provide discovery adapter specific parameters. This could be for static discovery information for a device.

**DiscoveryAdapter Parameters**

There is a per discovery adapter wide parameters that can be specified under root/discovery_adapters/parameters in the json config. The unique identifier is identity and there should be only one instance per adapter identity.

    {
        "identity": "environment-sensor-sample-discovery-adapter",
        "_comment_sensor_id": "Report a dummy device with SensorId 10 to pnp bridge",
        "sensor_id": "10"
    }

**Lifetime of DiscoveryAdapters**

[Picture Needed]

Adapter 1
StartDiscovery -> (DiscoveryAdapter_ReportDevice) -> StopDiscovery

Adapter 2
StartDiscovery -> (DiscoveryAdapter_ReportDevice) -> StopDiscovery

1. The discovery adapters will be initialized when the pnpbridge starts
2. Discovery adapters will be initialized before pnp adapters are initialized
3. Discovery adapter initialization is sequential 
4. The tear down of adapters will occur when any discovery adapter start fails or pnpbridge is shutting down
5. Discovery adapter tear down will happen after pnp adapter tear down

