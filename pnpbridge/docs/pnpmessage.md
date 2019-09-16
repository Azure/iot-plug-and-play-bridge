# PnpMessage

A PNPMESSAGE is a device discovery message that a discovery adapter will report to the Bridge using the DiscoveryAdapter_ReportDevice. 
The discovery adapter should use PnpMessage_CreateMessage to allocate this message followed by PnpMessage_SetMessage to set the message payload.

Charactersitics of PnpMessage

1. A PnpMessage is backed by PnpMemory which is ref counted
2. PnpMessage properties allow for specifying a context. This context is an arbitrary pointer and its lifetime should be 
3. When a discovery adapter reports a PnpMessage, it should ideally drop reference on that PnpMessage
4. PnpBridge will take a reference and hold onto the message till a corresponding removal message is sent


