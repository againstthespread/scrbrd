#line 1 "C:\\Users\\Jay\\OneDrive\\Documents\\Arduino\\SportsHub\\BluetoothManager.cpp"
#include "BluetoothManager.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

namespace
{
  BLEServer *bleServer = nullptr;
  BLEAdvertising *bleAdvertising = nullptr;

  bool clientConnected = false;
  bool shouldAdvertise = false;
  bool messageAvailable = false;
  String receivedMessage = "";

  class SportsHubServerCallbacks : public BLEServerCallbacks
  {
    void onConnect(BLEServer *server)
    {
      clientConnected = true;
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
