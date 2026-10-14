#!/usr/bin/env python3
"""Exercise actual SPSC callbacks/consumer, saturation, ownership and sessions.

Set NATIVE_SANITIZERS='-fsanitize=thread -g' for concurrency verification.
"""
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
#include <deque>
#include <thread>
#include <string>
// Model Arduino String's non-throwing copy allocation failure. Move remains
// allocation-free. This checks C++ return semantics, not the ESP32 allocator.
class String : public std::string {
public:
  using std::string::string;
  using std::string::operator=;
  static inline bool failCopy = false;
  static inline bool failBuffer = false;
  String(const char *text, size_t length) : std::string(failBuffer ? "" : text, failBuffer ? 0 : length) {}
  String(const String &other) : std::string(failCopy ? "" : other.c_str(), failCopy ? 0 : other.size()) {}
  String(String &&other) noexcept = default;
  String &operator=(const String &) = default;
  String &operator=(String &&) noexcept = default;
};
#define private public
#include "BluetoothManager.h"
#undef private
HWCDC USBSerial;
std::atomic<uint32_t> now{100};
unsigned long millis() { return now.load(std::memory_order_relaxed); }
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
String stressPacket(int sequence) {
  // Heap-backed values exercise ownership beyond std::string's short buffer.
  const size_t length = sequence % 4 == 0 ? 512 : 32 + (sequence * 73) % 481;
  String result(length, 'a' + sequence % 26);
  result.replace(0, std::to_string(sequence).size(), std::to_string(sequence));
  return result;
}

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
  rx.value = "allocation-failure";
  String::failBuffer = true; callback.onWrite(&rx); String::failBuffer = false;
  assert(manager.takeReceiveLossEvent()); assert(!manager.hasReceivedMessage());
  rx.value = "after-allocation-failure"; callback.onWrite(&rx);
  assert(manager.getReceivedMessage() == rx.value);
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
      now.fetch_add(1, std::memory_order_relaxed);
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
    manager.takeDisconnectEvent(); manager.isBluetoothConnected();
    manager.getBluetoothStatusText(); manager.updateBluetooth();
    ++processed; consumed.store(processed);
  }
  reconnectingProducer.join();
  assert(produced == packets && processed == packets && !manager.hasReceivedMessage());
  rx.value = "fresh-recovery"; callback.onWrite(&rx);
  assert(manager.getReceivedMessage() == "fresh-recovery");

  // Concurrent variable-size/512-byte packets, near-capacity bursts, and held
  // results surviving multiple reuses of their original ring slots.
  constexpr int heapPackets = 30000;
  consumed.store(0); done.store(false);
  const auto heapDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
  std::thread heapProducer([&] {
    for (int i = 0; i < heapPackets; ++i) {
      while (i - consumed.load() >= 55) {
        assert(std::chrono::steady_clock::now() < heapDeadline);
        std::this_thread::yield();
      }
      rx.value = stressPacket(i); callback.onWrite(&rx);
      if (i % 11 == 0) std::this_thread::yield();
    }
    done.store(true);
  });
  std::deque<std::pair<int, String>> held;
  expected = 0;
  while (!done.load() || manager.hasReceivedMessage()) {
    assert(std::chrono::steady_clock::now() < heapDeadline);
    if (!manager.hasReceivedMessage()) { std::this_thread::yield(); continue; }
    auto packet = manager.getReceivedMessage();
    assert(packet == stressPacket(expected));
    held.emplace_back(expected, std::move(packet));
    if (held.size() > 114) {
      assert(held.front().second == stressPacket(held.front().first));
      held.pop_front();
    }
    ++expected; consumed.store(expected);
    if (expected % 7 == 0) std::this_thread::yield();
  }
  heapProducer.join();
  assert(expected == heapPackets && !manager.takeReceiveLossEvent());
  for (const auto &entry : held) assert(entry.second == stressPacket(entry.first));

  // Pause the consumer at exact capacity. Alternate overflow-invalidated bursts
  // with valid full bursts to prove bounded rejection, event coalescing, FIFO
  // order, and recovery across repeated index wraparound.
  static_assert(RECEIVED_MESSAGE_QUEUE_CAPACITY == 56, "Preserve RX capacity");
  constexpr int bursts = 200;
  std::atomic<int> burstReady{0}, burstDrained{0};
  const auto burstDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
  std::thread burstingProducer([&] {
    for (int burst = 0; burst < bursts; ++burst) {
      while (burstDrained.load() != burst) {
        assert(std::chrono::steady_clock::now() < burstDeadline);
        std::this_thread::yield();
      }
      for (int slot = 0; slot < RECEIVED_MESSAGE_QUEUE_CAPACITY; ++slot) {
        rx.value = stressPacket(burst * RECEIVED_MESSAGE_QUEUE_CAPACITY + slot);
        callback.onWrite(&rx);
      }
      if (burst % 2 == 0) {
        // Multiple rejected writes coalesce into one interruption event.
        rx.value = "overflow"; callback.onWrite(&rx); callback.onWrite(&rx);
      }
      burstReady.store(burst + 1);
    }
  });
  for (int burst = 0; burst < bursts; ++burst) {
    while (burstReady.load() != burst + 1) {
      assert(std::chrono::steady_clock::now() < burstDeadline);
      std::this_thread::yield();
    }
    const bool lost = burst % 2 == 0;
    assert(manager.takeReceiveLossEvent() == lost);
    assert(!manager.takeReceiveLossEvent());
    for (int slot = 0; slot < RECEIVED_MESSAGE_QUEUE_CAPACITY; ++slot) {
      assert(manager.hasReceivedMessage());
      const auto packet = manager.getReceivedMessage();
      if (lost) assert(packet.empty());
      else assert(packet == stressPacket(burst * RECEIVED_MESSAGE_QUEUE_CAPACITY + slot));
    }
    assert(!manager.hasReceivedMessage());
    burstDrained.store(burst + 1);
  }
  burstingProducer.join();
}
'''
functions = '\n'.join(function(sig) for sig in [
    'void BluetoothManager::updateBluetooth()',
    'bool BluetoothManager::isBluetoothConnected()',
    'const char *BluetoothManager::getBluetoothStatusText()',
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
print('BLE SPSC RX passed: 70,000 concurrent packets, 200 full bursts, overflow/recovery, allocation failure and reconnects.')
