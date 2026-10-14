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
// Model Arduino String's non-throwing copy allocation failure. Move remains
// allocation-free. This checks C++ return semantics, not the ESP32 allocator.
class String : public std::string {
public:
  using std::string::string;
  using std::string::operator=;
  static inline bool failCopy = false;
  String(const String &other) : std::string(failCopy ? "" : other.c_str(), failCopy ? 0 : other.size()) {}
  String(String &&other) noexcept = default;
  String &operator=(const String &) = default;
  String &operator=(String &&) noexcept = default;
};
#define private public
#include "BluetoothManager.h"
#undef private
HWCDC USBSerial;
uint32_t now = 100;
unsigned long millis() { return now; }
struct BLEServer {};
struct BLEAdvertising { void start() {} };
struct BLEServerCallbacks {
  virtual void onConnect(BLEServer *) {}
  virtual void onDisconnect(BLEServer *) {}
};
struct BLECharacteristic { String value; unsigned notifications = 0; String getValue() { return value; } void setValue(const char *text) { value = text; } void notify() { ++notifications; } };
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
  BLECharacteristic wake;
  wakeCharacteristic = &wake; manager.bluetoothStarted = true;
  now = 20099; manager.updateBluetooth(); assert(wake.notifications == 0);
  now = 20100; manager.updateBluetooth(); assert(wake.notifications == 1 && wake.value == "WAKE");
  now = 20101; manager.updateBluetooth(); assert(wake.notifications == 1);
  server.onDisconnect(nullptr); now = 40100; manager.updateBluetooth();
  assert(wake.notifications == 1); assert(manager.takeDisconnectEvent());
  now = 0xFFFFFF00; server.onConnect(nullptr);
  now += 19999; manager.updateBluetooth(); assert(wake.notifications == 1);
  ++now; manager.updateBluetooth(); assert(wake.notifications == 2);
  // The consumer must transfer ownership without another allocating copy.
  rx.value = String(512, 'z'); callback.onWrite(&rx);
  String::failCopy = true;
  const auto packetWithoutAllocation = manager.getReceivedMessage();
  String::failCopy = false;
  assert(packetWithoutAllocation == String(512, 'z'));
  assert(!manager.takeReceiveLossEvent());
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
  produced.store(0); consumed.store(0); done.store(false);
  const auto reconnectDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
  std::thread reconnectingProducer([&] {
    for (int i = 0; i < packets; ++i) {
      while (i - consumed.load() >= 32) {
        if (std::chrono::steady_clock::now() > reconnectDeadline) { done.store(true); return; }
        std::this_thread::yield();
      }
      if (i % 137 == 0) { server.onDisconnect(nullptr); server.onConnect(nullptr); }
      rx.value = std::to_string(i); callback.onWrite(&rx); produced.store(i + 1);
    }
    done.store(true);
  });
  int last = -1, processed = 0;
  while (!done.load() || manager.hasReceivedMessage()) {
    if (std::chrono::steady_clock::now() > reconnectDeadline) break;
    if (!manager.hasReceivedMessage()) { std::this_thread::yield(); continue; }
    auto packet = manager.getReceivedMessage();
    if (!packet.empty()) { const int sequence = std::stoi(packet); assert(sequence > last); last = sequence; }
    manager.takeDisconnectEvent(); ++processed; consumed.store(processed);
  }
  reconnectingProducer.join();
  assert(produced == packets && processed == packets && !manager.hasReceivedMessage());
  rx.value = "fresh-recovery"; callback.onWrite(&rx);
  assert(manager.getReceivedMessage() == "fresh-recovery");
}
'''
functions = '\n'.join(function(sig) for sig in [
    'void BluetoothManager::updateBluetooth()',
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
