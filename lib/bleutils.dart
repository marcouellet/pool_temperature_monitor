import 'dart:async';

import 'package:flutter_blue/flutter_blue.dart';
import 'package:pool_temperature_monitor/config/appsettings.dart';

class BleUtils {
  static final FlutterBlue _flutterBlue = FlutterBlue.instance;
  static List<BluetoothDevice> _devicesList = <BluetoothDevice>[];
  static StreamSubscription<List<BluetoothDevice>>? _connectedDevicesStreamSubscription;
  static StreamSubscription<List<ScanResult>>? _scanResultsStreamSubscription;
  static int _sleepdelay = AppSettings.bleLookupForDeviceRetryDelaySeconds;
  static bool _isLookingForDevice = false;
  static bool _isScanCancelled = false;
  static int lookupRetries = 0;

  static initStreams() { 
    _connectedDevicesStreamSubscription =_flutterBlue.connectedDevices
      .asStream()
      .listen((List<BluetoothDevice> devices) async {
        for (BluetoothDevice device in devices) {
        _addDeviceTolist(device);
      }
    });  
    _scanResultsStreamSubscription = _flutterBlue.scanResults.listen((List<ScanResult> results) {
      for (ScanResult result in results) {
        _addDeviceTolist(result.device);
      }
    });     
  }

  static _addDeviceTolist(final BluetoothDevice device) {
    if (device.type == BluetoothDeviceType.le && device.name.startsWith(AppSettings.bleDeviceNamePrefix)) {
      if (!_devicesList.contains(device)) {
          _devicesList.add(device);
      }
    }
  }

  static List<BluetoothDevice> deviceList() {
    return _devicesList;
  }

  static void setSleepDelay(int delay) {
    _sleepdelay = (delay / 2).round();
  }

  static Future<void> scanForDevices() async {

    _devicesList = <BluetoothDevice>[];  
    _isScanCancelled = false;

    await _flutterBlue.startScan(timeout: const Duration(seconds: AppSettings.bleDeviceScanDurationSeconds));              
  }

  static Future<void> cancelScanForDevices() async {
    _isScanCancelled = true;
  }

  static bool isScanCancelled() {
    return _isScanCancelled;
  }

  static bool isScanRunning() {
    return _connectedDevicesStreamSubscription != null || _scanResultsStreamSubscription != null;
  }

  static bool isLookingForDevice() {
    return _isLookingForDevice;
  }

  static Future<BluetoothDevice?> scanForDeviceName(String deviceName) async {
    await scanForDevices();
    return _devicesList.where((d) => d.name == deviceName).firstOrNull;
  }

  static Future<BluetoothDevice?> lookupForDevice() async {
    _isLookingForDevice = true;
    _isScanCancelled = false;
    int lookupRetries = 0;
    BluetoothDevice? device;

    while (device == null && !isScanCancelled() && lookupRetries < AppSettings.bleLookupForDeviceRetriesMaximum) {
      if (lookupRetries == 0) {
        await Future.delayed(Duration(seconds: _sleepdelay), () async {
          if (!isScanCancelled()) {
            device = await scanForDeviceName(AppSettings.bleDeviceName);
          }
        });
      } else 
      {
        await Future.delayed(const Duration(seconds: AppSettings.bleLookupForDeviceRetryDelaySeconds), () async {
          if (!isScanCancelled()) {
            device = await scanForDeviceName(AppSettings.bleDeviceName);
          }
        });
      }
      lookupRetries++;
    }
    _isLookingForDevice = false;
    return device;
  }
}