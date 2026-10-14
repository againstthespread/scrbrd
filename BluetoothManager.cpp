#include "BluetoothManager.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <atomic>

namespace
{
  BLEServer *bleServer = nullptr;
  BLEAdvertising *bleAdvertising = nullptr;
  // BLE WAKE notifications trigger the connected app's refresh path.
  BLECharacteristic *wakeCharacteristic = nullptr;

  const unsigned long WAKE_NOTIFICATION_INTERVAL_MS = 20000;
  std::atomic<uint32_t> lastWakeNotificationTime{0};

  std::atomic<uint32_t> connectionGeneration{0};
  std::atomic<bool> disconnectPending{false};
  std::atomic<bool> clientConnected{false};
  std::atomic<bool> shouldAdvertise{false};
  std::atomic<bool> receiveLossPending{false};
  const uint8_t RECEIVED_MESSAGE_QUEUE_CAPACITY = 56;
  // Bluedroid invokes onWrite on its single host task (one producer). The
  // Arduino loop is the sole consumer. Each publishes its own index only
  // after finishing all access to a slot; acquire/release transfers ownership.
  // One spare slot distinguishes full from empty without a shared count.
  const uint8_t RECEIVED_MESSAGE_QUEUE_STORAGE = RECEIVED_MESSAGE_QUEUE_CAPACITY + 1;
  String receivedMessages[RECEIVED_MESSAGE_QUEUE_STORAGE];
  uint32_t receivedMessageGenerations[RECEIVED_MESSAGE_QUEUE_STORAGE] = {};
  std::atomic<uint8_t> receivedMessageHead{0};
  std::atomic<uint8_t> receivedMessageTail{0};

  void markReceiveLoss()
  {
    connectionGeneration.fetch_add(1);
    receiveLossPending.store(true);
  }

  class SportsHubServerCallbacks : public BLEServerCallbacks
  {
    void onConnect(BLEServer *server)
    {
      clientConnected = true;
      lastWakeNotificationTime = millis();

    }

    void onDisconnect(BLEServer *server)
    {
      connectionGeneration.fetch_add(1);
      disconnectPending.store(true);
      clientConnected = false;
      shouldAdvertise = true;

    }
  };

  class SportsHubCharacteristicCallbacks : public BLECharacteristicCallbacks
  {
    void onWrite(BLECharacteristic *characteristic)
    {
      const uint32_t generation = connectionGeneration.load();
      auto value = characteristic->getValue();

      const uint8_t tail = receivedMessageTail.load(std::memory_order_relaxed);
      const uint8_t next = (tail + 1) % RECEIVED_MESSAGE_QUEUE_STORAGE;
      if (value.length() == 0 || value.length() > 512 ||
          next == receivedMessageHead.load(std::memory_order_acquire))
      {
        // A known dropped packet invalidates staged transactions, including
        // fantasy transfers whose compatible format has no entry count.
        markReceiveLoss();
        return;
      }

      receivedMessages[tail] = String(value.c_str(), value.length());
      if (receivedMessages[tail].length() != value.length())
      {
        markReceiveLoss();
        return;
      }
      receivedMessageGenerations[tail] = generation;
      receivedMessageTail.store(next, std::memory_order_release);
      // Never log payloads from the BLE task: USB backpressure used to block
      // the write callback (and its ATT response) for seconds.

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

  if (shouldAdvertise.exchange(false) && bleAdvertising != nullptr)
  {
    bleAdvertising->start();
    USBSerial.println("BLE advertising restarted.");
  }

  // BLE WAKE notifications trigger the connected app's refresh path.
  const unsigned long now = millis();
  if (static_cast<uint32_t>(now - lastWakeNotificationTime.load()) < WAKE_NOTIFICATION_INTERVAL_MS)
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
  return receivedMessageHead.load(std::memory_order_acquire) !=
    receivedMessageTail.load(std::memory_order_acquire);
}


String BluetoothManager::getReceivedMessage()
{
  const uint8_t head = receivedMessageHead.load(std::memory_order_relaxed);
  if (head == receivedMessageTail.load(std::memory_order_acquire)) return "";

  const uint32_t generation = receivedMessageGenerations[head];
  String receivedMessage = std::move(receivedMessages[head]);
  receivedMessageHead.store((head + 1) % RECEIVED_MESSAGE_QUEUE_STORAGE,
                           std::memory_order_release);
  return generation == connectionGeneration.load() ? receivedMessage : String("");
}

// Remember even a disconnect/reconnect that occurs between loop iterations.
bool BluetoothManager::takeDisconnectEvent()
{
  return disconnectPending.exchange(false);
}

// Drop/capacity failures are consumed by the loop, which owns staging state.
bool BluetoothManager::takeReceiveLossEvent()
{
  return receiveLossPending.exchange(false);
}
