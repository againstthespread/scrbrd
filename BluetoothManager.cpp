#include "BluetoothManager.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

namespace
{
  BLEServer *bleServer = nullptr;
  BLEAdvertising *bleAdvertising = nullptr;
  // BLE WAKE notifications trigger the connected app's refresh path.
  BLECharacteristic *wakeCharacteristic = nullptr;

  const unsigned long WAKE_NOTIFICATION_INTERVAL_MS = 20000;
  unsigned long lastWakeNotificationTime = 0;

  bool clientConnected = false;
  bool shouldAdvertise = false;
  const uint8_t RECEIVED_MESSAGE_QUEUE_CAPACITY = 56;
  String receivedMessages[RECEIVED_MESSAGE_QUEUE_CAPACITY];
  volatile uint8_t receivedMessageHead = 0;
  volatile uint8_t receivedMessageTail = 0;
  volatile uint8_t receivedMessageCount = 0;

  class SportsHubServerCallbacks : public BLEServerCallbacks
  {
    void onConnect(BLEServer *server)
    {
      clientConnected = true;
      lastWakeNotificationTime = millis();
      USBSerial.println("BLE client connected.");
    }

    void onDisconnect(BLEServer *server)
    {
      clientConnected = false;
      shouldAdvertise = true;
      USBSerial.println("BLE client disconnected.");
    }
  };

  class SportsHubCharacteristicCallbacks : public BLECharacteristicCallbacks
  {
    void onWrite(BLECharacteristic *characteristic)
    {
      auto value = characteristic->getValue();

      if (receivedMessageCount >= RECEIVED_MESSAGE_QUEUE_CAPACITY)
      {
        USBSerial.println("BLE received-message queue full; packet rejected.");
        return;
      }

      String receivedMessage = String(value.c_str(), value.length());
      receivedMessages[receivedMessageTail] = receivedMessage;
      receivedMessageTail =
        (receivedMessageTail + 1) % RECEIVED_MESSAGE_QUEUE_CAPACITY;
      receivedMessageCount++;

      USBSerial.print("BLE received message: ");
      USBSerial.println(receivedMessage);
    }
  };
}


/**
 * Start BLE as a peripheral that accepts short text writes from a phone.
 */
void BluetoothManager::beginBluetooth()
{
  if (bluetoothStarted)
  {
    return;
  }

  BLEDevice::init("Peter Sports Hub");

  bleServer = BLEDevice::createServer();
  bleServer->setCallbacks(new SportsHubServerCallbacks());

  BLEService *service = bleServer->createService(BLUETOOTH_SERVICE_UUID);

  BLECharacteristic *rxCharacteristic = service->createCharacteristic(
    BLUETOOTH_RX_CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR
  );

  rxCharacteristic->setCallbacks(new SportsHubCharacteristicCallbacks());
  rxCharacteristic->addDescriptor(new BLE2902());

  // BLE WAKE notifications trigger the connected app's refresh path.
  wakeCharacteristic = service->createCharacteristic(
    BLUETOOTH_WAKE_CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_NOTIFY
  );
  wakeCharacteristic->addDescriptor(new BLE2902());
  USBSerial.print("BLE WAKE: wake characteristic created: ");
  USBSerial.println(BLUETOOTH_WAKE_CHARACTERISTIC_UUID);

  service->start();

  bleAdvertising = BLEDevice::getAdvertising();
  bleAdvertising->addServiceUUID(BLUETOOTH_SERVICE_UUID);
  bleAdvertising->setScanResponse(true);
  bleAdvertising->start();

  bluetoothStarted = true;

  USBSerial.println("BLE started as Peter Sports Hub.");
  USBSerial.print("BLE service UUID: ");
  USBSerial.println(BLUETOOTH_SERVICE_UUID);
  USBSerial.print("BLE writable characteristic UUID: ");
  USBSerial.println(BLUETOOTH_RX_CHARACTERISTIC_UUID);
}


/**
 * Keep advertising available after a client disconnects.
 */
void BluetoothManager::updateBluetooth()
{
  if (!bluetoothStarted)
  {
    return;
  }

  if (shouldAdvertise && bleAdvertising != nullptr)
  {
    shouldAdvertise = false;
    bleAdvertising->start();
    USBSerial.println("BLE advertising restarted.");
  }

  // BLE WAKE notifications trigger the connected app's refresh path.
  const unsigned long now = millis();
  if (now - lastWakeNotificationTime < WAKE_NOTIFICATION_INTERVAL_MS)
  {
    return;
  }

  lastWakeNotificationTime = now;
  if (!clientConnected)
  {
    return;
  }

  if (wakeCharacteristic == nullptr)
  {
    USBSerial.println(
      "BLE WAKE: wake notification skipped; characteristic unavailable."
    );
    return;
  }

  wakeCharacteristic->setValue("WAKE");
  wakeCharacteristic->notify();
  USBSerial.println("BLE WAKE: wake notification sent.");
}


bool BluetoothManager::isBluetoothConnected()
{
  return clientConnected;
}


const char *BluetoothManager::getBluetoothStatusText()
{
  if (clientConnected)
  {
    return "Bluetooth: Connected";
  }

  return "Bluetooth: Waiting";
}


bool BluetoothManager::hasReceivedMessage()
{
  return receivedMessageCount > 0;
}


String BluetoothManager::getReceivedMessage()
{
  if (receivedMessageCount == 0)
  {
    return "";
  }

  String receivedMessage = receivedMessages[receivedMessageHead];
  receivedMessages[receivedMessageHead] = "";
  receivedMessageHead =
    (receivedMessageHead + 1) % RECEIVED_MESSAGE_QUEUE_CAPACITY;
  receivedMessageCount--;
  return receivedMessage;
}
