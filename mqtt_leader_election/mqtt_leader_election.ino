#include <SPI.h>
#include <PubSubClient.h>
#include <Ethernet.h>
#include <stdlib.h>
#include <string.h>

/*
 * MqttLeaderElectionDemo
 *
 * A simple application for mqtt leader election
 */

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
char ipTopic[] = "ip";
char rttTopic[15];  //各端末のipアドレスとする
char electTopic[] = "election";
char getIDTopic[] = "getID";
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

char message_buff[100];

// role : follower | leader
#define FOLLOWER  0
#define LEADER    1

//first role is follower
int role = FOLLOWER;
//int role = LEADER;

//リーダーを決めるための仮の代表者かどうか
bool isPreLeader;
//if leader election finished.
bool isElected = false;

//プレイ端末のID
//TODO:IPアドレスとかどういった者にするべきか検討
int deviceID=-1;

class Player{
  int playerID;
  int value;
public:
//  Player(int _id):playerID(_id){}
  void setValue(int _id, int _val);
  int getValue(){ return value; }
};
void Player::setValue(int _id, int _val){
  playerID = _id;
  value = _val;  
}

Player pl[10];

/*Leader Election関係*/
unsigned long time;
unsigned long pubTime=0;
unsigned long rttCounter=0, rtt=0, rttAverage=0;
//char ip[15];
int ip;
//char iprtt[100];
byte iprtt[2] = {0}; //{ip, rtt}
byte eachIP[10], eachRTT[10];
//char* eachIP[10], eachRTT[10];
byte joinCount=0;
byte idCount[1]={0};
bool isAppriedID=false;

//プロトタイプ宣言
void callback(char* topic, byte* payload, unsigned int length);

void setup()
{
  // initialize the digital pin as an output.
  pinMode(ledPin0, OUTPUT);
  pinMode(ledPin1, OUTPUT);
  pinMode(ledPin2, OUTPUT);

  // init serial link for debugging
  Serial.begin(9600);
  
  if (Ethernet.begin(MAC_ADDRESS) == 0)
  {
      Serial.println("Failed to configure Ethernet using DHCP");
      return;
  }

  client = PubSubClient(MQTT_SERVER, 1883, callback, ethClient);
  setIPAddress();
  //プレイヤーの設定
  //TODO:動的にプレイヤーidを生成して追加できるように
  for(int i=0; i<3; i++) pl[i].setValue(i,0);
}

void loop()
{  
  if (!client.connected())
  {
      client.connect("shinji");
      client.subscribe(ipTopic, 1);
      client.subscribe(rttTopic, 1);
      client.subscribe(dataTopic, 1);
      if(role == LEADER)
        client.subscribe(syncTopic, 1);
      client.subscribe(setTopic, 1);
      client.subscribe(electTopic, 1);
  }

  //RTTフェーズ
  //TODO:1秒に1回にしているが検討
  if (millis() > (time + 1000)) {
    time = millis();
    pubTime = millis();
    client.publish(rttTopic, "", true);
  }

  if(!isElected){
    //ipアドレスとRTTを各ノードに送る
    client.publish(ipTopic, iprtt, sizeof(iprtt) / sizeof(iprtt[0]));
  } else {
    /*もらってなければ一意のIDをもらう*/
    if(deviceID < 0) {
      if(role == LEADER){
        deviceID = 0;
      } else if(role == FOLLOWER) {
        client.publish(getIDTopic, "apply", true);
      }
    }

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

//void DecideLeader(char* _iprtt, unsigned int _len){
void DecideLeader(byte* _iprtt, unsigned int _len){
  byte tempIP, tempRTT;
  bool isInIP = false;
  byte minIP, minRTT;
  char leaderIP[15];

  for(byte i=0; i<joinCount; i++){
    if(eachIP[i] == _iprtt[0]){
      //すでにIPアドレスが登録されていればRTTの更新
      eachRTT[i] = _iprtt[1];
      isInIP = true;
    }
  }
  if(isInIP){
    //
  } else {
    eachIP[joinCount] = _iprtt[0];
    eachRTT[joinCount++] = _iprtt[1];
  }

  /*２端末未満なら返す*/
  if(joinCount < 1)
    return;
  /*最小ip端末を見つける*/
  minIP = 0;
  for(byte i=1; i<joinCount; i++){
    if(eachIP[minIP] > eachIP[i])
      minIP = i;
  }
  /*自分が最小のipアドレスでなかったら返す*/
  if(iprtt[0] != eachIP[minIP])
    return;
  /*最小RTT端末を見つける*/
  minRTT=0;
  for(byte i=0; i<joinCount; i++){
    if(eachRTT[minRTT] > eachRTT[i])
      minRTT = i;
  }

  /*IPアドレスがeachIP[minRTT]であるものがLeader*/
  sprintf(leaderIP, "%d", eachIP[minRTT]);
  client.publish(electTopic, leaderIP, true);
  
//  char *tok;
//  char _canIP[15], _canRTT[10];
//
//  tok = strtok(_iprtt, " ");
//  while( tok != NULL ){
//    _canIP[joinCount] = (char *)malloc(sizeof(char) * (strlen(tok)+1));
//    strcpy(eachIP[joinCount], tok);
//    tok = strtok( NULL, " " );  /* 2回目以降 */
//    if(tok != NULL)
//      _canRTT[joinCount] = (char *)malloc(sizeof(char) * (strlen(tok)+1));
//    strcpy(eachRTT[joinCount], tok);
//  }
}

// handles message arrived on subscribed topic(s)
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.println("Message arrived:  topic: " + String(topic));

  int i;
  char _recvMessage[100];
  char _rttChar[100];
  char _iprttTemp[100] = {0};
  
  //create character buffer with ending null terminator (string)
  for(i=0; i<length; i++) {
    _recvMessage[i] = payload[i];
  }
  _recvMessage[i] = '\0';

  //Topicの場合分け
  if(strcmp(topic, ipTopic) == 0){
    Serial.println(payload[0]);
    Serial.println(payload[1]);
    DecideLeader(payload, length);
    
  } else if(strcmp(topic, rttTopic) == 0){
    /*rttの今までの平均を求める*/
    rtt += (millis() - pubTime);
    rttCounter++;
    rttAverage = rtt / rttCounter;
    /*iprtt = "ipAdress + " " + rtt" とする*/
    iprtt[1] = rttAverage;
    
//    sprintf(_rttChar, "%d", rttAverage); //数値を文字列に変換
//    strcat(_iprttTemp, ip);
//    strcat(_iprttTemp, " ");
//    strcat(_iprttTemp,_rttChar);
//    strcpy(iprtt, _iprttTemp);

  } else if(strcmp(topic, electTopic) == 0){
    if(iprtt[0] == atoi(_recvMessage)){
      role = LEADER;
      isElected = true;
    } else {
      role = FOLLOWER;
    }

  /*可動物に一意にIDを割り当てる host->guestにIDを渡す*/
  } else if(strcmp(topic, getIDTopic) == 0){
    if(role == LEADER){
      if(strcmp(_recvMessage, "apply")){
        client.publish(getIDTopic, idCount, sizeof(idCount) / sizeof(idCount[0]));
        idCount[0]++;
      }
    } else if(role == FOLLOWER){
      if(strcmp(_recvMessage, "apply"))
        return;  
      deviceID = atoi(_recvMessage);
    }

  //TODO:メッセージの分解をホストとゲストどちらでやるか
  } else if(strcmp(topic, syncTopic) == 0){
    client.publish(setTopic, _recvMessage, true );
    
  } else if(strcmp(topic, setTopic) == 0) {
    setValue(_recvMessage, length);
  }
}

//ipアドレスを文字列としてつなげ、数値変換
void setIPAddress()
{
  char tempip[10];
  char ipChar[15];

//  sprintf(rttTopic, "%d", Ethernet.localIP()[3]);

  //ipアドレスを文字列として連結させる
  for(int i=0; i<4; i++){
    sprintf(tempip, "%d", Ethernet.localIP()[i]);
    strcat(ipChar, tempip);
  }
  strcpy(rttTopic, ipChar);
  ip = atoi(ipChar);
  iprtt[0] = ip;
  Serial.println(ip);
}

