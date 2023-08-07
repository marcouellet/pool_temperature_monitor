import 'package:flutter/material.dart';
import 'package:flutter_svg/flutter_svg.dart';
import 'package:sleek_circular_slider/sleek_circular_slider.dart';
import 'package:timer_count_down/timer_count_down.dart';

class HomeUI extends StatefulWidget {
  final int waterTemperature;
  final int airTemperature;
  final int charge;
  final int lowVoltageAlarm;
  final int sleepDelay;
  final bool isLookingForDevice;

  const HomeUI({Key? key, required this.waterTemperature,  required this.airTemperature, required this.charge, 
                required this.lowVoltageAlarm, required this.sleepDelay, required this.isLookingForDevice}) : super(key: key);
  @override
  // ignore: library_private_types_in_public_api
  _HomeUIState createState() => _HomeUIState();
}

class _HomeUIState extends State<HomeUI> {

  bool isSleeping() {
    return widget.isLookingForDevice;
  }

  int sleepDelay() {
    return widget.sleepDelay;
  }

  bool  isLowVoltageAlarm() {
    return widget.lowVoltageAlarm == 1;
  }

  String refreshStatus() {
    return isSleeping() ? "Maj des données en cours..." : "Données à jour";
  }

  int getWaterTemperatureSafeValue() {
    return (widget.waterTemperature < 0 || widget.waterTemperature > 100) ? 0 : widget.waterTemperature;
  }

  int getAirTemperatureSafeValue() {
    return (widget.airTemperature < 0 || widget.airTemperature > 100) ? 0 : widget.airTemperature;
  }

  int getChargeSafeValue() {
    return (widget.charge < 0 || widget.charge > 100) ? 0 : widget.charge;
  }

  int toFarenheit(int celcius) {
    double c = celcius.toDouble();
    double f = c * 9.0 / 5.0 + 32.0;
    return f.toInt();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      body: Center(
        child: Column(
          children: [
            SizedBox(
              height: 100,
              width: double.infinity,
              child: SvgPicture.asset(
                "assets/swimming-pool-swimming-svgrepo-com.svg",
                fit: BoxFit.fitHeight,
              ),
            ),
            Padding(
              padding: const EdgeInsets.only(
                left: 14,
                right: 14,
              ),
              child: Container(
                decoration: BoxDecoration(
                    color: Colors.grey[200],
                    border: Border.all(color: Colors.black)),
                width: double.infinity,
                child: Column(
                  children: [                   
                    Row(
                      mainAxisAlignment: MainAxisAlignment.center,
                      children: [
                        isLowVoltageAlarm() 
                        ? const Text(
                          'Alerte, batterie faible!',
                            style: TextStyle(fontSize: 20.0, color: Colors.redAccent),
                            textAlign: TextAlign.center,
                          )
                        : const SizedBox(
                          height: 10,
                        )
                      ],
                    ), 
                    Text(
                      'Température de l\'air ${getAirTemperatureSafeValue()} ˚C / ${toFarenheit(getAirTemperatureSafeValue())} ˚F',
                      style: const TextStyle(fontSize: 20.0, color: Colors.black),
                      textAlign: TextAlign.center,
                    ),
                    const SizedBox(
                      height: 20,
                    ),
                    SleekCircularSlider(
                      appearance: CircularSliderAppearance(
                          customWidths: CustomSliderWidths(
                              trackWidth: 4,
                              progressBarWidth: 10,
                              shadowWidth: 10),
                          customColors: CustomSliderColors(
                              trackColor: HexColor('#ef6c00'),
                              progressBarColor: HexColor('#ffb74d'),
                              shadowColor: HexColor('#ffb74d'),
                              shadowMaxOpacity: 0.5, //);
                              shadowStep: 20),
                          infoProperties: InfoProperties(
                              bottomLabelStyle: TextStyle(
                                  color: HexColor('#6DA100'),
                                  fontSize: 20,
                                  fontWeight: FontWeight.w600),
                              bottomLabelText: 'Tempér. eau',
                              mainLabelStyle: TextStyle(
                                  color: HexColor('#54826D'),
                                  fontSize: 20.0,
                                  fontWeight: FontWeight.w600),
                              modifier: (double value) {
                                return '${getWaterTemperatureSafeValue()} ˚C / ${toFarenheit(getWaterTemperatureSafeValue())} ˚F';
                              }),
                          startAngle: 90,
                          angleRange: 360,
                          size: 140.0,
                          animationEnabled: true),
                      min: 0,
                      max: 100,
                      initialValue: getWaterTemperatureSafeValue().toDouble(),
                    ),
                    const SizedBox(
                      height: 20,
                    ),
                    SleekCircularSlider(
                      appearance: CircularSliderAppearance(
                          customWidths: CustomSliderWidths(
                              trackWidth: 4,
                              progressBarWidth: 10,
                              shadowWidth: 10),
                          customColors: CustomSliderColors(
                              trackColor: HexColor('#0277bd'),
                              progressBarColor: HexColor('#4FC3F7'),
                              shadowColor: HexColor('#B2EBF2'),
                              shadowMaxOpacity: 0.5, //);
                              shadowStep: 20),
                          infoProperties: InfoProperties(
                              bottomLabelStyle: TextStyle(
                                  color: HexColor('#6DA100'),
                                  fontSize: 20,
                                  fontWeight: FontWeight.w600),
                              bottomLabelText: 'Charge',
                              mainLabelStyle: TextStyle(
                                  color: HexColor('#54826D'),
                                  fontSize: 20.0,
                                  fontWeight: FontWeight.w600),
                              modifier: (double value) {
                                return '${widget.charge} %';
                              }),
                          startAngle: 90,
                          angleRange: 360,
                          size: 140.0,
                          animationEnabled: true),
                      min: 0,
                      max: 100,
                      initialValue: getChargeSafeValue().toDouble(),
                    ),
                    const SizedBox(
                      height: 20,
                    ),
                    Row (
                      mainAxisAlignment: MainAxisAlignment.center,
                      children: [
                        isSleeping() && sleepDelay() > 0 ?
                          Countdown(
                          seconds: sleepDelay(),
                          build: (BuildContext context, double time) => 
                            time > 0 
                            ? Text("Prochaine maj dans ${time.toString()} secs",
                                    style: const TextStyle(fontSize: 20, color: Colors.blueAccent))
                            : Text(refreshStatus(),                           
                                    style: const TextStyle(fontSize: 20, color: Colors.blueAccent)),
                          interval: const Duration(seconds: 1),
                          )
                        : Text(
                            refreshStatus(),
                            style: const TextStyle(fontSize: 20, color: Colors.blueAccent),
                          ),
                        ],
                    )
                  ],
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }
}

class HexColor extends Color {
  static int _getColorFromHex(String hexColor) {
    hexColor = hexColor.toUpperCase().replaceAll('#', '');
    if (hexColor.length == 6) {
      hexColor = 'FF$hexColor';
    }
    return int.parse(hexColor, radix: 16);
  }

  HexColor(final String hexColor) : super(_getColorFromHex(hexColor));
}
