class AppSettings {
  static const String bleServiceUUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b";
  static const String bleCharacteristicsUUID = "beb5483e-36e1-4688-b7f5-ea07361b26a8";
  static const String bleDeviceName = "ESP32TM Pool"; 
  static const String bleDeviceNamePrefix = "ESP32TM";   
  static const int bleDisconnectFromDeviceDurationSeconds = 15;
  static const int bleDeviceScanDurationSeconds = 5;
  //static const int bleConnectToDeviceDurationSeconds = 10;
  static const int bleLookupForDeviceRetryDelaySeconds = 5;
  static const int bleLookupForDeviceRetriesMaximum = 6;
}