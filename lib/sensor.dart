import 'dart:async';
import 'dart:convert' show utf8;

import 'package:flutter/material.dart';
import 'package:flutter_blue/flutter_blue.dart';
import 'package:global_configs/global_configs.dart';

import 'homeui.dart';

class SensorPage extends StatefulWidget {
  const SensorPage({Key? key, required this.device}) : super(key: key);
  final BluetoothDevice device;

  @override
  // ignore: library_private_types_in_public_api
  _SensorPageState createState() => _SensorPageState();
}

class _SensorPageState extends State<SensorPage> {
  // ignore: non_constant_identifier_names
  final String service_uuid = GlobalConfigs().get('app.config.ble_service_uuid'); 
  // ignore: non_constant_identifier_names
  final String characteristic_uuid = GlobalConfigs().get('app.config.ble_characteristic_uuid'); 
  // ignore: non_constant_identifier_names
  final int disconnect_from_device_duration_seconds = GlobalConfigs().get('app.config.ble_disconnect_from_device_duration_seconds'); 
  bool? isReady;
  Stream<List<int>>? stream;
  List? _tempchargedata;
  double _temp = 0;
  double _charge = 0;
  @override
  void initState() {
    super.initState();
    isReady = false;
    connectToDevice();
  }

  @override
  void dispose() {
    widget.device.disconnect();
    super.dispose();
  }

  connectToDevice() async {
    // ignore: unnecessary_null_comparison
    if (widget.device == null) {
      _pop();
      return;
    }

    Timer(Duration(seconds: disconnect_from_device_duration_seconds!), () {
      if (!isReady!) {
        disconnectFromDevice();
        _pop();
      }
    });

    await widget.device.connect();
    discoverServices();
  }

  disconnectFromDevice() {
    // ignore: unnecessary_null_comparison
    if (widget.device == null) {
      _pop();
      return;
    }

    widget.device.disconnect();
  }

  discoverServices() async {
    // ignore: unnecessary_null_comparison
    if (widget.device == null) {
      _pop();
      return;
    }

    List<BluetoothService> services = await widget.device.discoverServices();
    services.forEach((service) {
      if (service.uuid.toString() == service_uuid) {
        service.characteristics.forEach((characteristic) {
          if (characteristic.uuid.toString() == characteristic_uuid) {
            characteristic.setNotifyValue(!characteristic.isNotifying);
            stream = characteristic.value;

            setState(() {
              isReady = true;
            });
          }
        });
      }
    });

    if (!isReady!) {
      _pop();
    }
  }
  
  _pop() {
    Navigator.of(context).pop(true);
  }

  String _dataParser(List<int> dataFromDevice) {
    return utf8.decode(dataFromDevice);
  }

  @override
  Widget build(BuildContext context) {
    return WillPopScope(
      onWillPop: () async {
        bool confirm = await showDialog(
          context: context,
          builder: (BuildContext context) {
            return AlertDialog(
              title: const Text('Are you sure?'),
              content: const Text('Do you want to disconnect device and go back?'),
              actions: <Widget>[
                TextButton(
                    onPressed: () => Navigator.of(context).pop(false),
                    child: const Text('No')),
                TextButton(
                    onPressed: () {
                      disconnectFromDevice();
                      Navigator.of(context).pop(true);
                    },
                    child: const Text('Yes')),
              ],
            );
          },
        );
        return confirm;
      },
      child: Scaffold(
        appBar: AppBar(
          title: const Text('dht11 Sensor'),
        ),
        body: Container(
            child: !isReady!
                ? const Center(
                    child: Text(
                      "Waiting...",
                      style: TextStyle(fontSize: 24, color: Colors.red),
                    ),
                  )
                : Container(
                    child: StreamBuilder<List<int>>(
                      stream: stream,
                      builder: (BuildContext context,
                          AsyncSnapshot<List<int>> snapshot) {
                        if (snapshot.hasError)
                        {
                          return Text('Error: ${snapshot.error}');
                        }

                        if (snapshot.connectionState ==
                            ConnectionState.active) {
                          // geting data from bluetooth
                          var currentValue = _dataParser(snapshot.data!);
                          _tempchargedata = currentValue.split(",");
                          if(_tempchargedata!.length == 2) {
                            if (_tempchargedata![0] != "nan") {
                              _temp = double.parse('${_tempchargedata![0]}');
                            }
                            if (_tempchargedata![1] != "nan") {
                              _charge = double.parse('${_tempchargedata![1]}');
                            }
                          }

                          return HomeUI(
                            charge: _charge,
                            temperature: _temp,
                          );
                        } else {
                          return const Text('Check the stream');
                        }
                      },
                    ),
                  )),
      ),
    );
  }
}
