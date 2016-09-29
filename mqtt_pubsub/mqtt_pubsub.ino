#include <SPI.h>
#include <PubSubClient.h>
#include <Ethernet.h>

/*
 * LightSensorMqttDemo
 *
 * A simple m2m.io platform demo for Arduino.
 */

//#define MQTT_SERVER "q.m2m.io"
//#define MQTT_SERVER "192, 168, 2, 50"
byte MQTT_SERVER[] = {192, 168, 2, 50 };

// MAC Address of Arduino Ethernet Sheild (on sticker on shield)
//byte MAC_ADDRESS[] = {0x00, 0x50, 0xC2, 0x97, 0x20, 0xD8}; /* UX8 */
//byte MAC_ADDRESS[] = {0x00, 0x50, 0xC2, 0x97, 0x20, 0xD9 }; /* Nym */
byte MAC_ADDRESS[] = {0x90, 0xA2, 0xDA, 0x0E, 0xF3, 0x10 }; /* UXt */
//byte MAC_ADDRESS[] = {0x90, 0xA2, 0xDA, 0x10, 0x8D, 0x8A }; /*  */
PubSubClient client;
EthernetClient ethClient;

// topic
char electionTopic[] = "election";
char dataTopic[] = "topic";
String sElectionTopic = String(electionTopic);
String sDataTopic = String(dataTopic);

// Pin 3 is the LED output pin
int ledPin = 3;
// Analog 0 is the input pin
int lightPinIn = 0;

// defines and variable for sensor/control mode
#define MODE_OFF    0  // not sensing light, LED off
#define MODE_ON     1  // not sensing light, LED on
#define MODE_SENSE  2  // sensing light, LED controlled by software. this is leader mode.
int senseMode = 4;

unsigned long time;

char message_buff[100];

// role : follower | candidate | leader
#define FOLLOWER  0
#define CANDIDATE 1
#define LEADER    2

//first role is follower
int role = FOLLOWER;

//each arduino has random timeout.
long timeout;

//if leader election finished.
bool isElected = false;

//candidateのcandidateへの投票数 candidateは１つだけでなければならない
int competeNum = 0;

//leader electionにおけるfollowerのcandidateに対しての投票数
int voteNum = 0;  

void setup()
{
  // initialize the digital pin as an output.
  pinMode(ledPin, OUTPUT);
  
  // init serial link for debugging
  Serial.begin(9600);
  
  if (Ethernet.begin(MAC_ADDRESS) == 0)
  {
      Serial.println("Failed to configure Ethernet using DHCP");
      return;
  }

  client = PubSubClient(MQTT_SERVER, 1883, callback, ethClient);

  // set timeout
  randomSeed(analogRead(0));
  timeout = random(1500, 3000);
}

void loop()
{  
  if (!client.connected())
  {
      // clientID, username, MD5 encoded password
//      client.connect("arduino-mqtt", "john@m2m.io", "00000000000000000000000000000");
      client.connect("shinji");
      client.publish(dataTopic, "I'm alive!");
      client.subscribe(dataTopic, 1);
      // leader election topic
      client.publish(electionTopic, "1");
      client.subscribe(electionTopic, 1);
  }

  //
  if (!isElected && (millis() > (time + timeout))) {
    Serial.println("passed");
    time = millis();
    role = CANDIDATE;
    client.publish(electionTopic, "0");  // 0 is vote message
  }

  //競合した場合（candidateが２つ以上になった場合）
  if(competeNum > 1){
    role = FOLLOWER;
    competeNum = 0;
  }

  // 過半数の票を得られれば（過半数の数は仮に設定）
  if(voteNum > 1){
    role = LEADER;
    isElected = true;
    client.publish(electionTopic, "2");
  }
  
  if(role == LEADER){
    senseMode = 2;
  }
  
  switch (senseMode) {
    case MODE_OFF:
      // light should be off
      digitalWrite(ledPin, LOW);
      break;
    case MODE_ON:
      // light should be on
      digitalWrite(ledPin, HIGH);
      break;
    case MODE_SENSE:
      // light is adaptive to light sensor
      
      // read from light sensor (photocell)
      int lightRead = analogRead(lightPinIn);

      // if there is light in the room, turn off LED
      // else, if it is "dark", turn it on
      // scale of light in this circit is roughly 0 - 900
      // 500 is a "magic number" for "dark"
      if (lightRead > 500) {
        digitalWrite(ledPin, LOW);
      } else {
        digitalWrite(ledPin, HIGH);
      }
      
      // publish light reading every 5 seconds
      if (millis() > (time + 1000)) {
        time = millis();
        String pubString = "";
        pubString += String(lightRead);
        pubString.toCharArray(message_buff, pubString.length()+1);
        //Serial.println(pubString);
//        client.publish("topic", message_buff, true);
        client.publish(dataTopic, message_buff, true );
      }
       
  }
  
  // MQTT client loop processing
  client.loop();
}

// handles message arrived on subscribed topic(s)
void callback(char* topic, byte* payload, unsigned int length) {

  int i = 0;
  int ledValue, electValue;

  Serial.println("Message arrived:  topic: " + String(topic));
  
  // create character buffer with ending null terminator (string)
  for(i=0; i<length; i++) {
    message_buff[i] = payload[i];
  }
  message_buff[i] = '\0';
  
  String msgString = String(message_buff);
  
  Serial.println("Payload: " + msgString);

  // which topic is message arrived
  String sTopic = String(topic);
  if(sTopic.equals(sElectionTopic)){
    electValue = msgString.toInt();

    if(role == CANDIDATE && electValue == 0){
      competeNum++;
    } else if(role == FOLLOWER && electValue == 0){
      client.publish(electionTopic, "1");
    } else if(role == CANDIDATE && electValue == 1){
      voteNum++;
    } else if(role != LEADER && electValue == 2){
      isElected = true;
      role = FOLLOWER;
    }

  // this topic is subscribed after leader election finish
  } else if (sTopic.equals(sDataTopic) && isElected){
    ledValue = msgString.toInt();

    if(role == FOLLOWER){
      if (ledValue > 500) {
        senseMode = MODE_OFF;
      } else {
        senseMode = MODE_ON;
      }    
    }
  }
}

