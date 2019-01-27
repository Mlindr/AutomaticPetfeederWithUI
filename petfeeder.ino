#include <LiquidCrystal.h>
//#include <Ethernet.h>

/*//Ethernet
String readString;// stores the HTTP request
byte mac[] = { 0x64, 0x18, 0x22, 0x40, 0x30, 0x28 };
EthernetServer server(80);// normal http port = 80
*/

volatile int keskeytyslippu;
volatile int ethernet_laskuri;
volatile int16_t keskeytyshetki, keskhetki_servo, servo, servo_time;
int ruoka_korkeus;
bool kellolippu;
bool ajanasetuslippu;
bool aika_asetettu = 0;
int ajan_naytto=0;

/*************GUI X ja Y AKSELIT***************/
int x = 0;
int x_vanha;
int y = 0;
int y_vanha;

int x_aika = 0;

/***************KELLONAJAT*************/
int tunnit = 0;
int tunnit_aamupala = 0;
int tunnit_lounas = 0;
int tunnit_iltapala = 0;

int minuutit = 0;
int minuutit_aamupala = 0;
int minuutit_lounas = 0;
int minuutit_iltapala = 0;

int sekuntilaskuri = 0;
int sekunti = 0;
int sekunti_aamupala = 0;
int sekunti_lounas = 0;
int sekunti_iltapala = 0;

/************LCD****************/
const int rs = 7, en = 6, d4 = 5, d5 = 4, d6 = 3, d7 = 2;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);//sets LCD pins

/**************KYTKIMET********************/
unsigned int vanhakytkin;
unsigned int kytkin;
unsigned int kytkin_painettu;

unsigned int kytkin_down;
unsigned int vanhakytkin_down;
unsigned int kytkin_tila_down;

unsigned int kytkin_up;
unsigned int vanhakytkin_up;
unsigned int kytkin_tila_up;

unsigned int kytkin_vasen;
unsigned int vanhakytkin_vasen;
unsigned int kytkin_tila_vasen;

unsigned int kytkin_oikea;
unsigned int vanhakytkin_oikea;
unsigned int kytkin_tila_oikea;

void setup() {

  servo=-1;//servo off
  /*I/O-port registers INPUT/OUTPUT setup*/
  DDRB = 0x02;
  DDRC = 0x01;

  //Ethernet
  Serial.begin(9600);
  //Serial.print("Waiting IP num. ");
  //ask_IP();    // Pick IP number
  //server.begin();  // start server

  /*TIMER1 in use,16 bit register, using compare B, interrupt setup*/
  keskeytyshetki=20000;
  keskhetki_servo=20000;
  TCCR1A=0x00;//clear on compare match, when timer reaches OCR0B value=10ms
  TCCR1B=0x02;// prescaler of 8->16MHz/8=2MHz=0,5us
  OCR1B=keskeytyshetki;//20000*0,5us=10ms
  OCR1A=keskhetki_servo;//40000*0,5us=20ms
  TIMSK1=0x06;//OCR1B enabled as interrupt service

  /*LCD setup*/
  lcd.begin(16, 2);// set up the LCD's number of columns and rows
  lcd.display();// Turn on the display
  delay(1000);
}

void loop() {

  //set where to start on the lcd
  lcd.setCursor(0, 0); //(row,column)

  kytkintoiminnot();

  kayttoliittyma();
  
  automaattinen_ruoka();

  /****RUUAN MITTAUS****/
  ruoka_korkeus = ping();
}

/****************************************KYTKIMIEN ASETUKSET*****************************************************************/
void kytkimet(){
  
    /******************kytkimien ja muut tarpeelliset alustukset******************/
    keskeytyslippu = 0x00; //set interrupt flag 0

    //set button pressed 10ms ago as old switch state variable "vanhakytkin" value
    vanhakytkin = kytkin; 
    vanhakytkin_down = kytkin_down;
    vanhakytkin_up = kytkin_up;
    vanhakytkin_oikea = kytkin_oikea;
    vanhakytkin_vasen = kytkin_vasen;

    //set UI x and y coordinate values 10ms ago as old(vanha) value
    x_vanha = x;
    y_vanha = y;

    //set button as present value
    kytkin = (PINC & 0x02) >> 1;
    kytkin_down = (PINC & 0x10) >> 4;
    kytkin_vasen = (PINC & 0x04) >> 2;
    kytkin_oikea = (PINC & 0x08) >> 3;
    kytkin_up = (PINC & 0x20) >> 5;

    /*******************JOS KISSAN KYTKIN PAINETTU **********************/
    //manual switch to turn servo and give food, "pet switch", can be used by the pet
    if (kytkin == 1) { //if button state is 1 (pressed)
      if (vanhakytkin == 0) { //and if old switch state was 0, so a state change has happened wich is not jittering

        kytkin_painettu = 1; //buttonflag is 1
      }
    }

    /*******If DOWN button is pressed***/
    else if  (kytkin_down == 1) { // if buttun state is 1 (pressed)
      if (vanhakytkin_down == 0) {
        kytkin_tila_down = 1;
      }
    }

    /*******jos UP  nappi painettu***/
    else if  (kytkin_up == 1) { // if buttun state is 1 (pressed)
      if (vanhakytkin_up == 0) {
        kytkin_tila_up = 1;
      }
    }

    /*******jos VASEN  nappi painettu***/
    else if  (kytkin_vasen == 1) { // if buttun state is 1 (pressed)
      if (vanhakytkin_vasen == 0) {
        kytkin_tila_vasen = 1;
      }
    }

    /*******jos OIKEA  nappi painettu***/
    else if  (kytkin_oikea == 1) { // if buttun state is 1 (pressed)
      if (vanhakytkin_oikea == 0) {
        kytkin_tila_oikea = 1;
      }
    } 
}

void aika(){
  
  if(sekuntilaskuri==100){
      
    sekuntilaskuri=0;
      
    if(sekunti<59){
      sekunti++;  
    }
    else{
      sekunti=0;
      if(minuutit<59){
        minuutit++;
      }
      else{
        minuutit=0;
        if(tunnit < 23){
          tunnit++;
         }
         else{
          tunnit=0;
        }
      }
    }
  }
}

/*********************************KELLO KESKEYTYSPALVELU B*******************************/
ISR(TIMER1_COMPB_vect){

    //viritetään seuraava keskeytyksen hetki
    keskeytyshetki+=20000;
    OCR1B=keskeytyshetki;

    sekuntilaskuri++;
    ethernet_laskuri++;

    /*if(ethernet_laskuri==5){
      ethernet_laskuri=0;
      listenForEthernetClients();//listen to client every 50 ms
    }*/
    
    aika();//ajan laskuri
    
    kytkimet();//käydään kytkimet läpi
  }

/****************************SERVO KESKEYTYSPALVELU A****************************************/
ISR(TIMER1_COMPA_vect){

  if(servo==1){
    if(servo_time==1){
      servo_time=0;
      PORTC=0x01;
      keskhetki_servo+=3000;
      OCR1A=keskhetki_servo;
    }
    else if(servo_time==0){
      servo_time=1;
      PORTC=0x00;
      keskhetki_servo+=37000;
      OCR1A=keskhetki_servo;
    }  
  }
  
  if(servo==0){
    if(servo_time==1){
      servo_time=0;
      PORTC=0x01;
      keskhetki_servo+=1200;
      OCR1A=keskhetki_servo;
    }
    else if(servo_time==0){
      servo_time=1;
      PORTC=0x00;
      keskhetki_servo+=38800;
      OCR1A=keskhetki_servo;
    }  
  }
}

/***************************SERVON KÄYTTÖ********************************************/
void luukku_auki() {

  //write to servo and have delay between
  servo=1;
  delay(500);
  kayttoliittyma();
  servo=0;
  delay(500);
  servo=-1;
  kayttoliittyma();
  
}

/************************************RUUAN MITTAUS************************************************/
int ping()
{
  // establish variables for duration of the ping,
  // and the distance result in inches and centimeters:
  long kesto, syvyys;
  // The PING))) is triggered by a HIGH pulse of 2 or more microseconds.
  // Give a short LOW pulse beforehand to ensure a clean HIGH pulse:

  PORTB = 0x00;
  delayMicroseconds(5);
  PORTB = 0x02;
  delayMicroseconds(10);
  PORTB = 0x00;

  kesto = pulseIn(8, HIGH);

  // convert the time into a distance
  syvyys = microsecondsToCentimeters(kesto);
  return syvyys;
}

long microsecondsToCentimeters(long microseconds)
{
  // The speed of sound is 340 m/s or 29 microseconds per centimeter.
  // The ping travels out and back, so to find the distance of the
  // object we take half of the distance travelled.
  return microseconds / 29 / 2;
}

/*****************************AJAN TULOSTUS***************************************/
void naytaKello() {

  lcd.clear();
  lcd.print("kello on:");
  lcd.setCursor(0, 1);
  lcd.print(tunnit);
  lcd.print(":");
  lcd.print(minuutit);
  lcd.print(":");
  lcd.print(sekunti);
}

/*******************************KÄYTTÖLIITTYMÄN TULOSTUS********************************************/
void kayttoliittyma() {

  if (x == 0 && x_vanha > 0) {
    y = 0;
  }

  if (x == 0) {

    if (y == 0) {
      lcd.clear();
      lcd.print("Asetukset");
      kellolippu = 0;
      ajan_naytto=0;
    }

    else if (y == 1) {
      lcd.clear();
      lcd.print("Kellonaika");
      kellolippu = 1;
    }
  }

  /*************X=1***********************/
  if (x == 1) {
    if (kellolippu != 1) {
      if (y == 0) {
        lcd.clear();
        lcd.print("Aamupala");
      }

      else if (y == 1) {
        lcd.clear();
        lcd.print("lounas");
      }

      else if (y == 2) {
        lcd.clear();
        lcd.print("iltapala");
      }

      else if (y == 3) {
        lcd.clear();
        lcd.print("kellonaika");
      }
    }
    else {
      ajan_naytto=1;
      lcd.clear();
      naytaKello();
    }
  }
  /***************X=2************************/
  if (x == 2) {

    ajanasetuslippu = 1;

    if (y == 0) {
      lcd.clear();
      lcd.print("aseta aamupala: ");
      lcd.setCursor(0, 1);

      lcd.print(tunnit_aamupala);
      lcd.print(":");
      lcd.print(minuutit_aamupala);
    }

    else if (y == 1) {
      lcd.clear();
      lcd.print("aseta lounas:");
      lcd.setCursor(0, 1);
      lcd.print(tunnit_lounas);
      lcd.print(":");
      lcd.print(minuutit_lounas);
    }

    else if (y == 2) {
      lcd.clear();
      lcd.print("aseta iltapala:");
      lcd.setCursor(0, 1);
      lcd.print(tunnit_iltapala);
      lcd.print(":");
      lcd.print(minuutit_iltapala);
    }

    else if (y == 3) {
      aika_asetettu = 1;
      lcd.clear();
      lcd.print("aseta kellonaika:");
      lcd.setCursor(0, 1);
      lcd.print(tunnit);
      lcd.print(":");
      lcd.print(minuutit);
    }
  }
}

/*************************************************KYTKIMIEN TOIMINTA*************************************************************/
void kytkintoiminnot() {

  //If pet's own feeding button was pressed
  if (kytkin_painettu == 1) {
    
    kytkin_painettu = 0x00;
    luukku_auki();
  }

  //if down button was pressed
  else if (kytkin_tila_down == 1 && ajan_naytto==0) {

    kytkin_tila_down = 0;
    if (ajanasetuslippu == 0) {
      if (x > 0 && y != 3) {
        y++;
      }

      else if (x == 0 && y != 1) {
        y++;
      }

      else {
        y = 0;
      }
    }

   else if (y == 0) {

      if (x_aika == 0) {
        if (tunnit_aamupala > 0) {
          tunnit_aamupala--;
        }
        else {
          tunnit_aamupala = 0;
        }
      }

      else if (x_aika == 1) {
        if (minuutit_aamupala > 0) {
          minuutit_aamupala--;
        }
        else {
          minuutit_aamupala = 59;
          tunnit_aamupala--;
        }
      }
    }

    else if (y == 1) {

      if (x_aika == 0) {
        if (tunnit_lounas > 0) {
          tunnit_lounas--;
        }
        else {
          tunnit_lounas = 0;
        }
      }

      else if (x_aika == 1) {
        if (minuutit_lounas > 0) {
          minuutit_lounas--;
        }
        else {
          minuutit_lounas = 59;
          tunnit_lounas--;
        }
      }
    }

    else if (y == 2) {

      if (x_aika == 0) {
        if (tunnit_iltapala > 0) {
          tunnit_iltapala--;
        }
        else {
          tunnit_iltapala = 0;
        }
      }

      else if (x_aika == 1) {
        if (minuutit_iltapala > 0) {
          minuutit_aamupala--;
        }
        else {
          minuutit_iltapala = 59;
          tunnit_iltapala--;
        }
      }
    }

    else if (y == 3) {

      if (x_aika == 0) {
        if (tunnit > 0) {
          tunnit--;
        }
        else {
          tunnit = 0;
        }
      }

      else if (x_aika == 1) {
        if (minuutit > 0) {
          minuutit--;
        }
        else {
          minuutit = 59;
          tunnit--;
        }
      }
    }
  }

  //if up button was pressed
  else if (kytkin_tila_up == 1 && ajan_naytto==0) {

    kytkin_tila_up = 0;

    if (ajanasetuslippu == 0) {
      if (y != 0) {
        y--;
      }

      else if (x == 0 && y == 0) {
        y = 1;
      }

      else {
        y = 3;
      }
    }

    else if (y == 0) {

      if (x_aika == 0) {
        if (tunnit_aamupala < 23) {
          tunnit_aamupala++;
        }
        else {
          tunnit_aamupala = 0;
        }
      }

      else if (x_aika == 1) {
        if (minuutit_aamupala < 59) {
          minuutit_aamupala++;
        }
        else {
          minuutit_aamupala = 0;
          tunnit_aamupala++;
        }
      }
    }

    else if (y == 1) {

      if (x_aika == 0) {
        if (tunnit_lounas < 23) {
          tunnit_lounas++;
        }
        else {
          tunnit_lounas = 0;
        }
      }

      if (x_aika == 1) {
        if (minuutit_lounas < 59) {
          minuutit_lounas++;
        }
        else {
          minuutit_lounas = 0;
          tunnit_lounas++;
        }
      }
    }

    else if (y == 2) {

      if (x_aika == 0) {
        if (tunnit_iltapala < 23) {
          tunnit_iltapala++;
        }
        else {
          tunnit_iltapala = 0;
        }
      }

      if (x_aika == 1) {
        if (minuutit_iltapala < 59) {
          minuutit_aamupala++;
        }
        else {
          minuutit_iltapala = 0;
          tunnit_iltapala++;
        }
      }
    }

    else if (y == 3) {

      if (x_aika == 0) {
        if (tunnit < 23) {
          tunnit++;
        }
        else {
          tunnit = 0;
        }
      }

      if (x_aika == 1) {
        if (minuutit < 59) {
          minuutit++;
        }
        else {
          minuutit = 0;
          tunnit++;
        }
      }
    }
  }

  //if button to the right was pressed
  else if (kytkin_tila_oikea == 1 && ajan_naytto==0) {

    kytkin_tila_oikea = 0;

    if (ajanasetuslippu == 0) {
      if (x < 3) {
        x++;
      }
      else {
        x = 0;
      }
    }
    else if (x_aika < 1) {
      x_aika++;
    }

    else if (x_aika == 1) {
      ajanasetuslippu = 0;
      x_aika = 0;
      y = 0;
      x = 0;
    }
  }

  //if button to the left was pressed
  else if (kytkin_tila_vasen == 1) {

    kytkin_tila_vasen = 0;

    if (ajanasetuslippu == 0) {
      if (x = !0) {
        x--;
      }
      else {
        x = 0;
      }
    }

    else if (x_aika == 1) {
      x_aika--;
    }

    else if (x_aika == 0) {
      if (y == 0) {
        x--;
        ajanasetuslippu = 0;
        tunnit_aamupala = 0;
        minuutit_aamupala = 0;
      }

      if (y == 1) {
        x--;
        ajanasetuslippu = 0;
        tunnit_lounas = 0;
        minuutit_lounas = 0;
      }

      if (y == 2) {
        x--;
        ajanasetuslippu = 0;
        tunnit_iltapala = 0;
        minuutit_iltapala = 0;
      }

      if (y == 3) {
        x--;
        ajanasetuslippu = 0;
        tunnit = 0;
        minuutit = 0;
      }
    }
  }
}

void automaattinen_ruoka() {

  if (aika_asetettu == 1 && ajanasetuslippu != 1) {

    if (tunnit_aamupala == tunnit && minuutit_aamupala == minuutit && sekunti_aamupala == sekunti) {
      luukku_auki();
    }

    else if (tunnit_lounas == tunnit && minuutit_lounas == minuutit && sekunti_lounas == sekunti) {
      luukku_auki();
    }

    else if (tunnit_iltapala == tunnit && minuutit_iltapala == minuutit && sekunti_iltapala == sekunti) {
      luukku_auki();
    }
  }
}
/*
void ask_IP(void) {

  byte rev;

  //// Open ethernet connection with myMAC number
  rev = Ethernet.begin(mac);

  if ( rev == 0) {

    // Ethernet connection failed
    Serial.print("Connection failed.");
  }

  else{
  
    Serial.print( "IP number = " );
    Serial.println( Ethernet.localIP() );
    delay(500);
  }
}

void listenForEthernetClients() {

  // listen for incoming clients

  EthernetClient client = server.available();

  if (client) {
    while (client.connected()) {

      if (client.available()) {

        char c = client.read();

        //read char by char HTTP request

        if (readString.length() < 100) {

          //store characters to string
          readString += c;
        }
        
        // if you've gotten to the end of the line (received a newline
        // character) and the line is blank, the http request has ended,
        // so you can send a reply
        if (c == '\n'){

          Serial.println(readString); //print to serial monitor for debuging

          // send a standard http response header
          client.println("HTTP/1.1 200 OK");

          client.println("Content-Type: text/html");

          client.println();

          client.println("<meta http-equiv='refresh' content='4'/>");// web page update interval 2 second

          client.println("<HTML>");

          client.println("<HEAD>");

          client.println("<TITLE>Lemmikin ruokintasysteemi </TITLE>");

          client.println("</HEAD>");

          client.println("<BODY>");

          client.println("<h1> Purkissa on ruokaa = ");

          client.println("<FONT color=""green"">");

          int purkissa_ruokaa = 24-ruoka_korkeus;            
          char tbuf[7]="";

           sprintf(tbuf,"%4d",purkissa_ruokaa);
           client.println(tbuf);
           client.print("cm");
            

          client.println("</FONT> </h1>");

          client.println("<br>");

          client.print("<br><br>");

          client.println("<br />");

          client.println("</BODY>");

          client.println("</HTML>");

          delay(1);

          //stopping client
          client.stop();

          //clearing string for next read
          readString = "";
        }
      }
    }
  }
}*/
