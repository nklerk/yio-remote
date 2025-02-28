#include <QtDebug>
#include <QJsonDocument>
#include <QJsonArray>

#include "homeassistant.h"
#include "../../entities/entity.h"
#include "math.h"

void HomeAssistant::initialize(int integrationId, const QVariantMap& config, QObject* entities, QObject* notifications)
{
    setIntegrationId(integrationId);

    for (QVariantMap::const_iterator iter = config.begin(); iter != config.end(); ++iter) {
        if (iter.key() == "type")
            setType(iter.value().toString());
        else if (iter.key() == "friendly_name")
            setFriendlyName(iter.value().toString());
    }

    // crate a new instance and pass on variables
    HomeAssistantThread *HAThread = new HomeAssistantThread(config, entities, notifications);

    // move to thread
    HAThread->moveToThread(&m_thread);

    // connect signals and slots
    QObject::connect(&m_thread, &QThread::finished, HAThread, &QObject::deleteLater);

    QObject::connect(this, &HomeAssistant::connectSignal, HAThread, &HomeAssistantThread::connect);
    QObject::connect(this, &HomeAssistant::disconnectSignal, HAThread, &HomeAssistantThread::disconnect);
    QObject::connect(this, &HomeAssistant::sendCommandSignal, HAThread, &HomeAssistantThread::sendCommand);

    QObject::connect(HAThread, &HomeAssistantThread::stateChanged, this, &HomeAssistant::stateHandler);

    m_thread.start();
}

void HomeAssistant::connect()
{
    emit connectSignal();
}

void HomeAssistant::disconnect()
{
    emit disconnectSignal();
}

void HomeAssistant::sendCommand(const QString& type, const QString& entity_id, const QString& command, const QVariant& param)
{
    emit sendCommandSignal(type, entity_id, command, param);
}

void HomeAssistant::stateHandler(int state)
{
    if (state == 0) {
        setState(CONNECTED);
    } else if (state == 1) {
        setState(CONNECTING);
    } else if (state == 2) {
        setState(DISCONNECTED);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//// HOME ASSISTANT THREAD CLASS
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

HomeAssistantThread::HomeAssistantThread(const QVariantMap &config, QObject *entities, QObject *notifications)
{
    for (QVariantMap::const_iterator iter = config.begin(); iter != config.end(); ++iter) {
        if (iter.key() == "data") {
            QVariantMap map = iter.value().toMap();
            m_ip = map.value("ip").toString();
            m_token = map.value("token").toString();
        }
    }
    m_entities = qobject_cast<EntitiesInterface *>(entities);
    m_notifications = qobject_cast<NotificationsInterface *>(notifications);

    m_webSocketId = 4;

    m_websocketReconnect = new QTimer(this);

    m_websocketReconnect->setSingleShot(true);
    m_websocketReconnect->setInterval(2000);
    m_websocketReconnect->stop();

    m_socket = new QWebSocket;
    m_socket->setParent(this);

    QObject::connect(m_socket, SIGNAL(textMessageReceived(const QString &)), this, SLOT(onTextMessageReceived(const QString &)));
    QObject::connect(m_socket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(onError(QAbstractSocket::SocketError)));
    QObject::connect(m_socket, SIGNAL(stateChanged(QAbstractSocket::SocketState)), this, SLOT(onStateChanged(QAbstractSocket::SocketState)));

    QObject::connect(m_websocketReconnect, SIGNAL(timeout()), this, SLOT(onTimeout()));
}



void HomeAssistantThread::onTextMessageReceived(const QString &message)
{
    QJsonParseError parseerror;
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8(), &parseerror);
    if (parseerror.error != QJsonParseError::NoError) {
        qDebug() << "JSON error : " << parseerror.errorString();
        return;
    }
    QVariantMap map = doc.toVariant().toMap();

    QString m = map.value("error").toString();
    if (m.length() > 0) {
        qDebug() << "error : " << m;
    }

    QString type = map.value("type").toString();
    int id = map.value("id").toInt();

    if (type == "auth_required") {
        QString auth = QString("{ \"type\": \"auth\", \"access_token\": \"%1\" }\n").arg(m_token);
        m_socket->sendTextMessage(auth);
        return;
    }

    if (type == "auth_ok") {
        qDebug() << "Connection successful";
        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // FETCH STATES
        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        m_socket->sendTextMessage("{\"id\": 2, \"type\": \"get_states\"}\n");
    }

    if (id == 2) {
        QVariantList list = map.value("result").toJsonArray().toVariantList();
        for (int i = 0; i < list.length(); i++) {
            QVariantMap result = list.value(i).toMap();
            updateEntity(result.value("entity_id").toString(), result);
        }

        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // SUBSCRIBE TO EVENTS IN HOME ASSISTANT
        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        m_socket->sendTextMessage("{\"id\": 3, \"type\": \"subscribe_events\", \"event_type\": \"state_changed\"}\n");
    }

    if (type == "result" && id == 3) {
        setState(0);
        qDebug() << "Subscribed to state changes";
        // remove notifications that we don't need anymore as the integration is connected
        //        m_notifications->remove("Cannot connect to Home Assistant.");
    }

    if (id == m_webSocketId) {
        qDebug() << "Command successful";
    }

    if (type == "event" && id == 3) {
        QVariantMap data = map.value("event").toMap().value("data").toMap();
        QVariantMap newState = data.value("new_state").toMap();
        updateEntity(data.value("entity_id").toString(), newState);
    }
}

void HomeAssistantThread::onStateChanged(QAbstractSocket::SocketState state)
{
    if (state == QAbstractSocket::UnconnectedState && !m_userDisconnect) {
        setState(2);
        m_websocketReconnect->start();
    }
}

void HomeAssistantThread::onError(QAbstractSocket::SocketError error)
{
    qDebug() << error;
    m_socket->close();
    setState(2);
    m_websocketReconnect->start();
}

void HomeAssistantThread::onTimeout()
{
    if (m_tries == 3) {
        m_websocketReconnect->stop();

        m_notifications->add(true,tr("Cannot connect to Home Assistant."), tr("Reconnect"), "homeassistant");
        disconnect();
        m_tries = 0;
    }
    else {
        m_webSocketId = 4;
        if (m_state != 1)
        {
            setState(1);
        }
        QString url = QString("ws://").append(m_ip).append("/api/websocket");
        m_socket->open(QUrl(url));

        m_tries++;
    }
}

void HomeAssistantThread::webSocketSendCommand(const QString& domain, const QString& service, const QString& entity_id, QVariantMap *data)
{
    // sends a command to home assistant
    m_webSocketId++;

    QVariantMap map;
    map.insert("id", QVariant(m_webSocketId));
    map.insert("type", QVariant("call_service"));
    map.insert("domain", QVariant(domain));
    map.insert("service", QVariant(service));

    if (data == NULL) {
        QVariantMap d;
        d.insert("entity_id", QVariant(entity_id));
        map.insert("service_data", d);
    }
    else {
        data->insert("entity_id", QVariant(entity_id));
        map.insert("service_data", *data);
    }
    QJsonDocument doc = QJsonDocument::fromVariant(map);
    QString message = doc.toJson(QJsonDocument::JsonFormat::Compact);
    m_socket->sendTextMessage(message);

}

int HomeAssistantThread::convertBrightnessToPercentage(float value)
{
    return int(round(value/255*100));
}

void HomeAssistantThread::updateEntity(const QString& entity_id, const QVariantMap& attr)
{
    Entity* entity = (Entity*)m_entities->get(entity_id);
    if (entity) {
        if (entity->type() == "light") {
            updateLight(entity, attr);
        }
        if (entity->type() == "blind") {
            updateBlind(entity, attr);
        }
        if (entity->type() == "media_player") {
            updateMediaPlayer(entity, attr);
        }
    }
}

void HomeAssistantThread::updateLight(Entity* entity, const QVariantMap& attr)
{
    QVariantMap attributes;

    // state
    if (attr.value("state").toString() == "on") {
        attributes.insert("state", true);
    } else {
        attributes.insert("state", false);
    }

    // brightness
    if (entity->supported_features().indexOf("BRIGHTNESS") > -1) {
        if (attr.value("attributes").toMap().contains("brightness")) {
            attributes.insert("brightness", convertBrightnessToPercentage(attr.value("attributes").toMap().value("brightness").toInt()));
        } else {
            attributes.insert("brightness", 0);
        }
    }

    // color
    if (entity->supported_features().indexOf("COLOR") > -1) {
        QVariant color = attr.value("attributes").toMap().value("rgb_color");
        QVariantList cl(color.toList());
        char buffer[10];
        sprintf(buffer, "#%02X%02X%02X", cl.value(0).toInt(), cl.value(1).toInt(), cl.value(2).toInt());
        attributes.insert("color", buffer);
    }

    // color temp
    if (entity->supported_features().indexOf("COLORTEMP") > -1) {

    }

    m_entities->update(entity->entity_id(), attributes);
}

void HomeAssistantThread::updateBlind(Entity *entity, const QVariantMap &attr)
{
    QVariantMap attributes;

    // state
    if (attr.value("state").toString() == "open") {
        attributes.insert("state", true);
    } else {
        attributes.insert("state", false);
    }

    // position
    if (entity->supported_features().indexOf("POSITION") > -1) {
        attributes.insert("position", attr.value("attributes").toMap().value("current_position").toInt());
    }

    m_entities->update(entity->entity_id(), attributes);
}

void HomeAssistantThread::updateMediaPlayer(Entity *entity, const QVariantMap &attr)
{
    QVariantMap attributes;

    //state
    if (attr.value("state").toString() == "off") {
        attributes.insert("state", 0);
    } else if (attr.value("state").toString() == "on") {
        attributes.insert("state", 1);
    } else if (attr.value("state").toString() == "idle") {
        attributes.insert("state", 2);
    } else if (attr.value("state").toString() == "playing") {
        attributes.insert("state", 3);
    } else {
        attributes.insert("state", 0);
    }

    // source
    if (entity->supported_features().indexOf("SOURCE") > -1 && attr.value("attributes").toMap().contains("source")) {
        attributes.insert("source", attr.value("attributes").toMap().value("source").toString());
    }

    // volume
    if (entity->supported_features().indexOf("VOLUME") > -1 && attr.value("attributes").toMap().contains("volume_level")) {
        attributes.insert("volume", attr.value("attributes").toMap().value("volume_level").toDouble());
    }

    // media type
    if (entity->supported_features().indexOf("MEDIA_TYPE") > -1 && attr.value("attributes").toMap().contains("media_content_type")) {
        attributes.insert("mediaType", attr.value("attributes").toMap().value("media_content_type").toString());
    }

    // media image
    if (entity->supported_features().indexOf("MEDIA_IMAGE") > -1 && attr.value("attributes").toMap().contains("entity_picture")) {
        QString url = attr.value("attributes").toMap().value("entity_picture").toString();
        QString fullUrl = QString("http://").append(m_ip).append(url);
        attributes.insert("mediaImage", fullUrl);
    }

    // media title
    if (entity->supported_features().indexOf("MEDIA_TITLE") > -1 && attr.value("attributes").toMap().contains("media_title")) {
        attributes.insert("mediaTitle", attr.value("attributes").toMap().value("media_title").toString());
    }

    // media artist
    if (entity->supported_features().indexOf("MEDIA_ARTIST") > -1 && attr.value("attributes").toMap().contains("media_artist")) {
        attributes.insert("mediaArtist", attr.value("attributes").toMap().value("media_artist").toString());
    }

    m_entities->update(entity->entity_id(), attributes);
}

void HomeAssistantThread::setState(int state)
{
    m_state = state;
    emit stateChanged(state);
}

void HomeAssistantThread::connect()
{
    m_userDisconnect = false;

    setState(1);

    // reset the reconnnect trial variable
    m_tries = 0;

    // turn on the websocket connection
    QString url = QString("ws://").append(m_ip).append("/api/websocket");
    m_socket->open(QUrl(url));
}

void HomeAssistantThread::disconnect()
{
    m_userDisconnect = true;

    // turn of the reconnect try
    m_websocketReconnect->stop();

    // turn off the socket
    m_socket->close();

    setState(2);
}

void HomeAssistantThread::sendCommand(const QString &type, const QString &entity_id, const QString &command, const QVariant &param)
{
    if (type == "light") {
        if (command == "TOGGLE")
            webSocketSendCommand(type, "toggle", entity_id, NULL);
        else if (command == "ON")
            webSocketSendCommand(type, "turn_on", entity_id, NULL);
        else if (command == "OFF")
            webSocketSendCommand(type, "turn_off", entity_id, NULL);
        else if (command == "BRIGHTNESS") {
            QVariantMap data;
            data.insert("brightness_pct", param);
            webSocketSendCommand(type, "turn_on", entity_id, &data);
        }
        else if (command == "COLOR") {
            QColor color = param.value<QColor>();
            QVariantMap data;
            QVariantList list;
            list.append(color.red());
            list.append(color.green());
            list.append(color.blue());
            data.insert("rgb_color", list);
            webSocketSendCommand(type, "turn_on", entity_id, &data);
        }
    }
    if (type == "blind") {
        if (command == "OPEN")
            webSocketSendCommand("cover", "open_cover", entity_id, NULL);
        else if (command == "CLOSE")
            webSocketSendCommand("cover", "close_cover", entity_id, NULL);
        else if (command == "STOP")
            webSocketSendCommand("cover", "stop_cover", entity_id, NULL);
        else if (command == "POSITION") {
            QVariantMap data;
            data.insert("position", param);
            webSocketSendCommand("cover", "set_cover_position", entity_id, &data);
        }
    }
    if (type == "media_player") {
        if (command == "VOLUME_SET") {
            QVariantMap data;
            data.insert("volume_level", param);
            webSocketSendCommand(type, "volume_set", entity_id, &data);
        }
        else if (command == "PLAY")
            webSocketSendCommand(type, "media_play_pause", entity_id, NULL);
        else if (command == "PREVIOUS")
            webSocketSendCommand(type, "media_previous_track", entity_id, NULL);
        else if (command == "NEXT")
            webSocketSendCommand(type, "media_next_track", entity_id, NULL);
        else if (command == "TURNON")
            webSocketSendCommand(type, "turn_on", entity_id, NULL);
        else if (command == "TURNOFF")
            webSocketSendCommand(type, "turn_off", entity_id, NULL);
    }
}
