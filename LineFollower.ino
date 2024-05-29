#include <Wire.h>
#include <ZumoShield.h>

ZumoBuzzer buzzer;
ZumoReflectanceSensorArray reflectanceSensors(QTR_NO_EMITTER_PIN);
ZumoMotors motors;
Pushbutton button(ZUMO_BUTTON);
int lastError = 0;
const int MAX_SPEED = 100;

#define LED 13
#define QTR_THRESHOLD 1500 // Threshold for border detection

unsigned long previousTime = 0; // Variable to track previous time

void setup()
{
  Serial.begin(9600); // Initialize serial communication
  pinMode(LED, OUTPUT);
  buzzer.play(">g32>>c32");

  // Initialize the reflectance sensors module
  reflectanceSensors.init();

  // Wait for the user button to be pressed and released
  button.waitForButton();

  // Turn on LED to indicate we are in calibration mode
  digitalWrite(LED, HIGH);

  // Wait 1 second and then begin automatic sensor calibration
  // by rotating in place to sweep the sensors over the line
  delay(1000);
  int i;
  for (i = 0; i < 80; i++)
  {
    if ((i > 10 && i <= 30) || (i > 50 && i <= 70))
      motors.setSpeeds(-200, 200);
    else
      motors.setSpeeds(200, -200);
    reflectanceSensors.calibrate();

    // Since our counter runs to 80, the total delay will be
    // 80*20 = 1600 ms.
    delay(20);
  }
  motors.setSpeeds(0, 0);

  // Turn off LED to indicate we are through with calibration
  digitalWrite(LED, LOW);
  buzzer.play(">g32>>c32");

  // Wait for the user button to be pressed and released
  button.waitForButton();

  // Play music and wait for it to finish before we start driving.
  buzzer.play("L16 cdegreg4");
  while (buzzer.isPlaying());
}

void loop()
{
  unsigned int sensors[6];
  int position = reflectanceSensors.readLine(sensors);
  bool onLine = (position >= 2400 && position <= 2600);

  // Get current time in seconds
  unsigned long currentTime = millis() / 1000;

  // Calculate error and speed difference
  int error = position - 2500;
  int speedDifference = error / 4 + 6 * (error - lastError);
  lastError = error;

  // Get motor speeds
  int m1Speed = MAX_SPEED + speedDifference;
  int m2Speed = MAX_SPEED - speedDifference;

  // Constrain motor speeds
  if (m1Speed < 0) m1Speed = 0;
  if (m2Speed < 0) m2Speed = 0;
  if (m1Speed > MAX_SPEED) m1Speed = MAX_SPEED;
  if (m2Speed > MAX_SPEED) m2Speed = MAX_SPEED;

  // Set motor speeds
  motors.setSpeeds(m1Speed, m2Speed);

  // Determine direction
  String direction;
  if (speedDifference < 0) {
    direction = "left";
  } else if (speedDifference > 0) {
    direction = "right";
  } else {
    direction = "straight";
  }

  // Border detection
  String leftBorderStatus = (sensors[0] < QTR_THRESHOLD) ? "close" : "safe";
  String rightBorderStatus = (sensors[5] < QTR_THRESHOLD) ? "close" : "safe";

  // Send timestamp, status, direction, and border distance over serial every 1 second
  if (currentTime - previousTime >= 1) {
    previousTime = currentTime; // Update previous time
    Serial.print(currentTime); // Send timestamp
    Serial.print(",");
    Serial.print(onLine ? "1" : "0"); // Send status '1' for on the line and '0' for off the line
    Serial.print(",");
    Serial.print(direction); // Send direction
    Serial.print(",");
    Serial.print(leftBorderStatus); // Send left border status
    Serial.print(",");
    Serial.println(rightBorderStatus); // Send right border status
  }
}
