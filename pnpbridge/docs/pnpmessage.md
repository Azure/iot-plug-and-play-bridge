# PnpMessage

A pnpmessage is packet sent from a discovery adapter to pnpbridge to notify a device arrival or removal.

Charactersitics of PnpMessage

1. A PnpMessage is backed by PnpMemory which is ref counted
2. PnpMessage properties allow for specifying a context. This context is an arbitrary pointer and its lifetime should be 
3. When a discovery adapter reports a PnpMessage, it should ideally drop reference on that PnpMessage
4. PnpBridge will take a reference and hold onto the message till a corresponding removal message is sent


