import QtQuick 2.11
import QtWebSockets 1.0
import Integration 1.0

import "qrc:/integrations/homeassistant" as HomeAssistant

import "qrc:/scripts/helper.js" as JSHelper

Integration {
    id: integration

    // VARIOUS ENTITY INTEGRATIONS

    property alias light: light
    HomeAssistant.Light {
        id: light
    }

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
        // write connect function here

        state = Integration.CONNECTING

        // reset the reconnnect trial variable
        websocketReconnect.tries = 0;

        // turn on the websocket connection
        socket.active = true;
    }

    function disconnect()
    {
        // write disconnect function here

        // turn off the socket
        socket.active = false;

        // turn of the reconnect try
        websocketReconnect.running = false;

        state = Integration.DISCONNECTED
    }

    onConnected: {
        // remove notifications that say couldn't connec to Home Assistant
//        var tmp = notifications;
//        tmp.forEach(function(entry, index, object) {
//            if (entry.text === "Failed to connect to " + friendlyName + ".") {
//                tmp.splice(index, 1);
//            }
//        });
//        notifications = tmp;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // rest of the code starts here
    property int webSocketId: 4

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // SIGNALS
    // Signals to push the data to the components
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    signal sendFetchJson(var json)
    signal sendEventJson(var json)


    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // SIGNALS END
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // FUNCTIONS
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    function webSocketProcess (message) {

        //                        console.debug(message);
        var json = JSON.parse(message);

        if (json.error != null) {
            console.debug(json.error);
        }

        if (json.type == "auth_required") {
            json = {};
            json["type"] = "auth";
            json["access_token"] = config.integration[integrationId].data.token;
            socket.sendTextMessage(JSON.stringify(json));
        }

        if (json.type == "auth_ok") {
            console.debug("Connection successful")
            ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
            // FETCH STATES
            ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
            socket.sendTextMessage('{"id": 2, "type": "get_states"}\n');
        }

        if (json.success == true && json.id == 2) {

            //            fetch_json = json;
            integration.sendFetchJson(json);

            ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
            // SUBSCRIBE TO EVENTS IN HOME ASSISTANT
            ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
            socket.sendTextMessage('{"id": 3, "type": "subscribe_events", "event_type": "state_changed"}\n');
        }

        if (json.success == true && json.id == 3) {
            state = Integration.CONNECTED;
            console.debug("Subscribed to state changes");
        }

        if (json.success == true && json.id == webSocketId) {
            console.debug("Command successful");
        }

        if (json.success == false) {
            console.debug("Websocket error: " + json.error.message);
        }

        if (json.type == "event" && json.id == 3) {
            //            event_json = json;
            integration.sendEventJson(json);
        }
    }


    function webSocketSendCommand(domain, service, entity_id, data) { // sends a command to home assistant

        webSocketId++;

        var json = {};
        json["id"] = webSocketId;
        json["type"] = "call_service";
        json["domain"] = domain;
        json["service"] = service;
        if (data == null) {
            json["service_data"] = {"entity_id" : entity_id};
        } else {
            data["entity_id"] = entity_id;
            json["service_data"] = data;
        }
        socket.sendTextMessage(JSON.stringify(json));
    }


    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // WEBSOCKET CONNECTION
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    WebSocket {
        id: socket
        url: "ws://" + config.integration[integrationId].data.ip + "/api/websocket"
        onTextMessageReceived: {
            //            console.debug("\nReceived message: " + message)
            webSocketProcess(message);
        }
        onStatusChanged: if (socket.status == WebSocket.Error) {
                             console.debug("Error: " + socket.errorString)
                             socket.active = false
                             state = Integration.DISCONNECTED;
                             console.debug("Websocket connection error: " + connected)
                             websocketReconnect.start()
                         } else if (socket.status == WebSocket.Open) {
                             // open
                         } else if (socket.status == WebSocket.Closed) {
                             socket.active = false
                             state = Integration.DISCONNECTED;
                             console.debug("Websocket connection closed: " + connected);
                             websocketReconnect.start()
                         }
        active: false
    }

    Timer {
        id: websocketReconnect
        running: false
        repeat: false
        interval: 2000

        property int tries: 0

        onTriggered: {
            if (tries == 2) {
                websocketReconnect.running = false;
                //                connectionState = "failed"
                console.debug("Failed to connect");

                addNotification("error", qsTr("Failed to connect to Home Assistant.") + translateHandler.emptyString, function () { integration.connect(); }, "Reconnect");
                disconnect();

                tries = 0
            } else {
                webSocketId = 4
                if (state != Integration.CONNECTING)
                {
                    state = Integration.CONNECTING
                }
                socket.active = true
                console.debug("Reconnecting...")
                tries += 1
            }
        }
    }

}
