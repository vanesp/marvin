/*
MarvinBase

Basic controls of IoT Academy Marvin LoRa Development board.

This version supports:
	- Sending LoRa uplink messages using ABP 
	- Blink three times when sending data
  - Power control to RN2483 module

Instructions:
	- Get the latest version of the Arduino software
	- In Arduino IDE select Arduino Leonardo and com port of your device
	- Please adjust ABP adresses and key below to match yours
	- The loop() is where the actual stuff happens. Adjust input of send_lora_data() in void loop() to send your own data.
*/

// Demo code for Grove - Temperature Sensor V1.1/1.2
// Loovee @ 2015-8-26

#include <math.h>

// Which device am I -- only allow 1
// #define marvin_kpn
#define marvin2

// Some variables for the Grove temperature sensor v1.2 : http://wiki.seeed.cc/Grove-Temperature_Sensor_V1.2/
const int B=4275;                 // B value of the thermistor
const int R0 = 100000;            // R0 = 100k
const int pinTempSensor = A3;     // Grove - Temperature Sensor connect to A3 - This is the port closest to the USB port of Marvin

// Port to assign the type of lora data (any port can be used between 1 and 223)
int     set_port  = 1;

// Some standard ports that depend on the layout of the Marvin
long    defaultBaudRate = 57600;
int     reset_port = 5;
int     RN2483_power_port = 6; //Note that an earlier version of the Marvin doesn't support seperate power to RN2483
int     led_port = 13;
int     loopcount = 1;

// PvE: HWEUI: 0004A30B001C345E

//*** Set parameters here BEGIN ---->
// KPN values
// String  set_nwkskey = "cee427a11502fd86494bcf8b08767d41"; // Put your 32 hex char here
// String  set_appskey = "d050c7087b2ff676d085af35f8fa043f"; // Put your 32 hex char here
// String  set_devaddr = "142031E4"; // Put your 8 hex char here
// TTN values
#ifdef marvin_kpn
String  set_nwkskey = "BD3B65E02AC1B9E67F5DD374D5C93000";
String  set_appskey = "D1ACBA13D2965530D6B7A3707F7AEA9B";
String  set_devaddr = "260114E8";
#endif

#ifdef marvin2
String  set_devaddr = "260119F1";
String  set_nwkskey = "AC14F20B3BD234687F483D8A9986F9ED";
String  set_appskey = "6A48C2D2B3203DCCDBE0A9D0FC768E0C";
#endif
//*** <---- END Set parameters here

// timer related stuff
unsigned long previousMillis = 0;        // will store last time data was sent
unsigned long interval = 60000;           // interval at which to send the data = one minute
unsigned long currentInterval = 0;

String rx_data_rec = "";

/*
 * Setup() function is called when board is started. Marvin uses a serial connection to talk to your pc and a serial
 * connection to talk to the RN2483, these are both initialized in seperate functions. Also some Arduino port are 
 * initialized and a LED is called to blink when everything is done. 
 */
void setup() {
  
  delay(1000*5);
  Serial.begin(defaultBaudRate);
  Serial1.begin(defaultBaudRate);
  InitializeSerials(defaultBaudRate);
  initializeRN2483(RN2483_power_port, reset_port);
  pinMode(led_port, OUTPUT); // Initialize LED port  
  blinky();
  previousMillis = 0;
}

void loop() {

  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= currentInterval) {
      previousMillis = currentMillis;
      currentInterval = interval;

      // start with transmission every minute, then make it 5 minutes
       loopcount++;
       if (loopcount == 5) {
        interval = interval * loopcount;  
      }
      
      int a = analogRead(pinTempSensor );
      
      float R = 1023.0/((float)a)-1.0;
      R = 100000.0*R;
      float temperature=1.0/(log(R/100000.0)/B+1/298.15)-273.15;//convert to temperature via datasheet ;

      Serial.print("temperature = ");
      Serial.println(temperature);

      // Add 100 degrees to make sure it is a positive value... to be subtracted in decoder
      // Encode float as int (20.98 becomes 2098)
      temperature = temperature + 100;
      int16_t celciusInt = round(temperature * 100);

      // Encode int as bytes and transmit as HEX
      String str = String(highByte(celciusInt), HEX) + String(lowByte(celciusInt), HEX);
      send_LoRa_data(set_port, str);
 
      // send_LoRa_data(set_port, String(TempValue));
      // send_LoRa_data(set_port, payload);

      // Flash the led
      blinky();
  }

  read_data_from_LoRa_Mod();

  if (rx_data_rec == "mac_rx 1 FF") {
      Serial.println("Received downlink request for response");
      // acknowledge - just for demo purpose
      send_LoRa_data(set_port, "0000");  
      rx_data_rec ="";
  } else if (rx_data_rec > "") {
      Serial.print("Received: ");
      Serial.println(rx_data_rec);
      rx_data_rec = "";     
  }
    
}

void InitializeSerials(int baudrate)
{
  delay(1000);
  print_to_console("Serial ports initialised");
}

void initializeRN2483(int pwr_port, int rst_port)
{
  //Enable power to the RN2483
  pinMode(pwr_port, OUTPUT);
  digitalWrite(pwr_port, HIGH);
  print_to_console("RN2483 Powered up");
  delay(1000);
  
  //Disable reset pin
  pinMode(rst_port, OUTPUT);
  digitalWrite(rst_port, HIGH);
  delay (1000);
 

  //Configure LoRa module
  send_LoRa_Command("sys reset");
  read_data_from_LoRa_Mod();

  send_LoRa_Command("radio set crc off");
  delay(1000);
  read_data_from_LoRa_Mod();

  send_LoRa_Command("mac set nwkskey " + set_nwkskey);
  read_data_from_LoRa_Mod();

  send_LoRa_Command("mac set appskey " + set_appskey);
  read_data_from_LoRa_Mod();

  send_LoRa_Command("mac set devaddr " + set_devaddr);
  read_data_from_LoRa_Mod();

  //For this commands some extra delay is needed.
  send_LoRa_Command("mac set adr on");
  delay(1000);
  read_data_from_LoRa_Mod();

  send_LoRa_Command("mac save");
  delay(1000);
  read_data_from_LoRa_Mod();

  send_LoRa_Command("mac join abp");
  delay(1000);
  read_data_from_LoRa_Mod();

}

void print_to_console(String message)
{
  Serial.println(message);
}

void read_data_from_LoRa_Mod()
{
  if (Serial1.available()) {
    String inByte = Serial1.readString();
    rx_data_rec = inByte;
    Serial.println(inByte);
  }

}

void send_LoRa_Command(String cmd)
{
  print_to_console("Now sending: " + cmd);
  Serial1.println(cmd);
  delay(500);
}

void send_LoRa_data(int tx_port, String rawdata)
{
  send_LoRa_Command("mac tx uncnf " + String(tx_port) + String(" ") + rawdata);
}


void blinky()
{
  digitalWrite(led_port, HIGH);   // turn the LED on (HIGH is the voltage level)
  delay(500);                     // wait for a second
  digitalWrite(led_port, LOW);    // turn the LED off by making the voltage LOW
  delay(500);                     // wait for a second
  digitalWrite(led_port, HIGH);   // turn the LED on (HIGH is the voltage level)
  delay(500);                     // wait for a second
  digitalWrite(led_port, LOW);    // turn the LED off by making the voltage LOW
  delay(500);                     // wait for a second

}
