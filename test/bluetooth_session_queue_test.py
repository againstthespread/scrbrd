#!/usr/bin/env python3
"""Verify actual BLE callback/queue code discards previous-session packets."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'BluetoothManager.cpp').read_text()
callbacks = source[source.index('namespace\n{'):source.index('/**\n * Start BLE')]


def function(signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


prefix = r'''
#include <atomic>
#include <cassert>
#include <string>
using String = std::string;
#include "BluetoothManager.h"
HWCDC USBSerial;
unsigned long millis() { return 100; }
struct BLEServer {};
struct BLEAdvertising {};
struct BLEServerCallbacks {
  virtual void onConnect(BLEServer *) {}
  virtual void onDisconnect(BLEServer *) {}
};
struct BLECharacteristic { String value; String getValue() { return value; } };
struct BLECharacteristicCallbacks { virtual void onWrite(BLECharacteristic *) {} };
'''
tests = r'''
int main() {
  BluetoothManager manager;
  SportsHubServerCallbacks serverImplementation;
  SportsHubCharacteristicCallbacks callbackImplementation;
  BLEServerCallbacks &server = serverImplementation;
  BLECharacteristicCallbacks &callback = callbackImplementation;
  BLECharacteristic rx;
  server.onConnect(nullptr);
  rx.value = "old-start"; callback.onWrite(&rx);
  rx.value = "old-entry"; callback.onWrite(&rx);
  server.onDisconnect(nullptr); server.onConnect(nullptr);
  rx.value = "new-start"; callback.onWrite(&rx);
  assert(manager.takeDisconnectEvent());
  assert(!manager.takeDisconnectEvent());
  assert(manager.getReceivedMessage().empty());
  assert(manager.getReceivedMessage().empty());
  assert(manager.getReceivedMessage() == "new-start");
  assert(!manager.hasReceivedMessage());
  for (int cycle = 0; cycle < 100; ++cycle) {
    rx.value = "old-end"; callback.onWrite(&rx);
    server.onDisconnect(nullptr); server.onConnect(nullptr);
    assert(manager.takeDisconnectEvent());
    rx.value = "current-entry"; callback.onWrite(&rx);
    assert(manager.getReceivedMessage().empty());
    assert(manager.getReceivedMessage() == "current-entry");
    assert(!manager.hasReceivedMessage());
  }
}
'''
functions = '\n'.join(function(sig) for sig in [
    'bool BluetoothManager::hasReceivedMessage()',
    'String BluetoothManager::getReceivedMessage()',
    'bool BluetoothManager::takeDisconnectEvent()',
])
with tempfile.TemporaryDirectory(prefix='ble-session-native-') as directory:
    directory = Path(directory)
    (directory / 'HWCDC.h').write_text('''class HWCDC { public:
      template<class T> void print(T) {} template<class T> void println(T) {} };
    ''')
    cpp = directory / 'test.cpp'
    cpp.write_text(prefix + callbacks + functions + tests)
    executable = directory / 'test'
    subprocess.run(['c++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                    '-Wno-unused-variable', '-Wno-unused-parameter',
                    '-I' + str(directory), '-I' + str(root / 'test/native'),
                    '-I' + str(root), str(cpp), '-o', str(executable)], check=True)
    subprocess.run([str(executable)], check=True)
print('BLE disconnect event and session-isolated RX queue passed.')
