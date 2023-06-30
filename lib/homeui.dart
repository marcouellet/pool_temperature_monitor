import 'package:flutter/material.dart';
import 'package:flutter_svg/flutter_svg.dart';
import 'package:sleek_circular_slider/sleek_circular_slider.dart';

class HomeUI extends StatefulWidget {
  final double temperature;
  final double charge;
  const HomeUI({Key? key, required this.temperature, required this.charge}) : super(key: key);
  @override
  // ignore: library_private_types_in_public_api
  _HomeUIState createState() => _HomeUIState();
}

class _HomeUIState extends State<HomeUI> {
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
                              bottomLabelText: 'Température',
                              mainLabelStyle: TextStyle(
                                  color: HexColor('#54826D'),
                                  fontSize: 20.0,
                                  fontWeight: FontWeight.w600),
                              modifier: (double value) {
                                return '${widget.temperature} ˚C';
                              }),
                          startAngle: 90,
                          angleRange: 360,
                          size: 160.0,
                          animationEnabled: true),
                      min: 0,
                      max: 100,
                      initialValue: widget.temperature,
                    ),
                    const SizedBox(
                      height: 40,
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
                          size: 160.0,
                          animationEnabled: true),
                      min: 0,
                      max: 100,
                      initialValue: widget.charge,
                    ),
                    const SizedBox(
                      height: 20,
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
