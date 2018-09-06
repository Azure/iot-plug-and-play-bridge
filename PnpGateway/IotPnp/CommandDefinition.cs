using System;
using System.Collections.Generic;
using System.Text;

namespace PnpGateway
{
    public delegate string CommandHandler(string cmd, string input);

    class CommandDef
    {
        public CommandDef(string command, CommandHandler handler)
        {
            if (string.IsNullOrEmpty(command) || handler == null) {
                throw new ArgumentException("Invalid parameters passed to command definition");
            }

            Command = command;
            Handler = handler;
        }


        public string Command { get; private set; }

        public CommandHandler Handler { get; private set; }
    }
}
