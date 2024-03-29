/* This code is based on adfruit library and
   uses BLE to transfer distance measured by
   JSN-SR04T-V2 Ultrasonic Sensor.
   The range readings are in units of mm. */


#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <bluefruit.h>

/* use SR04T's searial port instead
   JSN-SR04T-v2 R27=47kohm, Arduino Serial1
*/

//int trigPin = 11;
//int echoPin = 10;
//long duration;
//int distance;


// define serial message
int index_n = 0;
byte message[4];
uint16_t dist;


// BLE Service
BLEDfu  bledfu;  // OTA DFU service
BLEDis  bledis;  // device information
BLEUart bleuart; // uart over ble
BLEBas  blebas;  // battery

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels
// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     4 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
//Adafruit_SSD1306 display = Adafruit_SSD1306();

#if (SSD1306_LCDHEIGHT != 32)
 #error("Height incorrect, please fix Adafruit_SSD1306.h!");
#endif


void setup()
{
//  Serial.begin(19200);
//  while ( !Serial ) delay(10);   // for nrf52840 with native usb

  // Setup the BLE LED to be enabled on CONNECT
  // Note: This is actually the default behaviour, but provided
  // here in case you want to control this LED manually via PIN 19
  Bluefruit.autoConnLed(true);

  // Config the peripheral connection with maximum bandwidth
  // more SRAM required by SoftDevice
  // Note: All config***() function must be called before begin()
  Bluefruit.configPrphBandwidth(BANDWIDTH_MAX);

  Bluefruit.begin();
  Bluefruit.setTxPower(4);    // Check bluefruit.h for supported values
  Bluefruit.setName("SI-Ultrasonic");

  Bluefruit.Periph.setConnectCallback(connect_callback);
  Bluefruit.Periph.setDisconnectCallback(disconnect_callback);

  // To be consistent OTA DFU should be added first if it exists
  bledfu.begin();

  // Configure and Start Device Information Service
  bledis.setManufacturer("Adafruit Industries");
  bledis.setModel("Bluefruit Feather52");
  bledis.begin();

  // Configure and Start BLE Uart Service
  bleuart.begin();

  // Start BLE Battery Service
  blebas.begin();
  blebas.write(100);

  // Set up and start advertising
  startAdv();

  // initilize oled
//  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 128x32)
//  // init done
//  display.display();
//  delay(1000);

//  Wire.begin();

//  sensor.init();
//  sensor.setTimeout(500);

//#if defined LONG_RANGE
//  // lower the return signal rate limit (default is 0.25 MCPS)
//  sensor.setSignalRateLimit(0.1);
//  // increase laser pulse periods (defaults are 14 and 10 PCLKs)
//  sensor.setVcselPulsePeriod(VL53L0X::VcselPeriodPreRange, 18);
//  sensor.setVcselPulsePeriod(VL53L0X::VcselPeriodFinalRange, 14);
//#endif
//
//#if defined HIGH_SPEED
//  // reduce timing budget to 20 ms (default is about 33 ms)
//  sensor.setMeasurementTimingBudget(20000);
//#elif defined HIGH_ACCURACY
//  // increase timing budget to 200 ms
//  sensor.setMeasurementTimingBudget(200000);
//#endif

  // text display big!
//  display.setTextSize(4);
//  display.setTextColor(WHITE);

//  pinMode(trigPin, OUTPUT);
//  pinMode(echoPin, INPUT);
//  digitalWrite(trigPin, LOW);
//  delayMicroseconds(500);

  // Use Serial1 to retrive distance from sensor
  Serial1.begin(9600);

}


void startAdv(void)
{
  // Advertising packet
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();

  // Include the BLE UART (AKA 'NUS') 128-bit UUID
  Bluefruit.Advertising.addService(bleuart);

  // Secondary Scan Response packet (optional)
  // Since there is no room for 'Name' in Advertising packet
  Bluefruit.ScanResponse.addName();

  /* Start Advertising
   * - Enable auto advertising if disconnected
   * - Interval:  fast mode = 20 ms, slow mode = 152.5 ms
   * - Timeout for fast mode is 30 seconds
   * - Start(timeout) with timeout = 0 will advertise forever (until connected)
   *
   * For recommended advertising interval
   * https://developer.apple.com/library/content/qa/qa1931/_index.html
   */
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244);    // in unit of 0.625 ms
  Bluefruit.Advertising.setFastTimeout(30);      // number of seconds in fast mode
  Bluefruit.Advertising.start(0);                // 0 = Don't stop advertising after n seconds
}


#define PACKET_ACC_LEN                  (15)
#define PACKET_GYRO_LEN                 (15)
#define PACKET_MAG_LEN                  (15)
#define PACKET_QUAT_LEN                 (19)
#define PACKET_BUTTON_LEN               (5)
#define PACKET_COLOR_LEN                (6)
#define PACKET_LOCATION_LEN             (15)

//    READ_BUFSIZE            Size of the read buffer for incoming packets
#define READ_BUFSIZE                    (20)


/* Buffer to hold incoming characters */
uint8_t packetbuffer[READ_BUFSIZE+1];

/**************************************************************************/
/*!
    @brief  Waits for incoming data and parses it
*/
/**************************************************************************/
uint8_t readPacket(BLEUart *ble_uart, uint16_t timeout)
{
  uint16_t origtimeout = timeout, replyidx = 0;

  memset(packetbuffer, 0, READ_BUFSIZE);

  while (timeout--) {
    if (replyidx >= 20) break;
    if ((packetbuffer[1] == 'A') && (replyidx == PACKET_ACC_LEN))
      break;
    if ((packetbuffer[1] == 'G') && (replyidx == PACKET_GYRO_LEN))
      break;
    if ((packetbuffer[1] == 'M') && (replyidx == PACKET_MAG_LEN))
      break;
    if ((packetbuffer[1] == 'Q') && (replyidx == PACKET_QUAT_LEN))
      break;
    if ((packetbuffer[1] == 'B') && (replyidx == PACKET_BUTTON_LEN))
      break;
    if ((packetbuffer[1] == 'C') && (replyidx == PACKET_COLOR_LEN))
      break;
    if ((packetbuffer[1] == 'L') && (replyidx == PACKET_LOCATION_LEN))
      break;

    while (ble_uart->available()) {
      char c =  ble_uart->read();
//      if (c == '!') {
//        replyidx = 0;
//      }
      packetbuffer[replyidx] = c;
      replyidx++;
      timeout = origtimeout;
    }

    if (timeout == 0) break;
    delay(1);
  }

  return replyidx;
}

bool toggle_run = false;

void loop()
{
  uint8_t len = readPacket(&bleuart, 100); //500
  if (len != 0) {
    toggle_run = !toggle_run;
  }

  if (toggle_run) {
//    int dist = sensor.readRangeSingleMillimeters();

//    digitalWrite(trigPin, LOW); // Turns trigPin OFF
//    delayMicroseconds(2);
//    digitalWrite(trigPin, HIGH); // Turns trigPin ON
//    delayMicroseconds(20);
//    digitalWrite(trigPin, LOW);
//    duration = pulseIn(echoPin, HIGH); // Reads echoPin
//    //  distance = duration * 0.034 / 2.0;
//    distance = (int) ((float)duration / 58);
//    Serial.print("Distance: ");
//    Serial.println(distance);

    if (Serial1.available()) {
      delay(5);
      byte c = Serial1.read();
      if (c == 0xFF){
        message[index_n++] = c;
        while (Serial1.available() && index_n <4){
          c = Serial1.read();
          message[index_n++] = c;
        }
        uint8_t check_sum = (message[0] + message[1] + message[2]) & 0x00ff;
        if (check_sum == message[3]) {
          dist = message[1];
          dist = dist << 8;
          dist = dist | message[2];

          char buf[4];
          int n = sprintf(buf, "%d\n", (dist));
          bleuart.write( buf, n );

//          Serial.println(buf);

//          display.clearDisplay();
//          display.setCursor(0,0);
//          display.print(dist);
//          display.print("cm");
//          display.display();

        }

        index_n = 0;
      }

      Serial1.flush();
    }

    delay(50);
  }


//  Serial.print(sensor.readRangeSingleMillimeters());
//  if (sensor.timeoutOccurred()) { Serial.print(" TIMEOUT"); }

//  Serial.println();

//  delay(1);
}

// callback invoked when central connects
void connect_callback(uint16_t conn_handle)
{
  // Get the reference to current connection
  BLEConnection* connection = Bluefruit.Connection(conn_handle);

  char central_name[32] = { 0 };
  connection->getPeerName(central_name, sizeof(central_name));

  Serial.print("Connected to ");
  Serial.println(central_name);
}


/**
 * Callback invoked when a connection is dropped
 * @param conn_handle connection where this event happens
 * @param reason is a BLE_HCI_STATUS_CODE which can be found in ble_hci.h
 */
void disconnect_callback(uint16_t conn_handle, uint8_t reason)
{
  (void) conn_handle;
  (void) reason;

  Serial.println();
  Serial.println("Disconnected");
}
