#include <SPI.h>
#include <PubSubClient.h>
#include <Ethernet.h>
#include <stdlib.h>
#include <string.h>

/*
 * LightSensorMqttDemo
 *
 * A simple m2m.io platform demo for Arduino.
 */

//#define MQTT_SERVER "192, 168, 2, 50"
byte MQTT_SERVER[] = {192, 168, 2, 50 };

// MAC Address of Arduino Ethernet Sheild (on sticker on shield)
//byte MAC_ADDRESS[] = {0x00, 0x50, 0xC2, 0x97, 0x20, 0xD8}; /* UX8 */
byte MAC_ADDRESS[] = {0x00, 0x50, 0xC2, 0x97, 0x20, 0xD9 }; /* Nym */
//byte MAC_ADDRESS[] = {0x90, 0xA2, 0xDA, 0x0E, 0xF3, 0x10 }; /* UXt */
//byte MAC_ADDRESS[] = {0x90, 0xA2, 0xDA, 0x10, 0x8D, 0x8A }; /*  */
PubSubClient client;
EthernetClient ethClient;

// topic
char electTopic[] = "election";
char dataTopic[] = "data";
char syncTopic[] = "sync";
char setTopic[] = "set";
String selectTopic = String(electTopic);
String sDataTopic = String(dataTopic);

// Pin 3 is the LED output pin
int ledPin0 = 3;
int ledPin1 = 4;
int ledPin2 = 5;

// Analog 0 is the input pin : センサが繋がっているピン
int lightPinIn = 0;

// defines and variable for sensor/control mode
#define MODE_OFF    0  // not sensing light, LED off
#define MODE_ON     1  // not sensing light, LED on
#define MODE_SENSE  2  // sensing light, LED controlled by software. this is leader mode.
int senseMode = 4;

unsigned long time;

char message_buff[100];

// role : follower | leader
#define FOLLOWER  0
#define LEADER    1

//first role is follower
int role = FOLLOWER;
//int role = LEADER;

//if leader election finished.
bool isElected = false;

//プレイ端末のID
//TODO:IPアドレスとかどういった者にするべきか検討
int deviceID;

//RTT TODO:rttを自分で求める
int rtt = 30;
//int rtt = 20;
//int rtt = 10;

class Player{
  int playerID;
  int value;
public:
//  Player(int _id):playerID(_id){}
  void setValue(int _id, int _val);
  int getValue(){ return value; }
};
//Player::Player(int _id):playerID(_id){}
//void Player::setValue(int _id, int _val):playerID(_id), value(_val){}
void Player::setValue(int _id, int _val){
  playerID = _id;
  value = _val;  
}

Player pl[3];

//プロトタイプ宣言
void callback(char* topic, byte* payload, unsigned int length);

void setup()
{
  // initialize the digital pin as an output.
  pinMode(ledPin0, OUTPUT);
  pinMode(ledPin1, OUTPUT);
  pinMode(ledPin2, OUTPUT);

  //initialize device ID
  //TODO:deviceIDが0ならhost, それ以外はhostからもらうように
  //hostにするかどうかはRTTをブルーアルゴリズムのように比較する
//  deviceID = 0;
//  deviceID = 1;
  deviceID = 2;
  
  // init serial link for debugging
  Serial.begin(9600);
  
  if (Ethernet.begin(MAC_ADDRESS) == 0)
  {
      Serial.println("Failed to configure Ethernet using DHCP");
      return;
  }

  client = PubSubClient(MQTT_SERVER, 1883, callback, ethClient);

  //プレイヤーの設定
  //TODO:動的にプレイヤーidを生成して追加できるように
  for(int i=0; i<3; i++) pl[i].setValue(i,0);
}

void loop()
{  
  if (!client.connected())
  {
      //
      client.connect("shinji");
      client.publish(dataTopic, "I'm alive!");
      client.subscribe(dataTopic, 1);
      if(role == LEADER)
        client.subscribe(syncTopic, 1);
      client.subscribe(setTopic, 1);
      // leader election topic
//      client.publish(electTopic, "1");
//      client.subscribe(electTopic, 1);
  }
  
//  if(role == LEADER){
//    senseMode = 2;
//  }

  //自身の値を取得
  //read from light sensor (photocell)
  int lightRead = analogRead(lightPinIn);
  
  //自分の値をホストへ送信
  //publish light reading every 1 seconds
  if (millis() > (time + 1000)) {
    time = millis();
    String pubString = "";
    pubString += String(deviceID);
    pubString += " ";
    pubString += String(lightRead);
    pubString.toCharArray(message_buff, pubString.length()+1);
    client.publish(syncTopic, message_buff, true );
  }
  
  //
  if (pl[0].getValue() > 500) {
    digitalWrite(ledPin0, LOW);
  } else {
    digitalWrite(ledPin0, HIGH);
  }
  if (pl[1].getValue() > 500) {
    digitalWrite(ledPin1, LOW);
  } else {
    digitalWrite(ledPin1, HIGH);
  }
  if (pl[2].getValue() > 500) {
    digitalWrite(ledPin2, LOW);
  } else {
    digitalWrite(ledPin2, HIGH);
  }
  
  // MQTT client loop processing
  client.loop();
}

void sync(int playerID, int value){
//  publish(syncTopic, )
}

//playerIDごとのセンサ値をセットする
//TODO:汎用性まったくないから修正
void setValue(char* values, unsigned int len){
  int _id, _val, i=0;
  char* vals[2];
  char *tok;

  Serial.println(values);

  tok = strtok( values, " " );
  while( tok != NULL ){
    vals[i] = (char *)malloc(sizeof(char) * (strlen(tok)+1));
    strcpy(vals[i], tok);
    i++;
    tok = strtok( NULL, " " );  /* 2回目以降 */
  }

  _id = atoi(vals[0]);
  _val = atoi(vals[1]);

  for(int i=0; i<2; i++){
    free(vals[i]);
  }
  
  Serial.println(_id);
  Serial.println(_val);
  
  pl[_id].setValue(_id, _val);

  return;
}

// handles message arrived on subscribed topic(s)
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.println("Message arrived:  topic: " + String(topic));

  int i;
  char recvMessage[100];
  // create character buffer with ending null terminator (string)
  for(i=0; i<length; i++) {
    recvMessage[i] = payload[i];
  }
  recvMessage[i] = '\0';

  //TODO:メッセージの分解をホストとゲストどちらでやるか
  if(strcmp(topic, syncTopic) == 0){
    client.publish(setTopic, recvMessage, true );
    
  } else if(strcmp(topic, setTopic) == 0) {
    setValue(recvMessage, length);
  }
}

