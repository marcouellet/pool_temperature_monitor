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
  late bool isReady;
  late bool isLookingForDevice;
  late bool isDeviceConnected;
  late List _sleeptempchargedata;
  StreamSubscription<BluetoothDeviceState>? deviceStateSubscription;
  BluetoothDevice? device;
  Stream<List<int>>? stream;

  int _temp = 0;
  int _charge = 0;

  @override
  void initState()  {
    super.initState();
    isReady = false;
    isLookingForDevice = false;
    isDeviceConnected = false;
    BleUtils.initStreams();
    lookupForDevice();
  }

  @override
  void dispose() {
    device!.disconnect();
    super.dispose();
  }

  void tryLookupForDevices() {
    if (!isLookingForDevice) {
       lookupForDevice();
    }
  }

  Future<void> lookupForDevice() async {
    setState(() { isLookingForDevice = true; });
    device = await BleUtils.lookupForDevice();
    if (device != null) {
      await device!.connect();
      monitorDeviceState();
      isDeviceConnected = true;
    } else {
      setState(() { isReady = false; });
    }
    setState(() { isLookingForDevice = false; });
  }

  void cancelDeviceStateMonitoring() {
    if (deviceStateSubscription != null) {
      deviceStateSubscription!.cancel();
      deviceStateSubscription = null;
    }
  }

  void monitorDeviceState() async {
    cancelDeviceStateMonitoring();
    deviceStateSubscription = device!.state.listen((event) async {
      if (event == BluetoothDeviceState.disconnected) {
        cancelDeviceStateMonitoring();
        await disconnectDevice();
        tryLookupForDevices();
      }
      if (event == BluetoothDeviceState.connected) {
        discoverServices();
      }
     });
  }

  String deviceTitle() {
    // ignore: unnecessary_null_comparison
    return device == null ? 'No device' : 
      '${device!.name.substring(AppSettings.bleDeviceNamePrefix.length)} Temperature Sensor';
  }

  disconnectDevice() async {
    if (isDeviceConnected) {
      await device!.disconnect();
      isDeviceConnected = false;
    }
  }

  disconnectFromDevice() {
    device!.disconnect();
    isReady = false;
  }

  discoverServices() async {
    List<BluetoothService> services = await device!.discoverServices();
    services.forEach((service) {
      if (service.uuid.toString() == AppSettings.bleServiceUUID) {
        service.characteristics.forEach((characteristic) {
          if (characteristic.uuid.toString() == AppSettings.bleCharacteristicsUUID) {
            characteristic.setNotifyValue(true);
            setState(() {
              stream = characteristic.value;
              isReady = true;
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
          title: Text(deviceTitle())),
        body: Container(
            child: !isReady
              ? isLookingForDevice ? 
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
                        onPressed: () => tryLookupForDevices(),
                        child: const Text('Retry scan for device')),
                    ],
                  ),
                )
                : StreamBuilder<List<int>>(
                  stream: stream,
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
                        _temp = int.parse('${_sleeptempchargedata[1]}');
                      }
                      if (_sleeptempchargedata[2] != "nan") {
                        _charge = int.parse('${_sleeptempchargedata[2]}');
                      }
                    } 
                  }
                  return HomeUI(
                      charge: _charge,
                      temperature: _temp,
                    );
                },
              )
          ),
      );
  }
}
