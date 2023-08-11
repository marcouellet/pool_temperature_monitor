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
  late bool _isInitialConnection;
  late bool _isReady;
  late bool _isLookingForDevice;
  late bool _isDeviceConnected;
  late List _sleeptempchargedata;
  late BleState _bleState;
  late int _notificationCopiesReceived;
  StreamSubscription<BluetoothDeviceState>? _deviceStateSubscription;
  BluetoothDevice? _device;
  Stream<List<int>>? _stream;
  int _waterTemp = 0;
  int _airTemp = 0;
  int _charge = 0;
  int _lowVoltageAlarm = 0;
  int _serverSleepDelay = 0;

  @override
  void initState()  {
    super.initState();
    _isInitialConnection = true;
    _isReady = false;
    _isLookingForDevice = false;
    _isDeviceConnected = false;
    _notificationCopiesReceived = 0;
    _bleState = BleUtils.getState();
    BleUtils.registerOnStateChange(_bleOnStateChanged);
    BleUtils.initStreams();
    _lookupForDevice();
  }

  @override
  void dispose() {
    _device!.disconnect();
    super.dispose();
  }

  void _bleOnStateChanged(BleState state) {
    setState(() { _bleState = state; });
  }

  void _tryLookupForDevices() {
    if (!_isLookingForDevice) {
       _lookupForDevice();
    }
  }

  void _cancelLookingForDevices() {
     setState(() { _isLookingForDevice = false; });
    BleUtils.cancelLookingForDevices();
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
        // If device is disconnected by server, set sleep delay to 0 to allow resync with client
        BleUtils.setSleepDelay(_isDeviceConnected ? 0 : _serverSleepDelay);
        _cancelDeviceStateMonitoring();
        await _disconnectDevice();
        _tryLookupForDevices();
      }
      if (event == BluetoothDeviceState.connected) {
        setState(() { _isInitialConnection = false; });
        if (!_isReady) {
          _discoverServices();
        }
      }
    });
  }

  String _deviceTitle() {
    // ignore: unnecessary_null_comparison
    return _device == null ? 'Aucun périphérique' : 
      '${_device!.name.substring(AppSettings.bleDeviceNamePrefix.length)} Sonde de temperature';
  }

  _disconnectDevice() async {
    if (_isDeviceConnected) {

      _isReady = false;
      _stream = const Stream.empty();
      _isDeviceConnected = false;

      await _device!.disconnect();
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

  Widget getBleWidget() {
    switch (_bleState) {
      case BleState.scanning:
        return const CircularProgressIndicator(              
                    color: Colors.white,
                    strokeWidth: 6,
                  );
      case BleState.idle:
        return const Icon(
                      Icons.done,
                      size: 40.0,
                      color: Colors.white54,
                    );
      case BleState.sleeping:
        return const Icon(
                      Icons.hourglass_top,
                      size: 40.0,
                      color: Colors.white54,
                    );
      case BleState.alarm:
        return const Icon(
                Icons.report_problem,
                size: 40.0,
                color: Colors.white54,
              );         
      case BleState.timeout:
        return const Icon(
                Icons.hourglass_empty,
                size: 40.0,
                color: Colors.white54,
              );      }
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
            child: getBleWidget()
          ),
        ]
      ),
      body: Container(
          child: _isInitialConnection
            ? _isLookingForDevice 
              ? Center(
                child: Column(
                  mainAxisAlignment: MainAxisAlignment.center,
                  children: [
                    const Text(
                      "En attente d'une connexion...",
                      style: TextStyle(fontSize: 24, color: Colors.red),
                    ),
                    ElevatedButton(
                      onPressed: () => _cancelLookingForDevices(),
                      child: const Text('Annuler le scan')),
                  ],
                ),
              )
              : Center(
                child: Column(
                  mainAxisAlignment: MainAxisAlignment.center,
                  children: [
                    const Text(
                      "Périphérique inaccessible",
                      style: TextStyle(fontSize: 24, color: Colors.red),
                    ),
                    ElevatedButton(
                      onPressed: () => _tryLookupForDevices(),
                      child: const Text('Reprendre le scan')),
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

              if (_isReady && snapshot.connectionState == ConnectionState.active && snapshot.hasData) {
                // geting data from bluetooth
                var currentValue = _dataParser(snapshot.data!);
                _sleeptempchargedata = currentValue.split(",");
                if(_sleeptempchargedata.length == 5) {
                  if (_sleeptempchargedata[0] != "nan") {
                    _serverSleepDelay = int.parse('${_sleeptempchargedata[0]}');
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
                  if (_sleeptempchargedata[4] != "nan") {
                    _lowVoltageAlarm = int.parse('${_sleeptempchargedata[4]}');
                  }   
                  _notificationCopiesReceived++;
                  if (_notificationCopiesReceived >= AppSettings.bleNotificationCopiesToReceive) {
                    _disconnectDevice();  
                  }         
                } 
              }
              return HomeUI(
                charge: _charge > 100 ? 100 : _charge,
                waterTemperature: _waterTemp,
                airTemperature: _airTemp,
                lowVoltageAlarm: _lowVoltageAlarm,
                sleepDelay: BleUtils.getSleepDelay(),
                isLookingForDevice: _isLookingForDevice,
              );
          },
        )
      ),
    );
  }
}
