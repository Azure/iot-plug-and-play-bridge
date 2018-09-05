using System.Threading.Tasks;

public delegate string CommandHandler(string command, string input);
public delegate string PropertyChangeHandler(string property, string input);

public class DeviceClient
{

}

public class PnPInterface
{
    public PnPInterface(string id, DeviceClient deviceClient, PropertyChangeHandler propHandler, CommandHandler handler) { }

    // Define the properties/event/commands
    public void BindProperty(string propertyName) { }
    public void BindEvent(string eventName) { }
    public void BindCommand(string CommandName) { }

    // Method to update the interface in device twin once binding is complete
    public async Task PublishInterface() { }

    // Methods to access the properties and send event
    public async Task WriteProperty(string propertyName, string val) { }
    public async Task SendEvent(string eventName, string data) { }
}