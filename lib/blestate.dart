import 'package:flutter/material.dart';

class BleState {
  bool? connected;
  String? values;

  BleState(this.connected, this.values);
} 

class BleStateNotifier extends ValueNotifier<BleState> {

  BleStateNotifier(BleState state) : super(state);

  setValues(String? values) {
    value.values = values;
    notifyListeners();
  }
}

final bleStateNotifier = BleStateNotifier(BleState(false, ""));

