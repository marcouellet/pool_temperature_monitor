import 'dart:async';

import 'package:flutter_blue/flutter_blue.dart';
import 'package:pool_temperature_monitor/config/appsettings.dart';

  enum BleState {
    idle,
    scanning,
    sleeping,
    alarm
  }

class BleUtils {
  static final FlutterBlue _flutterBlue = FlutterBlue.instance;
  static List<BluetoothDevice> _devicesList = <BluetoothDevice>[];
  static int _sleepdelay = 0; // Do not wait when app is started
  static BleState _blestate = BleState.idle;
  static Function(BleState state)? _onStateChange;
  static bool _isLookingForDeviceCancelled = false;

  static setState(BleState state) {
    _blestate = state;
    if (_onStateChange != null) {
      _onStateChange!(_blestate);
    }
  }

  static BleState getState() {
    return _blestate;
  }

  static registerOnStateChange(Function(BleState state)? onStateChange) {
    _onStateChange = onStateChange;
  }

  static initStreams() { 
    _flutterBlue.connectedDevices
      .asStream()
      .listen((List<BluetoothDevice> devices) async {
        for (BluetoothDevice device in devices) {
        _addDeviceTolist(device);
      }
    });  
    _flutterBlue.scanResults.listen((List<ScanResult> results) {
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
    _sleepdelay = delay.round() - 30;
    _sleepdelay = _sleepdelay > 0 ? _sleepdelay : 0;
  }

  static int scanForDeviceRetryCount() {
    if (_sleepdelay == 0) {
      return AppSettings.bleLookupForDeviceInitialRetriesMaximum;
    } else {
      return AppSettings.bleLookupForDeviceRetriesMaximum;
    }
  }

  static Future<void> scanForDevices() async {

    _devicesList = <BluetoothDevice>[];  
    _isLookingForDeviceCancelled = false;

    setState(BleState.scanning);
    await _flutterBlue.startScan(timeout: const Duration(seconds: AppSettings.bleDeviceScanDurationSeconds));
    setState(BleState.idle);            
  }

  static Future<void> cancelLookingForDevices() async {
    _isLookingForDeviceCancelled = true;
    setState(BleState.idle); 
  }

  static bool isLookingForDeviceCancelled() {
    return _isLookingForDeviceCancelled;
  }

  static Future<BluetoothDevice?> scanForDeviceName(String deviceName) async {
    await scanForDevices();
    return _devicesList.where((d) => d.name == deviceName).firstOrNull;
  }

  static Future<BluetoothDevice?> lookupForDevice() async {
    _isLookingForDeviceCancelled = false;
    int lookupRetries = 0;
    BluetoothDevice? device;

    while (device == null && !isLookingForDeviceCancelled() && lookupRetries < scanForDeviceRetryCount()) {
      if (lookupRetries == 0) {
        await Future.delayed(Duration(seconds: _sleepdelay), () async {
          if (!isLookingForDeviceCancelled()) {
            device = await scanForDeviceName(AppSettings.bleDeviceName);
          }
        });
      } else 
      {
        setState(BleState.sleeping);
        await Future.delayed(const Duration(seconds: AppSettings.bleLookupForDeviceRetryDelaySeconds), () async {
          setState(BleState.idle);
          if (!isLookingForDeviceCancelled()) {
            device = await scanForDeviceName(AppSettings.bleDeviceName);
          }
        });
      }
      lookupRetries++;
    }
    return device;
  }
}