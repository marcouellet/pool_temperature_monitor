import 'dart:async';
import 'dart:convert' show utf8;

import 'package:flutter/material.dart';
import 'package:flutter_blue/flutter_blue.dart';
import 'package:pool_temperature_monitor/config/appsettings.dart';
import 'package:pool_temperature_monitor/bleutils.dart';

import 'homeui.dart';

class SensorPage extends StatefulWidget {
  const SensorPage({Key? key}) : super(key: key);

  @override
  // ignore: library_private_types_in_public_api
  _SensorPageState createState() => _SensorPageState();
}

class _SensorPageState extends State<SensorPage> {
  late bool _isReady;
  late bool _isLookingForDevice;
  late bool _isDeviceConnected;
  late List _sleeptempchargedata;
  StreamSubscription<BluetoothDeviceState>? _deviceStateSubscription;
  BluetoothDevice? _device;
  Stream<List<int>>? _stream;
  int _waterTemp = 0;
  int _airTemp = 0;
  int _charge = 0;

  @override
  void initState()  {
    super.initState();
    _isReady = false;
    _isLookingForDevice = false;
    _isDeviceConnected = false;
    BleUtils.initStreams();
    _lookupForDevice();
  }

  @override
  void dispose() {
    _device!.disconnect();
    super.dispose();
  }

  void _tryLookupForDevices() {
    if (!_isLookingForDevice) {
       _lookupForDevice();
    }
  }

  Future<void> _lookupForDevice() async {
    setState(() { _isLookingForDevice = true; });
    _device = await BleUtils.lookupForDevice();
    if (_device != null) {
      await _device!.connect();
      _monitorDeviceState();
      _isDeviceConnected = true;
    } else {
      setState(() { _isReady = false; });
    }
    setState(() { _isLookingForDevice = false; });
  }

  void _cancelDeviceStateMonitoring() {
    if (_deviceStateSubscription != null) {
      _deviceStateSubscription!.cancel();
      _deviceStateSubscription = null;
    }
  }

  void _monitorDeviceState() async {
    _cancelDeviceStateMonitoring();
    _deviceStateSubscription = _device!.state.listen((event) async {
      if (event == BluetoothDeviceState.disconnected) {
        _cancelDeviceStateMonitoring();
        await _disconnectDevice();
        _tryLookupForDevices();
      }
      if (event == BluetoothDeviceState.connected) {
        _discoverServices();
      }
     });
  }

  String _deviceTitle() {
    // ignore: unnecessary_null_comparison
    return _device == null ? 'No device' : 
      '${_device!.name.substring(AppSettings.bleDeviceNamePrefix.length)} Temperature Sensor';
  }

  _disconnectDevice() async {
    if (_isDeviceConnected) {
      await _device!.disconnect();
      _isDeviceConnected = false;
    }
  }

  _discoverServices() async {
    List<BluetoothService> services = await _device!.discoverServices();
    services.forEach((service) {
      if (service.uuid.toString() == AppSettings.bleServiceUUID) {
        service.characteristics.forEach((characteristic) {
          if (characteristic.uuid.toString() == AppSettings.bleCharacteristicsUUID) {
            characteristic.setNotifyValue(true);
            setState(() {
              _stream = characteristic.value;
              _isReady = true;
            });
          }
        });
      }
    });
  }
  
  String _dataParser(List<int> dataFromDevice) {
    return utf8.decode(dataFromDevice);
  }

  @override
  Widget build(BuildContext context) {
     return Scaffold(
        appBar: AppBar(
          title: Text(_deviceTitle()),      
          actions: <Widget>[
            Container(
              padding: const EdgeInsets.all(8.0),
              child: _isLookingForDevice 
              ? const CircularProgressIndicator(              
                  color: Colors.white,
                  strokeWidth: 6,
                )
              : IconButton(
                  padding: EdgeInsets.zero,
                  iconSize: 40,
                  icon: const Icon(Icons.sync),
                  onPressed: () { _tryLookupForDevices(); }
                ),
            ),
          ]
        ),
        body: Container(
            child: !_isReady
              ? _isLookingForDevice ? 
                Center(
                  child: Column(
                    mainAxisAlignment: MainAxisAlignment.center,
                    children: [
                      const Text(
                        "Waiting for connection...",
                        style: TextStyle(fontSize: 24, color: Colors.red),
                      ),
                      ElevatedButton(
                        onPressed: () => BleUtils.cancelScanForDevices(),
                        child: const Text('Cancel scan for device')),
                    ],
                  ),
                )
              : Center(
                  child: Column(
                    mainAxisAlignment: MainAxisAlignment.center,
                    children: [
                      const Text(
                        "Device not reachable",
                        style: TextStyle(fontSize: 24, color: Colors.red),
                      ),
                      ElevatedButton(
                        onPressed: () => _tryLookupForDevices(),
                        child: const Text('Retry scan for device')),
                    ],
                  ),
                )
                : StreamBuilder<List<int>>(
                  stream: _stream,
                  builder: (BuildContext context,
                             AsyncSnapshot<List<int>> snapshot) {
                  if (snapshot.hasError)
                  {
                    return Text('Error: ${snapshot.error}');
                  }

                  if (snapshot.connectionState ==
                      ConnectionState.active && snapshot.hasData) {
                    // geting data from bluetooth
                    var currentValue = _dataParser(snapshot.data!);
                    _sleeptempchargedata = currentValue.split(",");
                    if(_sleeptempchargedata.length == 3) {
                      if (_sleeptempchargedata[0] != "nan") {
                        BleUtils.setSleepDelay(int.parse('${_sleeptempchargedata[0]}'));
                      }
                      if (_sleeptempchargedata[1] != "nan") {
                        _waterTemp = int.parse('${_sleeptempchargedata[1]}');
                      }
                      if (_sleeptempchargedata[2] != "nan") {
                        _charge = int.parse('${_sleeptempchargedata[2]}');
                      }
                    } 
                    if(_sleeptempchargedata.length == 4) {
                      if (_sleeptempchargedata[0] != "nan") {
                        BleUtils.setSleepDelay(int.parse('${_sleeptempchargedata[0]}'));
                      }
                      if (_sleeptempchargedata[1] != "nan") {
                        _waterTemp = int.parse('${_sleeptempchargedata[1]}');
                      }
                      if (_sleeptempchargedata[2] != "nan") {
                        _airTemp = int.parse('${_sleeptempchargedata[2]}');
                      }
                      if (_sleeptempchargedata[3] != "nan") {
                        _charge = int.parse('${_sleeptempchargedata[3]}');
                      }
                    } 
                  }
                  return HomeUI(
                      charge: _charge > 100 ? 100 : _charge,
                      waterTemperature: _waterTemp,
                      airTemperature: _airTemp,
                    );
                },
              )
          ),
      );
  }
}
