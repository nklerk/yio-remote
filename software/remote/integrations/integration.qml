////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// INTEGRATION TEMPLATE FILE
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

import QtQuick 2.11
import Integration 1.0

Integration {
    id: integration

    // PROPERTIES OF THE INTEGRATION
    // enum state - tells if the integration is connected. Set connected to true on succesfull connection. Set connected to false when disconnected.
    // CONNECTED, CONNECTING, DISCONNECTED
    // int integrationId - the id of the integration
    // string type - type of the integration, for example: homeassistant
    // string friendlyName - friendly name of the integration

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // CONNECT AND DISCONNECT FUNCTIONS
    // Must be the same function name for every integration
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    function connect()
    {
        state = Integration.CONNECTING
        // write connect function here
    }

    function disconnect()
    {
        state = Integration.DISCONNECTED
        // write disconnect functioen here
    }

    onConnected: {
        // when the connection state changes this signal triggered
        // remove notifications that say couldn't connec to Home Assistant
        var tmp = notifications;
        tmp.forEach(function(entry, index, object) {
            if (entry.text === "Failed to connect to " + friendlyName + ".") {
                tmp.splice(index, 1);
            }
        });
        notifications = tmp;
    }

    onConnecting: {}

    onDisconnected: {}

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // REST OF THE CODE STARTS THERE
}
