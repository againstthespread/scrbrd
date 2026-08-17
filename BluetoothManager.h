/**
 * BluetoothManager provides a small BLE text-command transport.
 */

#ifndef BLUETOOTH_MANAGER_H
#define BLUETOOTH_MANAGER_H

#include <Arduino.h>
#include "HWCDC.h"

extern HWCDC USBSerial;

const char BLUETOOTH_SERVICE_UUID[] = "d8f6a9b0-7a5e-4e8c-9f2a-2b2f5b6c1001";
const char BLUETOOTH_RX_CHARACTERISTIC_UUID[] = "d8f6a9b1-7a5e-4e8c-9f2a-2b2f5b6c1001";
// TEMPORARY BLE WAKE-NOTIFICATION EXPERIMENT: Remove after iOS testing.
const char BLUETOOTH_WAKE_CHARACTERISTIC_UUID[] = "d8f6a9b2-7a5e-4e8c-9f2a-2b2f5b6c1001";

class BluetoothManager
{
public:
  void beginBluetooth();
  void updateBluetooth();

  bool isBluetoothConnected();
  const char *getBluetoothStatusText();

  bool hasReceivedMessage();
  String getReceivedMessage();

private:
  bool bluetoothStarted = false;
};

#endif
