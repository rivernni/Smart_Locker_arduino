// 지문인식
#include <Adafruit_Fingerprint.h>
#define mySerial Serial1
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);


// LCD 1602
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(0x27, 16, 2); //1602


// 핀번호 (부저, 릴레이(솔레노이드 도어락), 마그네틱 도어)
#define buzzer 11
#define relay 10
#define door 12


// 키패드
#include <Keypad.h>

const byte ROWS = 4;
const byte COLS = 4;

//금고의 비밀번호는 6자리이다!
char pw[] = {'1', '2', '3', '4', '5', '6'};
String user_pw = "";

char hexaKeys[ROWS][COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};
byte rowPins[ROWS] = {9, 8, 7, 6};
byte colPins[COLS] = {5, 4, 3, 2};

Keypad customKeypad = Keypad( makeKeymap(hexaKeys), rowPins, colPins, ROWS, COLS);

byte err_count[] = {0, 0}; // err_count[0] = keypad, err_count[1] = finger_scan
unsigned long err_t[] = {0, 0}; //비밀번호 에러카운트 확인용 시계
unsigned long auto_t = 0; //자동잠금 확인용 시계
unsigned long r0 = 0;  // 매 1초마다 실행
unsigned long r1 = 0;  // 매 0.5초마다 실행
unsigned long r2 = 0;  // 매 0.2초마다 실행

int buzzer_tone[] = {2093, 1319, 1568, 2093};

boolean key_check = false;
String dates="";
char   datec[40];
char   datec_do[40];


//----------------------------------------------------------------------------------------


int error_max_num = 3;
int max_time = 60;
int door_state = LOW;       //LOW=문닫힘
int door_state_old = LOW;   //LOW=문닫힘
int relay_state = HIGH; //HIGH=도어락잠금

String buff;


//----------------------------------------------------------------------------------------


//WIFI 통신
#include "ESP8266.h"
//핫스팟
//#define SSID        "20201364"
//#define PASSWORD    "581309011013*"

// 2.4G
#define SSID        "U+Net6D0C"
#define PASSWORD    "F46D5#585G"

//호남대학교
//#define SSID        "Multimedia1"
//#define PASSWORD    "admin1234!"

ESP8266 wifi(Serial2, 9600);

//----------------------------------------------------------------------------------------


// DS1300 Clock -----------
//#define DS1300_CLK      22
//#define DS1300_DAT      23
//#define DS1300_RST      24
#include<DS1302.h>
DS1302 myrtc(24,23,22);





//========================================================================================
//========================================================================================
void setup() {

  //핀 설정
  pinMode(relay, OUTPUT);       //잠금장치 설정
  digitalWrite(relay, HIGH);    

  pinMode(door, INPUT_PULLUP);  //문이 닫히면 LOW 열리면 HIGH
  Serial.begin(9600); //PC-아두이노간 통신

//----------------------------------------------------------------------------------------
 
  // DS1302 초기 시간 설정
  myrtc.halt(false);
  myrtc.writeProtect(false);
  myrtc.setDOW(MONDAY);
  myrtc.setTime(5, 50, 20); // 시간, 분, 초
  myrtc.setDate(9, 6, 2022); // 일, 월, 년도
  delay(10); 

//---------------------------------------------------------------------------------------- 
  
  //Wifi Setup
  // EspSerial.begin(ESP8266_BAUD);
  if (wifi.setOprToStationSoftAP()) {
    Serial.print("to station + softap ok\r\n");
  } else {
    Serial.print("to station + softap err\r\n");
  }

  if (wifi.joinAP(SSID, PASSWORD)) {
    Serial.print("Join AP success\r\n");
    Serial.print("IP: ");
    Serial.println(wifi.getLocalIP().c_str());
  } else {
    Serial.print("Join AP failure\r\n");
  }

  if (wifi.enableMUX()) {
    Serial.print("multiple ok\r\n");
  } else {
    Serial.print("multiple err\r\n");
  }

  if (wifi.startTCPServer(80)) {
    Serial.print("start tcp server ok\r\n");
  } else {
    Serial.print("start tcp server err\r\n");
  }

  if (wifi.setTCPServerTimeout(10)) {
    Serial.print("set tcp server timout 10 seconds\r\n");
  } else {
    Serial.print("set tcp server timout err\r\n");
  }

  Serial.print("Wifi setup end\r\n");
  Serial.println(F("The server is starting."));  // 시리얼모니터로 확인용 삭제가능

//---------------------------------------------------------------------------------------- 

  // 지문 통신
  finger.begin(57600); //아두이노-FPM10A 통신라인

  // lcd 설정
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0); //0~15/0~1

  //DS1300 설정 시간 시리얼 모니터에 출력
  Serial.println("DS1302RTC Test");
  Serial.println("---------------");
  Serial.println(myrtc.getDOWStr());
  Serial.println(myrtc.getDateStr());   //날짜
  Serial.println(myrtc.getTimeStr());  // 시간
  
}
//----------------------------------------------------------------------------------------
// HTTP WEB화면출력
//void WEB_prt() {  }
//----------------------------------------------------------------------------------------







//========================================================================================
//========================================================================================
void loop() {

//----------------------------------- HTTP 웹서버용 ----------------------------------------
  uint8_t buffer[1024] = {0};
  uint8_t postStr[9] = {0};
  uint8_t mux_id;
  uint32_t len = wifi.recv(&mux_id, buffer, sizeof(buffer), 100);
  char sw[48];
  uint8_t timeBuffer[48] = {0};
  
  if (len > 0) {        
    //Serial.println("## 02 ##"); // 시리얼 모니터 테스트

    strcpy(postStr, buffer + len - 8);
    if(strcmp(postStr,"pinD13=0") == 0) {
      digitalWrite(relay, LOW);  //도어락 OPEN
      key_check = true;
      auto_t = millis();
      Serial.println("웹으로 도어 릴레이 open");
    }
    else if(strcmp(postStr,"pinD13=1") == 0) {
      digitalWrite(relay, HIGH);  //도어락 close
      key_check = false;
      Serial.println("웹으로 도어 릴레이 close");
    }
 

    //Serial.println("## 03 ##"); // http 서버에 출력할 내용
    char *title1 = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n<html>";
    char *title2 = "<head><title>Door Relay Control</title>\r\n";
    char *title3 = "<meta http-equiv=refresh content=5><meta http-equiv = Content-Type  content= text/html; charset=UTF-8 />";
    char *title4 = "\r\n</head>\r\n<body>\r\n";
        
    char *header = "<h1>Door Relay Control</h1>\r\n";                  
    char *button0 = "<form action='/' method='POST'><p><input type='hidden' name='pinD13' value='0'><input type='submit' value='Door Relay Open'/></form>\r\n";
    char *button1 = "<form action='/' method='POST'><p><input type='hidden' name='pinD13' value='1'><input type='submit' value='Door Relay Close'/></form>\r\n";
    char *tail = "</body></html>\r\n";  

    dates="Date : "+String(myrtc.getDateStr())+" -- "+String(myrtc.getTimeStr());
    strcpy(datec,dates.c_str());
    
    wifi.send(mux_id, title1, strlen(title1));
    wifi.send(mux_id, title2, strlen(title2));
    wifi.send(mux_id, title3, strlen(title3));
    wifi.send(mux_id, title4, strlen(title4));
    wifi.send(mux_id, header, strlen(header));
    wifi.send(mux_id, datec, strlen(datec));
    
    String s0="";
    if (digitalRead(door)==1)  s0="열림"; else s0="닫힘";
    String s1 = "<h2>사물함 문 상태: "+s0+" </h2>\r\n";
    strcpy(sw,s1.c_str());

    wifi.send(mux_id, sw, strlen(sw));
    Serial.println("## 01 ##"); // 시리얼 모니터 확인
    wifi.send(mux_id, datec_do, strlen(datec_do));  //마지막으로 도어가 열린시간
    Serial.println("## 02 ##"); // 시리얼 모니터 확인
    wifi.send(mux_id, button0, strlen(button0));
    wifi.send(mux_id, button1, strlen(button1));
    wifi.send(mux_id, tail, strlen(tail));
    
    Serial.println("## 03 ##"); // 시리얼 모니터 확인
  }


//----------------------------------- HTTP 웹서버용 끝 -------------------------------------









  // 에러 카운트가 3 이하라면 lcd 모니터에 choice() 내용 출력
  if (err_count[0] < 3 && door_state == 0 && relay_state == 1) {
    Choice();
  }


  //키패드 or 지문으로 락해제 되었다면
  if (key_check == true) {
    door_state = digitalRead(door);
    relay_state = digitalRead(relay);
    
    // Serial.println("## 06 ##"); // 시리얼 모니터 확인
    if (relay_state == LOW && door_state == LOW) {
      //open_door_lcd();
      //잠금이 해제가 되어있는 상태
      //문이 닫혀있는 상태
      //시간이 10초이상 경과하면 문을 다시 잠그겠다
      if (millis() - auto_t > 10000) {
        Auto_close();
        Serial.println("10초 경과 후 자동잠금");
        lcd.clear();
      }
    }
    // 잠금도 해제되어있고 문도 열려있다면 도어 자동 잠금 
    // => 솔레노이드 도어락에 계속해서 신호를 주는 문제 해결
    if (relay_state == LOW && door_state == HIGH) {
      //Auto_close();
      digitalWrite(relay, HIGH);
      Serial.println("door open, 자동잠금");
    }

    if(relay_state == HIGH && door_state == HIGH){
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.println("door open       ");
    }
  }

  //--------------------------------------------------------------------------------------
  // 마지막 실행으로부터 100분이 경과하면 에러카운트를 초기화
  if (err_count[0] > 0 && (millis() - err_t[0] > max_time * 100000)) {
    Serial.println("키패드 에러 발생 후 10분 경과, 에러 카운트 초기화");
    err_count[0] = 0;
  }






  //--------------------------------------------------------------------------------------
  if (r0 < millis()) {
    //Serial.println("매 1초마다 실행");
    //Serial.print("err_count[0]=");
    //Serial.println(err_count[0]);

    door_state = digitalRead(door);
    relay_state = digitalRead(relay);
    
 
    Serial.print("door="); Serial.print(door_state);
    Serial.print(" relay="); Serial.println(relay_state);

    if (relay_state == LOW && door_state == LOW) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("open door");
      lcd.setCursor(2, 1);
      lcd.print("  time sec: "); 
      int t = 10 - (millis()- auto_t) / 1000;
      
      if (t < 10){
        lcd.setCursor(13,1);
        lcd.print(t);
      }
      else{
        lcd.setCursor(12, 1);
        lcd.print(t);
      }
    }
    
    if (err_count[0] >= error_max_num) {
      Serial.println("#########");
      lcd.clear();
      lcd.setCursor(0, 0); //0~15/0~1
      lcd.print("ERROR COUNT!");

      int n = max_time - ( millis() - err_t[0]) / 1000;

      if (n < 10) {
        lcd.setCursor(1, 1);
        lcd.print(n);
      }
      else {
        lcd.setCursor(0, 1); //0~15/0~1
        lcd.print(n);
      }

      lcd.setCursor(2, 1);
      lcd.print("seconds stop!!");

      Serial.print("에러카운트");
      Serial.print(err_count[0]);
      Serial.println("회 누적!");
      Serial.println("60초간 휴식!");
      if(n < 1){
        err_count[0] = 0;
      }
    }
    r0 = millis() + 1000; //1초마다 실행
  }
  
  //--------------------------------------------------------------------------------------
  if (r1 < millis()) {
    //Serial.println("매 0.5초마다 도어열림확인 ");
    
    door_state = digitalRead(door);
    if (door_state==0 && door_state_old==1) door_state_old=0; //도어문이 닫히면
      
    if (door_state==1 && door_state_old==0) { //도어문이 열리면
      door_state_old=1;
      
      dates="Last Door Open : "+String(myrtc.getDateStr())+" -- "+String(myrtc.getTimeStr());
      strcpy(datec_do,dates.c_str());
      Serial.println("## 문 열림 ## ");
      Serial.println(dates);
    }
    r1 = millis() + 500; //매 0.5초마다 도어열림확인
  
  }
  //--------------------------------------------------------------------------------------


//Serial.println("## 08 ##"); // 시리얼 모니터 테스트 


  // 키패드 입력 처리
  char checkKey = customKeypad.getKey();
  // 키패드 입력시 소리 출력
  if (checkKey) {
    tone(buzzer, 2093);
    delay(100);
    noTone(buzzer);
  }
  if(door_state == 0){
    // 키패드에 *이 눌리면 키패드를 사용하여 잠금해제 시도
    if (checkKey == '*' && err_count[0] < 3) {

      lcd.clear();
      lcd.setCursor(0, 0); //0~15/0~1
      lcd.print("INPUT PASSWORD!");
      lcd.setCursor(0, 1); //0~ 15/0~1
  
      open_keypad();
    }

    // 키패드에 D이 눌리면 키패드를 사용하여 잠금해제 시도
    else if (checkKey == 'D') {
      lcd.clear();
      lcd.setCursor(0, 0); //0~15/0~1
      lcd.print("Finger scan");
      lcd.setCursor(0, 1); //0~15/0~1
      finger_scan();
    }
  }
}
//========================================================================================
//========================================================================================




//----------------------------------------------------------------------------------------
// 키패드 사용 처리 함수 
void open_keypad() {
  while (1) {
    char customKey = customKeypad.getKey();

    if (customKey) {
      Comm_sound();
    }

    if (customKey >= '0' && customKey <= '9') {
      if (user_pw.length() < 6) {
        user_pw += customKey;
      }
      String star = "";
      for (int i = 0; i < user_pw.length(); i++) {
        star += '*';
      }

      lcd.clear();
      lcd.setCursor(0, 0); //0~15/0~1
      lcd.print("INPUT PASSWORD!");
      lcd.setCursor(0, 1); //0~15/0~1
      lcd.print(star);

      Serial.print(user_pw);
      Serial.print("/");
      Serial.println(user_pw.length());
    }
    else if ( customKey == '#') {
      if (user_pw.length() == 6) {
        if (pw[0] == user_pw[0] && pw[1] == user_pw[1] && pw[2] == user_pw[2] && pw[3] == user_pw[3] && pw[4] == user_pw[4] && pw[5] == user_pw[5]) {
          Success_PW("키패드");
          key_check = true;
          break;
        }
        else {
          Fail_PW("키패드");
          lcd.clear();
          key_check = false;
          break;
        }
      }
    }
    // Backspace
    else if (customKey == 'B') {
      if (user_pw.length() > 0) {
        user_pw.remove(user_pw.length() - 1);
      }
      String star = "";
      for (int i = 0; i < user_pw.length(); i++) {
        //현재 입력되어있는 비밀번호 갯수만큼 루프가 회전한다!
        star += '*';
      }
      lcd.clear();
      lcd.setCursor(0, 0); //0~15/0~1
      lcd.print("INPUT PASSWORD!");
      lcd.setCursor(0, 1); //0~15/0~1
      lcd.print(star);

      Serial.print(user_pw);
      Serial.print("/");
      Serial.println(user_pw.length());
    }
    // Del > 키패드 사용 취소
    else if (customKey == 'D') {
      lcd.clear();
      user_pw = "";
      break;
    }

  }
}

//----------------------------------------------------------------------------------------
void finger_scan() {
  while (1) {

    char customKey = customKeypad.getKey();

    if (customKey) {
      Comm_sound();
    }

    bool door_state = digitalRead(door);
    bool relay_state = digitalRead(relay);

    int id = getFingerprintIDez();

    if (id != -1) {
      Serial.print("인식된 지문의 ID = ");
      Serial.println(id);

      if (id == 1) {
        //금고의 권한이 있는 지문
        Success_PW("지문");
        key_check = true;
        break;
      }
    }

    else if (customKey == 'D') {
      lcd.clear();
      key_check = false;
      break;
    }
  }
}

// 지문 확인 코드
int getFingerprintIDez() {
  uint8_t p = finger.getImage();
  if (p != FINGERPRINT_OK)  return -1;

  p = finger.image2Tz();
  if (p != FINGERPRINT_OK)  return -1;

  p = finger.fingerFastSearch();
  if (p != FINGERPRINT_OK)  return -1;

  // found a match!
  //Serial.print("Found ID #"); Serial.print(finger.fingerID);
  //Serial.print(" with confidence of "); Serial.println(finger.confidence);
  return finger.fingerID;
}

//----------------------------------------------------------------------------------------






//--------------------------LCD 출력 처리, 소리, 키패드 암호 확인 함수---------------------------
// 선택 창
void Choice() {
  lcd.setCursor(0, 0); //0~15/0~1
  lcd.print("*:password input");
  lcd.setCursor(0, 1);
  lcd.print("D:finger scanner");
}


// 자동 잠금 후 LCD 출력
void Auto_close() {
  digitalWrite(relay, HIGH);
  delay(500);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Close door");
  delay(1000);
  key_check = false;
}


// 일반 소리 부저
void Comm_sound() {
  tone(buzzer, 2093);
  delay(100);
  noTone(buzzer);
}


// 암호 일치
void Success_PW(String name) {
  Serial.print(name);
  Serial.println("인증 성공");

  auto_t = millis();

  lcd.clear();
  lcd.setCursor(0, 0); //0~15/0~1
  lcd.print(" Password match ");
  lcd.setCursor(0, 1); //0~15/0~1
  lcd.print("     UNLOCK     ");

  digitalWrite(relay, LOW);

  tone(buzzer, 2093); //도
  delay(200);
  tone(buzzer, 1319); //미
  delay(200);
  tone(buzzer, 1568); //솔
  delay(200);
  tone(buzzer, 2093); //도
  delay(200);
  noTone(buzzer);

  // 암호 초기화
  user_pw = "";
  err_count[0] = 0;
}


// 암호 인증 실패
void Fail_PW(String name) {

  Serial.print(name);
  Serial.println("인증 실패");

  err_t[0] = millis();
  err_count[0] += 1;

  user_pw = "";


  lcd.clear();
  lcd.setCursor(1, 0); //0~15/0~1
  lcd.print("PW do not match");
  lcd.setCursor(0, 1); //0~15/0~1
  lcd.print("UNLOCK FAIL");
  tone(buzzer, 2093); //도
  delay(200);
  tone(buzzer, 2093); //도
  delay(200);
  noTone(buzzer);
  delay(1000);
  user_pw = "";

  return;
}

void open_door_lcd(){
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.println("   open  door   ");
  lcd.setCursor(0, 1);
  lcd.println("  time : 10sec  ");
}

void close_door_lcd(){
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.println("   close door   ");
}
