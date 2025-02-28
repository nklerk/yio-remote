#include "bluetootharea.h"
#include "notifications.h"
#include "math.h"

BluetoothArea::BluetoothArea(QObject *parent) : QObject(parent)
{
}

void BluetoothArea::init(const QVariantMap &config)
{
    // read from config.json
//    if (config.value("settings").toMap().value("bluetootharea").toBool())  {

        // load the areas
        QVariantList areas = config.value("areas").toList();

        for (int i=0; i<areas.length(); i++) {
            QVariantMap m = areas[i].toMap();

            m_areas.insert(m["bluetooth"].toString(), m["area"].toString());
        }

        // create a bluetooth class
        BluetoothThread *blThread = new BluetoothThread(m_areas, m_timerInterval);

        // move it to a thread
        blThread->moveToThread(&m_thread);

        // connect signals and slots
        connect(&m_thread, &QThread::finished, blThread, &QObject::deleteLater);

        // connect signals from bluetootharea to object in thread
        connect(this, &BluetoothArea::startScanSignal, blThread, &BluetoothThread::startScan);
        connect(this, &BluetoothArea::stopScanSignal, blThread, &BluetoothThread::stopScan);
        connect(this, &BluetoothArea::turnOnSignal, blThread, &BluetoothThread::turnOn);
        connect(this, &BluetoothArea::turnOffSignal, blThread, &BluetoothThread::turnOff);
        connect(this, &BluetoothArea::lookForDockSignal, blThread, &BluetoothThread::lookForDock);
        connect(this, &BluetoothArea::sendInfoToDockSignal, blThread, &BluetoothThread::receiveInfoForDock);

        // connect signals from objet in thread to bluetootharea
        connect(blThread, &BluetoothThread::foundRoom, this, &BluetoothArea::deviceDiscovered);
        connect(blThread, &BluetoothThread::foundDock, this, &BluetoothArea::foundDock);

        // start thread
        m_thread.start();
//    }
}

void BluetoothArea::turnOn()
{
    emit turnOnSignal();
}

void BluetoothArea::turnOff()
{
    emit turnOffSignal();
}

void BluetoothArea::startScan()
{
    emit startScanSignal();
}

void BluetoothArea::stopScan()
{
    emit stopScanSignal();
}

void BluetoothArea::lookForDock()
{
    emit lookForDockSignal();
}

void BluetoothArea::sendInfoToDock(const QString &msg)
{
    emit sendInfoToDockSignal(msg);
}

void BluetoothArea::deviceDiscovered(const QString &area)
{
    m_currentArea = area;
    emit currentAreaChanged();
}

void BluetoothArea::foundDock()
{
    emit dockFound();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//// BLUETOOTHTHREAD CLASS
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

BluetoothThread::BluetoothThread(QMap<QString, QString> areas, int interval)
{
    m_areas = areas;

    m_timer->setInterval(interval);
    m_timer->setSingleShot(true);
    m_timer->stop();

    connect(m_timer, SIGNAL(timeout()), this, SLOT(onTimeout()));

    // if there is a bluetooth device, let's set up some things
    if (m_localDevice.isValid()) {

        connect(m_discoveryAgent, SIGNAL(deviceDiscovered(QBluetoothDeviceInfo)), this, SLOT(deviceDiscovered(QBluetoothDeviceInfo)));
        connect(m_discoveryAgent, SIGNAL(finished()), this, SLOT(discoveryFinished()));

        qDebug() << "Bluetooth init OK";

    } else {
        Notifications::getInstance()->add(true,"Bluetooth device was not found.");
    }
}

void BluetoothThread::startScan()
{
    if (!running) {

        running = true;

        qDebug() << "Turn on and start bluetooth scan";

        // turn bluetooth on if it was off
        if (m_localDevice.hostMode() == QBluetoothLocalDevice::HostPoweredOff) {
            m_localDevice.powerOn();

            // read the name
            m_localDeviceName = m_localDevice.name();

            // read the address
            m_localDeviceAddr = m_localDevice.address();
        }

        m_discoveryAgent->start();
    }
}

void BluetoothThread::stopScan()
{
    if (running) {

        running = false;

        qDebug() << "Stop bluetooth scan";

        // stop scan
        m_discoveryAgent->stop();
    }

        m_timer->stop();
}

void BluetoothThread::turnOff()
{
    // power off
    m_localDevice.setHostMode(QBluetoothLocalDevice::HostPoweredOff);

}

void BluetoothThread::turnOn()
{
    if (m_localDevice.hostMode() == QBluetoothLocalDevice::HostPoweredOff) {
        qDebug() << "Turning bluetooth on";
        m_localDevice.powerOn();

        // read the name
        m_localDeviceName = m_localDevice.name();

        // read the address
        m_localDeviceAddr = m_localDevice.address();
    }
}

void BluetoothThread::deviceDiscovered(const QBluetoothDeviceInfo &device)
{
    if (m_areas.contains(device.address().toString()) && device.rssi() != 0) {

        int rssi = int(device.rssi())*-1;
        m_rssi.insert(m_areas.value(device.address().toString()), rssi);

//        qDebug() << "Found" << m_areas.value(device.address().toString()) << rssi;

        // this map will be sorted by default
        QMap<int, QString> sortedMap;

        for (QMap<QString, int>::iterator i = m_rssi.begin(); i != m_rssi.end(); ++i) {
            sortedMap.insert(i.value(), i.key());
        }

        // qmap ordered by key, so now we get the first key to get the closest beacon
        m_currentArea = sortedMap.value(sortedMap.firstKey());
        emit foundRoom(m_currentArea);

        //qDebug() << "Current area" << m_currentArea;
    }

    // if dock is found
    if (device.name().contains("YIO-Dock")) {
        // stop the discovery
        m_dock_address = device.address();
        m_discoveryAgent->stop();
        m_localDevice.requestPairing(m_dock_address, QBluetoothLocalDevice::Paired);
        emit foundDock();
        qDebug() << "YIO Dock found";
    }
}

void BluetoothThread::discoveryFinished()
{
    // restart scan after delay
    running = false;
    m_timer->start();
}

void BluetoothThread::lookForDock()
{
    // turn bluetooth on if it was off
    if (m_localDevice.hostMode() == QBluetoothLocalDevice::HostPoweredOff) {
        m_localDevice.powerOn();

        // read the name
        m_localDeviceName = m_localDevice.name();

        // read the address
        m_localDeviceAddr = m_localDevice.address();
    }

    m_discoveryAgent->start();
}

void BluetoothThread::receiveInfoForDock(const QString &msg)
{
    m_dock_message = msg;

    m_socket = new QBluetoothSocket(QBluetoothServiceInfo::RfcommProtocol, this);
    connect(m_socket, &QBluetoothSocket::connected, this, &BluetoothThread::dockConnected);
    connect(m_socket, &QBluetoothSocket::stateChanged, this, &BluetoothThread::dockStateChanged);
    m_socket->connectToService(QBluetoothAddress(m_dock_address), QBluetoothUuid(QStringLiteral("00001101-0000-1000-8000-00805f9b34fb")));
}

void BluetoothThread::dockConnected()
{
    // open the bluetooth socket
    m_socket->open(QIODevice::WriteOnly);

    // format the message
    QByteArray text = m_dock_message.toUtf8() + '\n';
    m_socket->write(text);
    qDebug() << "Message sent to dock";

    // close and delete the socket
//    m_socket->close();
//    delete  m_socket;
//    m_socket = nullptr;
}

void BluetoothThread::dockStateChanged(QBluetoothSocket::SocketState state)
{
    qDebug() << state;
}

void BluetoothThread::onTimeout()
{
    m_discoveryAgent->start();
}
