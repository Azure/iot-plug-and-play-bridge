---
platform: linux
device: Impinj R700
language: {}
---

Connect Impinj R700 device to your Azure IoT services
===

---
# Table of Contents

- [Connect Impinj R700 device to your Azure IoT services](#connect-impinj-r700-device-to-your-azure-iot-services)
- [Table of Contents](#table-of-contents)
- [Introduction](#introduction)
- [Prerequisites](#prerequisites)
- [Preparing the Impinj R700 Reader](#preparing-the-impinj-r700-reader)
- [Installing PnP Bridge Application on the Impinj R700 Reader](#installing-pnp-bridge-application-on-the-impinj-r700-reader)
- [Connecting the PnP Bridge Application to Azure IoT](#connecting-the-pnp-bridge-application-to-azure-iot)
- [Building, Deploying, and Extending the IoT Plug and Play Bridge](#building-deploying-and-extending-the-iot-plug-and-play-bridge)
- [Additional Links](#additional-links)

<a name="Introduction"></a>

# Introduction 

[Impinj R700 - RAIN RFID Fixed Reader](https://www.impinj.com/products/readers/impinj-r700) 

The Impinj R700 RAIN RFID fixed reader provides industry-leading performance, enterprise reliability and security, and modern developer tools. IoT developers can easily build and deploy custom enterprise applications with a Linux OS, REST API, and native support for industry-standard data formats and protocols, such as MQTT. The Impinj R700 delivers increased support for on-reader applications. When combined with tags based on [Impinj M700 series tag chips](https://www.impinj.com/products/tag-chips/impinj-m700-series), the Impinj R700 advances RAIN RFID performance at dock doors, conveyors, and store exits. 

The Impinj R700 reader builds on the heritage of the [Impinj Speedway reader family](https://www.impinj.com/products/readers/impinj-speedway), which has been proven reliable for 15 years in the field. 
 
IoT Plug and Play certified device simplifies the process of building devices without custom device code. Using Solution builders can integrate quickly using the certified IoT Plug and Play enabled device based on Azure IoT Central as well as third-party solutions.

This getting started guide provides step by step instruction on getting an Impinj R700 device online within minutes and feeding data into Azure IoT Hub, Azure IoT Central, Azure Events Hub, Azure Storage, Power BI  or any other 3rd party platform.

<a name="Prerequisites"></a>
# Prerequisites

You should have the following items ready before beginning the process:

**Hardware**
-   [Impinj R700 fixed reader](https://docs.microsoft.com/en-us/azure/iot-hub/about-iot-hub)
-   Power over Ethernet (PoE/PoE+) switch or supply
-   Ethernet cables necessary to connect Impinj R700 to network/power
-   Network connection with internet access
-   PC on same local network as Impinj R700
-   (For reading tags) antenna & connective cabling
-   (For serial SSH access - optional) USB B micro to USB A cable

**For Configuring Impinj R700**
-   FTP Client (ex: FileZilla)
-   SSH Client (ex: PuTTY)
-   Text Editor (ex: Notepad++)

**For Azure IoT Central**
-   [Azure Account](https://portal.azure.com)
-   [Azure IoT Central application](https://apps.azureiotcentral.com/)

**For Azure IoT Hub**
-   [Azure IoT Hub Instance](https://docs.microsoft.com/en-us/azure/iot-hub/about-iot-hub)
-   [Azure IoT Hub Device Provisioning Service](https://docs.microsoft.com/en-us/azure/iot-dps/quick-setup-auto-provision)
-   [Azure IoT Public Model Repository](https://docs.microsoft.com/en-us/azure/iot-pnp/concepts-model-repository)

<a name="Preparing the Impinj R700 Reader"></a>
# Preparing the Impinj R700 Reader
1.	Plug the R700 into a PoE switch via Ethernet cable.
    - Ensure the PoE switch is connected properly to a LAN which will provide the reader with a DHCP address
    - The reader should power on, and the “system” LED will display yellow.
    - After about a minute, the “system” LED should turn blue, indicating the reader has fully booted.
2.	From a PC that can access the same LAN to which the R700 is connected, confirm the IP address and/or hostname of the R700.  Once you have the IP address or hostname, confirm the connection by opening a command prompt and issuing the command: ping <ip address or hostname> (you should see successful packets being sent).
    - Finding the IP Address or Hostname:
        - Option 1: DHCP server access - login to the DHCP server and look at the client list for the IP assigned to the reader’s hostname.  The Impinj R700 reader should have a hostname of “impinj-XX-XX-XX” where the Xs are the last 6 digits of the MAC address (MAC address can be found on the label on the underside of the reader).
        - Option 2: Default hostname - if the DHCP/DNS server offers hostname resolution, the default hostname is “impinj-XX-XX-XX”, where the Xs are the last 6 digits of the MAC address (MAC address can be found on the label on the underside of the reader). 
        - Option 3: USB micro console access – plug a USB micro cable into the reader “console” port, and the USB A end into your computer.  This should enumerate a USB serial port (COM<N>), and if on Windows, you can check the COM number in device manager.  
            1.	Open a serial connection to the reader via PuTTY (or your preferred serial program), with a baud rate of 115200.
            2.	Login (default credentials -> user: root, password: impinj)
            3.	Issue the RShell command > show network summary
                - This should display the IP address.
3.	Enter the IP address or hostname (confirmed in step 2) into a web browser, and hit enter.  You should be prompted to enter login credentials (the default is -> user: root, password: impinj).  After logging in, it should display the reader configuration page.
4.	In the “READER INTERFACE” section of the reader configuration page, be sure that the “Available Interfaces” dropdown selection is set to “Impinj IoT Device Interface”.  If it is not, select it, and then click the “Update” link below the dropdown menu.
    - In the “READER” section of the reader configuration page, confirm that “Reader Interface” is set to “Impinj IoT Device Interface”.
5.	If the Reader allows you to change your regulatory region, be sure to select the proper region and that “None – RFID Disabled” is NOT selected.  Then press the “Update Region” link below the region selection dropdown menu.
a.	If changing the region, you will have to reboot the reader for the settings to take effect.
6.	If HTTPS has not been enabled on the reader, you may have to enable it through RShell with the following command: config network https enable
    - The R700 RShell may be accessed via SSH over TCP (using the IP/hostname) or Serial (via USB micro port, 115200 baud).
    - Full instructions for enabling HTTPS can be found at the following link: https://support.impinj.com/hc/en-us/articles/360017447560-How-to-configure-HTTP-and-HTTPS-on-the-Impinj-R700-Reader

<a name="Installing PnP Bridge Application on the Impinj R700 Reader"></a>
# Installing PnP Bridge Application on the Impinj R700 Reader

1.	Download the compiled CAP (Customer Application Paritition) image file (UPGX) from: (TBD)
2.	Open a web browser and enter the Impinj R700 IP address or hostname to navigate to the reader configuration page.  Enter login credentials (default -> user: root, password: impinj).
3.	In the “READER UPGRADE” section, click on the “Browse…” button next to “Select Upgrade File”, then choose the downloaded UPGX file from step 1.
4.	Click the “Upgrade” link below the “Browse…” button.  The Upgrade Status value will change to “WaitingForCommitImage”.  
    - Wait until the Upgrade Status is “Ready” and the Last Operation Status is “Waiting for manual reboot”.
5.	 Reboot the reader by clicking the “Reboot” link in the “READER REBOOT” section.

<a name="Connecting the PnP Bridge Application to Azure IoT"></a>
# Connecting the PnP Bridge Application to Azure IoT

1.  Identify the connection parameters for the Azure IoT service desired for the device connection.  Save these for step 3, below.
    - Connection Parameters:
      - ID Scope
      - Device Entrollment Symmetric Primary Key
    - [Azure IoT Central](https://docs.microsoft.com/en-us/azure/iot-central/core/concepts-get-connected): follow the directions for individual device enrollment with symmetric keys 
    - [Azure IoT Hub/DPS](https://docs.microsoft.com/en-us/azure/iot-dps/how-to-manage-enrollments): follow the directions for individual device enrollment with symmetric keys 
2.	With the CAP installed on the reader (from the last section), you should now be able to access the reader drive via FTP.  Use your preferred FTP client to copy /cust/config.json from the reader to your local machine for editing.
3.	Edit the copied config.json according to the recommendations found in the #Configuration section of this README document: https://github.com/Azure/iot-plug-and-play-bridge/tree/pnpbridgedev-impinj/pnpbridge/src/adapters/src/impinj_reader_r700#configuration-configuration
4.	After the local config.json file has been updated with the proper credentials, use your FTP client to push the updated file back onto the reader at /cust/config.json.
5.	Reboot the reader, and if everything was configured correctly, the PnP Bridge should reach out and connect with the configured Azure DPS and/or IoT Hub instances.

# Building, Deploying, and Extending the IoT Plug and Play Bridge

[Azure IoT Plug-and-Play Bridge Overview](https://docs.microsoft.com/en-us/azure/iot-pnp/concepts-iot-pnp-bridge)

[Azure IoT Plug-and-Play Bridge - Source Files](https://github.com/Azure/iot-plug-and-play-bridge/blob/pnpbridgedev-impinj/pnpbridge)

[Impinj R700 Adapter for Azure IoT PnP Bridge - Source Files](https://github.com/Azure/iot-plug-and-play-bridge/tree/pnpbridgedev-impinj/pnpbridge/src/adapters/src/impinj_reader_r700)





<a name="AdditionalLinks"></a>
# Additional Links

Please refer to the below link for additional information for Plug and Play 

-   [Manage cloud device messaging with Azure-IoT-Explorer](https://github.com/Azure/azure-iot-explorer/releases)
-   [Import the Plug and Play model](https://docs.microsoft.com/en-us/azure/iot-pnp/concepts-model-repository)
-   [Configure to connect to IoT Hub](https://docs.microsoft.com/en-us/azure/iot-pnp/quickstart-connect-device-c)
-   [How to use IoT Explorer to interact with the device ](https://docs.microsoft.com/en-us/azure/iot-pnp/howto-use-iot-explorer#install-azure-iot-explorer)   