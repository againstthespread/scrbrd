#!/usr/bin/env python3
"""Verify actual BLE callback/queue code discards previous-session packets."""
from pathlib import Path
import os
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
#include <chrono>
#include <thread>
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
  // Overflow, oversized writes, and allocation-independent queue recovery.
  for (int i = 0; i < RECEIVED_MESSAGE_QUEUE_CAPACITY; ++i) {
    rx.value = "queued"; callback.onWrite(&rx);
  }
  rx.value = "overflow"; callback.onWrite(&rx);
  assert(manager.takeReceiveLossEvent()); assert(!manager.takeReceiveLossEvent());
  while (manager.hasReceivedMessage()) assert(manager.getReceivedMessage().empty());
  rx.value = String(513, 'x'); callback.onWrite(&rx);
  assert(manager.takeReceiveLossEvent()); assert(!manager.hasReceivedMessage());
  rx.value = String(512, 'x'); callback.onWrite(&rx);
  assert(manager.getReceivedMessage() == rx.value);
  assert(!manager.takeReceiveLossEvent());
  // Keep fewer than 32 packets in flight: capacity must never be reached.
  std::atomic<int> produced{0}, consumed{0};
  std::atomic<bool> done{false};
  constexpr int packets = 20000;
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
  std::thread producer([&] {
    for (int i = 0; i < packets; ++i) {
      while (i - consumed.load() >= 32) {
        if (std::chrono::steady_clock::now() > deadline) { done.store(true); return; }
        std::this_thread::yield();
      }
      rx.value = std::to_string(i); callback.onWrite(&rx);
      produced.store(i + 1);
    }
    done.store(true);
  });
  int expected = 0;
  while (!done.load() || manager.hasReceivedMessage()) {
    if (std::chrono::steady_clock::now() > deadline) break;
    if (!manager.hasReceivedMessage()) { std::this_thread::yield(); continue; }
    auto packet = manager.getReceivedMessage();
    assert(packet == std::to_string(expected));
    ++expected; consumed.store(expected);
  }
  producer.join();
  assert(produced == packets && consumed == packets && !manager.hasReceivedMessage());
}
'''
functions = '\n'.join(function(sig) for sig in [
    'bool BluetoothManager::hasReceivedMessage()',
    'String BluetoothManager::getReceivedMessage()',
    'bool BluetoothManager::takeDisconnectEvent()',
    'bool BluetoothManager::takeReceiveLossEvent()',
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
                    '-Wno-unused-variable', '-Wno-unused-parameter', '-pthread',
                    *os.environ.get('NATIVE_SANITIZERS', '').split(),
                    '-I' + str(directory), '-I' + str(root / 'test/native'),
                    '-I' + str(root), str(cpp), '-o', str(executable)], check=True)
    subprocess.run([str(executable)], check=True)
print('BLE disconnect event and session-isolated RX queue passed.')
