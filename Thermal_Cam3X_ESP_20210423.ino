//2021/4/23 Soft WTDリセット後またSetupに戻る時のためにSPI及びWiFi切断関数を最初に入れた。SendOSCも同様。復旧がより確実になる。
//いままで電波が届かなくなってからしばらくすると再接続が困難だったが、２分ほど切断しても復旧可能になる場合が多くなった
//2021/4/10 LoopにWiFi接続チェックを入れる。ESP8266がさらに安定
//2021/4/8  wificonnect()の変更で止まらなくなったfffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff
//2021/4/5  SendOSC大幅修正。BLUETOOTH、GPSがONのときESP8266が止まりにくくなった。＝妨害電波にかなり強くなり画像が乱れにくくなった
//2021/3/20 Bluetoothに強くなった途中で止まらなくなった
//2021/3/5  LEPTON3.5専用に改善
//2019/11/2   LEPTON3.5の時不具合があったので修正。ESP8266でWIFIの飛距離が伸びなくなったので各所で変更。最終的にLEPTONのリセットコマンドを無効にして前に戻った
//SD使用の場合はコメントを外す。LEPTON3.0、3.5自動判別。Bluetoth GPS ONでも殆ど影響を受けなくなった。
/*SD バージョンは当初5行配信だったが、リセットはいることがありyieldを多く入れる必要がある。4行より少しスピードが落ちるので4行配信に戻した3／31
2018/12/20 High speed version for LEPTON 3.0
This is source(the scketch) for ESP-WROOM-02 with the iPhone using the infrared sensor FLIR LEPTON 3.
i add unique functions, which are not available by genuine nor other parties in the marketed for this sensor and a movie speed(frame rate) close to the limit(9 fps) of LEPTON 3 with the lowest cost.
normally distance between iphone and ESP is 50 m or more if the location is clearly visible between iphone and ESP (5 or less surrounding WIFI stations).
the speed will be dropped In the room with 10 or more WIFI stations (including the arcade where the ceiling on the top).
I recommend that you check the number of WIFI stations before you use.
Normally, even if the image stops, image distribution will start as soon as you move within the reach of the radio waves,
If the image stops for more than 30 seconds, switch off(reset) the ESP side.
If packets are lost, The part where the striped pattern in the horizontal direction appears in the image. So ESP 8266 and iPhone
Are too far between, or interference waves (BlueTooth, WIFI stations, routers, etc.).
-> the iPhone apps, you can compile with my code, which is free or purchase it at apps store. 
-> The board manager should use ESP 8266 2.2.0.
-> When compiling arduino sketch, please set to Flash 80 MHz CPU 160 MHz.
-> As memory usage reaches the upper limit, adding more sources may cause operation instability.
-> It is executable if it is an ESP 8266 board that can use SPI and i2C terminals. however SPI is very sensitive if it is longer pattern or CLK, MOIS and MISO is equal condition
-> We do not suggest to remodel the main body antenna, and we sell boards that made the best use of the theoretical radio field intensity.
Please check the following.
-> At a minimum understanding of SSID, IP address, port number, Arduino source written to ESP 8266
I make this to whom can do it programing.
-> Since it is not software for sale, please refrain from basic questions.
If you can not practice the above We may help you including installed drones, so please check. limited offer since we are two of us.

-> Please set below 1 ~ 4 according to your environment.
** You can use my scketch and code free of charge other than selling, licence is under GPL（GNU General Public License). 
* 2017/8/28,2017/10/19 Takeshi Ono
*/
//#######################################################################
//******１.download below OSC library and copy and paste to the right place*******
//　　　　　https://github.com/sandeepmistry/esp8266-OSC
//========================================================================
//*************** ２.change below for your settings. **************
//========================================================================
#define SSID_X    "ABiPhone"   //iPhone
#define PASS_X    "watanabe3"   //iPhone
#define IP_X      172,20,10,1     //iPhone 固定。修正必要なし
//========================================================================
//***************** ３.if you change port number ****************************
//========================================================================
const unsigned int outPort =    8090;      // 送信は標準で8090 iPhone側は8091
const unsigned int localPort =  8091;      // 受信は標準で8091 iPhone側は8090
//ポート番号変更により複数のiPhoneまたは複数のESP8266の使い分けが可能になります。
//========================================================================
//***************** ４.ESP-WROOM-02 GPIO pin for LED ************************
#define LEDpin 16  //16
//========================================================================

//************** SPI speed setting (no need to change) *******************
//************ below speed setting according our test date not by FLIR ***************
#define CLOCK_TAKE 38500000  //28000000 ~ 39000000 で調整だが38500000 以外手をつけないこと!!　この数値でないと継続性が左右される
//************************************************************************
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <SPI.h>
#include <OSCBundle.h>
#include <OSCMessage.h>
//**************CS信号高速化用*****************
#define PIN_OUT *(volatile uint32_t *)0x60000300
#define PIN_ENABLE *(volatile uint32_t *)0x6000030C
#define PIN_15  *(volatile uint32_t *)0x60000364
//******************************************
//iPhoneテザリングLAN設定用
char ssid[] = SSID_X;          // your iPhone Name SSID (name)
char pass[] = PASS_X;          // your iPhone テザリングパスワード
WiFiUDP Udp;
// 機種IPアドレス設定
const   IPAddress outIp(IP_X); //iphone、iPad テザリング用アドレス。全機種共通
char    ip_ESP[16];
uint8_t lepton_image1[39360]; //イメージ用メモリ39,360バイト(39K)確保
int     touchX = 79, touchY = 1798;
float   sl_256 = 0.0f;
float   aux;
int     LEPTON;
float   value_min = 0, value_max = 0, tempXY = 0;
float   th_l,th_h;
//*************OSCアドレス********************
  OSCMessage msg1("/m");
  OSCMessage msg2("/i");
  //################################# LED    ########################################
void led(){      //完成しているので手をつけないこと!! Delay 9,10はこれ以上直さないこと
  digitalWrite(LEDpin, HIGH);delay(9);digitalWrite(LEDpin, LOW);delay(10);yield();yield();yield();yield();yield();yield();ESP.wdtFeed();//個々にある必要がありdelayは9 10である必要あり
}
//################################# WIFI CONNECT #############################################
void wificonnect(){     
     //WiFiが切れるとこのルーチンに入るが、WiFiを一端切って再接続をする。 WiFi.disconnect()の後にはDelayを入れる必要あり
     //継続しないのはこのためかも＝妨害が出た場合空きチャンネルを探させるため....

  SPI.end();   //SPI切断後初期化その後WiFi初期化を実行
  delay(15);
  SPI.begin();                    
  SPI.setFrequency(CLOCK_TAKE); 
  SPI.setDataMode(SPI_MODE2);  
  delay(15);
  WiFi.disconnect();
  delay(15);    //15でじゃなきゃダメ!
  ESP.wdtFeed();
  int xmax=-1000,ymax=0;   //最適チャンネルを探す
  for(int c=0;c<13;c++){
    if(WiFi.RSSI(c)>=xmax) {xmax=WiFi.RSSI(c);ymax=c+1;}
    yield();
  }
  delay(19); yield();yield();yield();yield();yield();yield();ESP.wdtFeed();
  WiFi.begin(ssid, pass,ymax);
  Udp.begin(localPort);    //OSC受信ポートを開始させておく
  delay(15);
  while (WiFi.status() != WL_CONNECTED) {  
      delay(20);
      digitalWrite(LEDpin, HIGH);
      delay(19); yield();yield();yield();yield();yield();yield();ESP.wdtFeed();
      digitalWrite(LEDpin, LOW);
  }
  delay(5);
}

//################################# SET UP ########################################
void setup(){

SPI.end(); //再接続を考慮して一応SPIを止める

//***************SD CARD ID,PASS Read!!**********************
digitalWrite(LEDpin, HIGH);
pinMode(1, OUTPUT);
pinMode(3, OUTPUT);
pinMode(4, OUTPUT);
pinMode(5, OUTPUT);
//***************LEPTON Read!!**********************

  pinMode(LEDpin, OUTPUT);  
  SPI.begin();                    //SPIの設定はSPI.begin()の後に記述すること！！ システム上何もしなければSSは15
  SPI.setFrequency(CLOCK_TAKE);   //SPIクロック設定
  SPI.setDataMode(SPI_MODE2);    //esp8266のみMODE2 他は MODE3
  PIN_OUT =     (1 << 15);       //SSピン高速化の前処理
  PIN_ENABLE =  (1 << 15);
  delay(500);         //再接続後このDelayが効く模様
  PIN_15 = 1; //LOW
  //***************
  SPI.setHwCs(true);
  //***************
  WiFi.disconnect();
  delay(15);    //じゃなきゃダメ!
  WiFi.begin(ssid, pass,6);  //SDから読み取ったssid と passを設定してWIFIスタート
  wificonnect();
  sprintf(ip_ESP, "%d.%d.%d.%d", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3]); //ローカルipを文字列に変換iPhoneへ送る下準備
  Udp.begin(localPort);    //OSC受信ポートを開始させておく
  delay(15); 
}
//################################# SPI date reading command ########################################
//Loop中、どの段階でも画面の先頭を見つけ出して、一挙に　4セグメント＝１画面　分読み込む。２バイト整数配列にしてしまうと
//ESP8266のメモリオーバーとなるため、計算は複雑になるが1バイト１次元配列以外方法なし。ソースはテストに基づくオリジナル。
//完成しているので手をつけないこと!!

void read_lepton_frame(void) //<---SPIリードコマンド。
{      
  delayMicroseconds(5800);  //********************************<-----この数値でなければダメ！！
  lepton_image1[3280] = 0;      //164*20=3280
  while ((lepton_image1[3280] & 0b01110000) != 0x10) {    //確実に１セグメント目を捕まえるための処理。２０行目にセグメント番号が出力される。
    delay(1);    //　ESP.wdtFeed()使うとき、ここに絶対必要　しかもdelay(1)である必要あり。yieldではダメ。
    uint16_t data = 0x0F;
    while (data != 0 ) //エラー行を読み飛ばすための処理 セグメントの一番最初の行を捕まえる。0x0fよりこの方が早い
    {
      SPI.transferBytes(0x0000, lepton_image1, 164);    //標準SDKではバイト単位で処理しているがスピードが間に合わないため1行分一挙に読み込む
      data =  (lepton_image1[1]);
    }
    for (int frame_number = 1; frame_number < 60; frame_number++) {        //セグメント1:1行目は２つに分けざるを得ない
      SPI.transferBytes(0x0000, &lepton_image1[frame_number * 164], 164); //セグメント1:2行目目から１度に読み込まないとスピードが間に合わない。
    }
  }
  for (int kk = 0; kk < 3; kk++) {                                      //セグメント2~4はdelay（５）を挟んで一挙に読み込む。delayMicrosecondsを使った方がいくらか良い。
    delayMicroseconds(5000);      //基本的には50000
    int ff = 60 * kk + 60;  //スピード確保。数値演算が3x3x60=540回分減る。気休めだが。
    for (int frame_number = ff; frame_number < ff + 60; frame_number++) {
      SPI.transferBytes(0x0000, &lepton_image1[frame_number * 164], 164);
    }
  }
  delayMicroseconds(50);
}
//################################## MAIN ROUTINE #####################################################
void loop()
{
  //iPhone 側がプログラム停止している時頻繁にリセットが掛かり、SPIとのバランスが崩れて最終的に画像が止まってしまうので、iPhoneから信号が来ていない時は、LEPTONから
  //データを読み込まないようにした。Udp.endPacket()でパケットの行き場所がなくなりエラーになるのが原因の模様
  if (WiFi.status() == WL_CONNECTED)  {
  yield();
  //*****************Send osc data*****************
  //自分のipを含めた4つのデータをiPhoneへ送信。iPhoneからは設定値が送られてくる
  msg1.add(value_min).add(value_max).add(tempXY).add(ip_ESP);
  sendOSC(msg1);
  yield(); yield();
  //**********Osc Receive**********　OSCデータ受信は最後に処理しないと途中で全体が止まる
  OSCBundle bundle;   //iPhoneから送られてきた設定データを待って、画像送信を開始する
  int size = Udp.parsePacket();
  if (size > 0) {
   // Serial.println(size);
    size=56;
    while (size--) {
          bundle.fill(Udp.read());
          yield();
    }
    if (!bundle.hasError()) {

      bundle.dispatch("/t", touch);
      send_ios();  //画像読み取り送信ルーチンの本体を実行
    }
  }
  else led();
  yield();
  //************** LEPTON SIDE STATIONARY TRANSACTION *************************
}
else {wificonnect(); }
}

//################################# iPhone Touch XY #################################
void touch(OSCMessage &msg) {
  float sli = sl_256;
  touchX  = msg.getInt(0);    //iPhone上のカーソル位置　X
 // if( touchX<0 || touchX >80) touchX=80;
  touchY  = msg.getInt(1);    //iPhone上のカーソル位置　Y
 // if( touchY<0 || touchY >240) touchY=120;
  sl_256  = msg.getFloat(2);  //ダイナミックレンジスラーダーの数値
  int huri=   msg.getInt(3);  //予備
  th_l    =   msg.getFloat(4);  //カラーレンジ設定下限温度
 // if(th_l != 999 &&(th_l<=0 || th_l>100)) th_l=20;
  th_h    =   msg.getFloat(5);  //カラーレンジ設定上限温度
 // if(th_h != 999 && (th_h<=0 || th_h>100)) th_l=39;
  if (sl_256 > 0.95 || sl_256 < -0.95) sl_256 = sli;
}
//################################# image sending to iPhone #################################
void sendOSC(OSCMessage &msgx) {
  int upe=0;
    if (WiFi.status() == WL_CONNECTED)  {
       while(Udp.beginPacket(outIp, outPort) != 1){yield(); delay(9);} // ->20->9->で　bloutoothに強くなるようだ 10だとbluetoothに弱い　30だと止まる場合あり でも15
       msgx.send(Udp); 
       while(Udp.endPacket()!=1 && upe<6) {   //WIFIが切れて以前は3個であまり上手くいかなかった。4行配信再トライ回数。現状10ではまだダメで6回で画像が続く。画像が乱れにくくなる
                                             //回数が少ないと2〜3分でESPが完全に止まることが多かったがSPIクロックとこの関数内のDELAY調整で可能になる
              yield();   
              msgx.send(Udp);//yield();ESP.wdtFeed();           
              led();       
              if (WiFi.status() != WL_CONNECTED) {wificonnect();} //yield();delayMicroseconds(50);yield(); } 
              upe++;
             
      }   //パケットが紛失するとエラー＝０になる。全体のWDTリセットはこれに起因する模様。
          //再送してもシステムが止まってしまうので、delay調整が最良。9と１０にすればiPhoneのBlueToothにいくらか強くなる。
          //その後エラーが起きたらdelayの後パケットを再送出ことで、ほとんどノイズなくなる！！！！！！！！
          msgx.empty();
    }   
    else {wificonnect();}
}
//################################# image sending to iPhone #################################
void send_ios()   //完成しているので何一つ手をつけないこと!!
{
  int av5,p, tempXY1, reading = 0;
  int col;
  unsigned int min = 65536;
  unsigned int max = 0;
  float av3;
  float diff;

yomikomi:  read_lepton_frame();   //とにかくLEPTON3からデータを読み込む。
  //**********　temp calculation　**********
  for (int frame_number = 0; frame_number < 240; ++frame_number) {
    int pxx=frame_number * 164;  
    for (int i = 2; i < 82; ++i)
    {
      int px = pxx + 2 * i;
      p = (lepton_image1[px] << 8 | lepton_image1[px + 1]);
      if (p < min) min = p;
      else if(p > max) max = p;
    }
  }
  int touch_A=touchY * 164 + 2 * (touchX+2);
  tempXY1= (lepton_image1[touch_A] << 8 | lepton_image1[touch_A+ 1]); //iPhoneをタッチしたアドレスの温度データ
  diff = (max - min) * (1.0f - abs(sl_256))/256.0f; //高温側を256等分 iPhoneスライダーに連動 標準は0。最高で0.9
  if(diff < 1.0f) diff = 1.0f;  //256階調で表現できなくなったらノイズを減らすため強制的に256階調演算をさせる
  int rrr = (max - min) * sl_256;         //開始低温度側温度
  if (sl_256 > 0) min = min + rrr; //最低温度表示も開始温度に連動させる
  else max = max + rrr; //最高温度表示も開始温度に連動させる minはそのまま
  //if (min<=0) goto yomikomi;
  float keisu,fpatemp_f ;
 fpatemp_f = -273.15f ;keisu=0.01;
  
  value_min = keisu * min     + fpatemp_f;
  value_max = keisu * max     + fpatemp_f;
  tempXY    = keisu * tempXY1 + fpatemp_f;
  if(th_l != 999){   //一応エラーチェック　温度範囲指定の時の事前計算
            float av1=255.0f/(th_h-th_l);
                  av3=av1/(255.0f/(value_max-value_min));
                  av5=(value_min-th_l)*av1;
            yield();
            }
  else {av3=1; av5=0;}   //iPhoneから送られてくる温度差が0以下なら通常の色設定にする
  float avx=av3/diff;
  int   avxx=int(-(int) min*avx+av5);
  //グレースケール変換データ送信。型はStringだが実質バイナリデータとして送信している。数値をASCII変換すると最小で３倍のデータ量になるため、通信条件が悪化する。
  //問題は送信数値が0になったとき。今回は視覚上のデータなので255色中0が1になったところでまったく問題がない。従って0の場合は、1に強制変換。
  ESP.wdtFeed();

  for (int frame_number = 0; frame_number < 240; frame_number += 4) {
    if( ! frame_number % 14) delayMicroseconds(100); //適度にdelayを入れて混線しにくくする（相手に余裕を与える！！）
    for (int ii = 0; ii < 4 ; ++ii) //5行ずつ送信 String型は450文字前後が実験による最大値。よって５行が限界。
    {
      int iix = ii * 81;
      for (int i = 2; i < 82; ++i)
      {
        int ax = (frame_number + ii) * 164 + 2 * i;
        col = (int)(lepton_image1[ax] << 8 | lepton_image1[ax + 1])*avx ; //(int)を入れないと符号無し演算になる!!
        col = col+avxx;   //別計算にしないとシステムダウンする-->メモリーオーバー
        if (col <= 0)   col = 1; //グレーカラー0の場合Stringの終端扱いになるため1にする。視覚上は許容範囲。
        else if(col > 255) col = 255;   
        lepton_image1[iix + i - 1] = col;  //メモリー節約のため　同じ配列を使う。最初の406文字だけを使うので重複しない。
        ESP.wdtFeed();   //ESP8266 CPUを160MHzとした場合ここにもESP.wdtFeed()が必要。2個である必要あり
        ESP.wdtFeed();   //<<<<<<---これを入れないのが諸悪の原因。yieldは遅くなるだけ。
      }
      lepton_image1[iix] = frame_number + ii; //一番最初のその他の行にはを行番号代入
      ESP.wdtFeed();  //ハイスピードではこれを入れるNo1 
      yield();        //ハイスピードではこれを入れるNo2
    }
    if ( frame_number == 0 ) lepton_image1[0] = 240; //最初の行の初めにに240を書き込んで　MacまたはiPhoneに渡す。0では改行になってしまうため
    lepton_image1[325] = '\0'; //ストリング型の終端処理
    //yield();yield();yield();//安定化のために必要かも
    //*****************Send osc data********************
    msg2.add( (char *)lepton_image1 );
    sendOSC(msg2);
    yield();      // <----絶対必要
    //*****************Send osc data********************
    delayMicroseconds(4);//入れた方が良い
  }
}
