# ESP #2 / iPhone setup — approval required before deployment

Verified sources: firmware `ee5b0c6` on `codex/firmware-production-fixes`, mobile
`9fd25f7` on `codex/mobile-production-fixes`. All listed completed fixes are
ancestors. Firmware's existing relocation into `scrbrd/` is preserved. Main and
application sources are unchanged. Remote branches are older (firmware ahead 3,
mobile ahead 5); use the local artifacts below. No push was performed.

## Verified artifacts

`build/hardware-validation/` in this firmware worktree contains the firmware,
ELF/map, unsigned iOS apps, signed `SCRBRD-Lab.app`, logs and `manifest.json`
with checksums/fix ancestry. `lab-signed-checksums.json` covers the signed app.

- Firmware: `esp32:esp32:esp32s3`, Arduino CLI 1.5.1, core 3.3.11, ArduinoJson
  7.4.3, GFX 1.6.7. Flash 780,585/1,310,720; static RAM 139,912/327,680 bytes.
  The 187,768-byte linker remainder is not measured runtime free heap.
- Mobile: Flutter 3.47.0 / Dart 3.13.0, Xcode 27.0. Unsigned arm64 iOS Release
  build passed (25.0 MB, minimum iOS 15.6). Locked Dart/Swift dependencies match
  source. Full 657 tests and analysis pass; ten native firmware entry points pass,
  with applicable ASan/UBSan. Current serializers/actual firmware agree on BLE
  name/UUIDs and 18 steps/112 packets, including rejected/incomplete transactions.
- Device identification now passed after the user connected only ESP #2 and
  unlocked the iPhone. ESP #2: `/dev/cu.usbmodem101`, USB serial
  `AC:A7:04:2A:19:94` (Espressif VID 303A / PID 1001). The iPhone 16 runs iOS
  26.6.2 with Developer Mode enabled. Its working Xcode/Flutter app is
  `com.againstthespread.scrbrd`, version 1.0.0 (6): the default experimental build
  would collide. Recheck the port/serial immediately before any approved upload.

## Flash only after explicit approval for ESP #2

1. ESP #1 stays powered off/unplugged. Close serial monitors. With only ESP #2
   attached, use the command below and confirm its port by unplug/replug. Do not
   select a port merely because its name looks plausible.

```sh
"/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli" board list
```

2. The following upload completed successfully once after explicit user approval
   for ESP #2 and firmware `ee5b0c6`. It is retained here as the exact command:

```sh
"/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli" upload   --fqbn esp32:esp32:esp32s3   --port /dev/cu.usbmodem101   --input-dir /Users/peterdeluca/Projects/_worktrees/scrbrd-codex/build/hardware-validation/firmware   /Users/peterdeluca/Projects/_worktrees/scrbrd-codex/scrbrd
```

3. Require esptool's successful write/hash-verification output. Reset/power-cycle
   only ESP #2 if needed to exit download mode. Recheck the runtime port, then:

```sh
"/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli" monitor   --port '<CONFIRMED_ESP2_RUNTIME_PORT>' --config baudrate=115200 --timestamp
```

If bootloader entry fails, hold BOOT while resetting (or while plugging USB if
there is no reset button), then release BOOT after enumeration and reidentify the
port. Do not erase the whole flash or alter eFuses/board settings. This sequence
follows [Espressif's ESP32-S3 boot-mode guidance](https://docs.espressif.com/projects/esptool/en/latest/esp32s3/advanced-topics/boot-mode-selection.html).

## Install iOS without replacing the working app

The default build is `com.againstthespread.scrbrd`, display name `Sports Hub Mobile`,
version 1.0.0 (6). **Do not run/install it yet**: matching the working app's bundle
ID would replace that installation. First unlock the iPhone, confirm the identified iPhone 16 is your test phone,
and read its SCRBRD app inventory. If the working app uses a different ID, a separate Lab identity may
be unnecessary; compare the IDs before choosing a route.

Recommended collision-safe route when the IDs match:

1. Use the isolated exact-source build copy at
   `/tmp/scrbrd-hardware-validation/ios-source` (recreate from commit `9fd25f7` if
   temporary files were removed). Do not change the main or experimental source
   app IDs for this bench session. Open its `ios/Runner.xcworkspace` in Xcode.
2. In that copy only, set Runner's Debug/Profile/Release bundle identifier to
   `com.againstthespread.scrbrd.lab`, and Info.plist's display name to `SCRBRD Lab`.
   Keep the existing Bluetooth/background capabilities. Select the existing team
   `37Y2R545AR`, automatic signing, and the connected iPhone. Do not press Run yet.
3. Provision the Lab App ID with the required Push Notifications capability and
   iPhone included. Xcode automatic signing has now created/downloaded a matching
   Lab development profile, including this iPhone and `aps-environment=development`.
   The profile expires October 15, 2027. See
   [Apple's development profile instructions](https://developer.apple.com/help/account/provisioning-profiles/create-a-development-provisioning-profile).
4. Register an additional iOS app for the Lab bundle ID in existing Firebase
   project `scrbrd-45109`, then use its matching configuration in the copy's
   `lib/firebase_options.dart` iOS block (and matching plist if added to the target).
   Merely renaming the current app's plist/bundle-ID string is insufficient. The
   user-authorized separate Firebase Lab registration has now been created and its
   matching configuration applied in the isolated copy; the original Firebase app
   remains unchanged.
   [Firebase requires a registered app/configuration for each bundle ID](https://firebase.google.com/docs/ios/setup).
5. Build the copy in Release, with matching signing, using Xcode Product > Build
   (not Run). Verify the built app's bundle ID/display name, signed application
   identifier and provisioning profile all match Lab; its ID must differ from the
   working app. After the user accepted Apple's updated agreement, the signed
   Release build passed: `SCRBRD Lab`, `com.againstthespread.scrbrd.lab`, version
   1.0.0 (1006). The saved `build/hardware-validation/SCRBRD-Lab.app` signature
   verifies, with application identifier
   `37Y2R545AR.com.againstthespread.scrbrd.lab` and the matching device profile.
   The working app remains installed as version 1.0.0 (6). Lab installation and
   side-by-side verification were subsequently completed as recorded in step 6.
6. **After explicit install approval**, use Xcode Run with scheme Run configuration
   Release, or install the reviewed signed app with the command below. Do not
   uninstall the working app. Lab starts with separate settings/league setup;
   configure it independently and leave the working app disconnected during tests.

   Completed with explicit user approval: Lab installed and launched without a
   debugger. A subsequent iPhone inventory confirms Lab 1.0.0 (1006) alongside
   the working Sports Hub Mobile 1.0.0 (6). No uninstall was performed.

```sh
xcrun devicectl device install app --device 00008140-000C7462022B001C /Users/peterdeluca/Projects/_worktrees/scrbrd-codex/build/hardware-validation/SCRBRD-Lab.app
```

For lock/background/soak tests use the signed Release build without a debugger
attached. BLE Fantasy alerts and iOS/APNs push notifications are separate paths;
Lab push needs its own correct registration/token targeting before push results
can be qualified. Reusing the working app's ID is an explicit replacement option,
not the default. Do not uninstall either app or export credentials for this test.
