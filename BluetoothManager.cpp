#include "BluetoothManager.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

namespace
{
  BLEServer *bleServer = nullptr;
  BLEAdvertising *bleAdvertising = nullptr;
  // TEMPORARY BLE WAKE-NOTIFICATION EXPERIMENT: Remove after iOS testing.
  BLECharacteristic *wakeCharacteristic = nullptr;

  const unsigned long WAKE_NOTIFICATION_INTERVAL_MS = 20000;
  unsigned long lastWakeNotificationTime = 0;

  bool clientConnected = false;
  bool shouldAdvertise = false;
  bool messageAvailable = false;
  String receivedMessage = "";

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

      receivedMessage = String(value.c_str(), value.length());
      messageAvailable = true;

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

  // TEMPORARY BLE WAKE-NOTIFICATION EXPERIMENT: Remove after iOS testing.
  wakeCharacteristic = service->createCharacteristic(
    BLUETOOTH_WAKE_CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_NOTIFY
  );
  wakeCharacteristic->addDescriptor(new BLE2902());
  USBSerial.print("TEMP BLE WAKE EXPERIMENT: wake characteristic created: ");
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

  // TEMPORARY BLE WAKE-NOTIFICATION EXPERIMENT: Remove after iOS testing.
  const unsigned long now = millis();
  if (now - lastWakeNotificationTime < WAKE_NOTIFICATION_INTERVAL_MS)
  {
    return;
  }

  lastWakeNotificationTime = now;
  if (!clientConnected)
  {
    USBSerial.println(
      "TEMP BLE WAKE EXPERIMENT: wake notification skipped; no central connected."
    );
    return;
  }

  if (wakeCharacteristic == nullptr)
  {
    USBSerial.println(
      "TEMP BLE WAKE EXPERIMENT: wake notification skipped; characteristic unavailable."
    );
    return;
  }

  wakeCharacteristic->setValue("WAKE");
  wakeCharacteristic->notify();
  USBSerial.println("TEMP BLE WAKE EXPERIMENT: wake notification sent.");
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
  return messageAvailable;
}


String BluetoothManager::getReceivedMessage()
{
  messageAvailable = false;
  return receivedMessage;
}
