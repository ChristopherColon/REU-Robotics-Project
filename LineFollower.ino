#include <Wire.h>
#include <ZumoShield.h>

ZumoBuzzer buzzer;
ZumoReflectanceSensorArray reflectanceSensors;
ZumoMotors motors;
Pushbutton button(ZUMO_BUTTON);
int lastError = 0;

// This is the maximum speed the motors will be allowed to turn.
// (100 lets the motors go at a controlled speed; increase to go faster)
const int MAX_SPEED = 100;

unsigned long previousTime = 0; // Variable to track previous time

#define LED 13
#define QTR_THRESHOLD 1500 // Threshold for border detection

// Variables for Data Streamer
int exampleVariable = 0;
int sensorPin = A0;

const byte kNumberOfChannelsFromExcel = 6; 
const char kDelimiter = ',';    
const int kSerialInterval = 50;   
unsigned long serialPreviousTime; 

char* arr[kNumberOfChannelsFromExcel];

void setup() {
  // Initialize serial communication
  Serial.begin(9600);

  // Play a little welcome song
  buzzer.play(">g32>>c32");

  // Initialize the reflectance sensors module
  reflectanceSensors.init();

  // Wait for the user button to be pressed and released
  button.waitForButton();

  // Turn on LED to indicate we are in calibration mode
  pinMode(LED, OUTPUT);
  digitalWrite(LED, HIGH);

  // Wait 1 second and then begin automatic sensor calibration
  // by rotating in place to sweep the sensors over the line
  delay(1000);
  int i;
  for (i = 0; i < 80; i++) {
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

void loop() {
  unsigned int sensors[6];

  // Get the position of the line. Note that we *must* provide the "sensors"
  // argument to readLine() here, even though we are not interested in the
  // individual sensor readings
  int position = reflectanceSensors.readLine(sensors);

  // Our "error" is how far we are away from the center of the line, which
  // corresponds to position 2500.
  int error = position - 2500;

  // Determine if we are on the line or off the line
  bool onLine = (sensors[1] > 200 || sensors[2] > 200 || sensors[3] > 200 || sensors[4] > 200);

  // Get current time in seconds
  unsigned long currentTime = millis() / 1000;

  // Get motor speed difference using proportional and derivative PID terms
  // (the integral term is generally not very useful for line following).
  // Here we are using a proportional constant of 1/4 and a derivative
  // constant of 6, which should work decently for many Zumo motor choices.
  // You probably want to use trial and error to tune these constants for
  // your particular Zumo and line course.
  int speedDifference = error / 4 + 6 * (error - lastError);

  lastError = error;

  // Get individual motor speeds. The sign of speedDifference
  // determines if the robot turns left or right.
  int m1Speed = MAX_SPEED + speedDifference;
  int m2Speed = MAX_SPEED - speedDifference;

  // Here we constrain our motor speeds to be between 0 and MAX_SPEED.
  // Generally speaking, one motor will always be turning at MAX_SPEED
  // and the other will be at MAX_SPEED-|speedDifference| if that is positive,
  // else it will be stationary. For some applications, you might want to
  // allow the motor speed to go negative so that it can spin in reverse.
  if (m1Speed < 0)
    m1Speed = 0;
  if (m2Speed < 0)
    m2Speed = 0;
  if (m1Speed > MAX_SPEED)
    m1Speed = MAX_SPEED;
  if (m2Speed > MAX_SPEED)
    m2Speed = MAX_SPEED;

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
  bool leftBorderClose = sensors[0] < QTR_THRESHOLD && sensors[1] < QTR_THRESHOLD;
  bool rightBorderClose = sensors[4] < QTR_THRESHOLD && sensors[5] < QTR_THRESHOLD;

  String leftBorderStatus = leftBorderClose ? "danger" : "safe";
  String rightBorderStatus = rightBorderClose ? "danger" : "safe";

  // Send timestamp, status, direction, and border distance over serial every 1 second
  if (currentTime - previousTime >= 1) {
    previousTime = currentTime; // Update previous time
    Serial.print(currentTime); // Send timestamp
    Serial.print(",");
    Serial.print(onLine ? "onLine" : "offLine"); // Send status '1' for on the line and '0' for off the line
    Serial.print(",");
    Serial.print(direction); // Send direction
    Serial.print(",");
    Serial.print(leftBorderStatus); // Send left border status
    Serial.print(",");
    Serial.print(rightBorderStatus); // Send right border status
    // Serial.print(",");
    // Serial.print(exampleVariable); // Send example variable
    Serial.println(); // New line for each set of data
  }

  // Read sensor data and process for Data Streamer
  // processSensors();
  processIncomingSerial();
  processOutgoingSerial();
}

// void processSensors() {
  // Read sensor pin and store to a variable
  // exampleVariable = analogRead(sensorPin);
// }

void sendDataToSerial() {
  // Send data out separated by a comma (kDelimiter)
  // Serial.print(exampleVariable);
  // Serial.print(kDelimiter);

  // Add final line ending character only once
  Serial.println(); 
}

void processOutgoingSerial() {
  // Enter into this only when serial interval has elapsed
  if ((millis() - serialPreviousTime) > kSerialInterval) {
    // Reset serial interval timestamp
    serialPreviousTime = millis();
    sendDataToSerial();
  }
}

void processIncomingSerial() {
  if (Serial.available()) {
    parseData(GetSerialData());
  }
}

char* GetSerialData() {
  static char inputString[64]; // Create a char array to store incoming data
  memset(inputString, 0, sizeof(inputString)); // Clear the memory from a previous reading
  while (Serial.available()) {
    Serial.readBytesUntil('\n', inputString, 64); // Read every byte in Serial buffer until line end or 64 bytes
  }
  return inputString;
}

void parseData(char data[]) {
  char *token = strtok(data, ","); // Find the first delimiter and return the token before it
  int index = 0; // Index to track storage in the array
  while (token != NULL) { // Char* strings terminate w/ a Null character. We'll keep running the command until we hit it
    arr[index] = token; // Assign the token to an array
    token = strtok(NULL, ","); // Continue to the next delimiter
    index++; // Increment index to store next value
  }
}
