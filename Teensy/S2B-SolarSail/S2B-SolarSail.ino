#include <Adafruit_NeoPixel.h>
#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>
#include <Adafruit_LSM9DS1.h>
#include <Adafruit_Sensor.h> 
#define SCB_AIRCR (*(volatile uint32_t *)0xE000ED0C)
#include "Synth3.h"
#ifdef __AVR__
#include <avr/power.h>
#endif

//__________________________________________________________________DEBUGGING
// #define DbgWAV
#define DbgTIMELINE
//#define DbgPULSEFADE
//#define DbgGYRO
//#define DbgTARGETS
//#define DbgRAINBOW
//#define DbgCOLLISIONS
//#define DbgCOLLISIONS2 
//__________________________________________________________________NEOPIXEL SETUP
#define PIN 0
float brightnessMult = 0.4;//<<<<==========================================================================SET PIXEL BRIGHTNESS<<<<<<<<<<<==================SET PIXEL BRIGHTNESS
//brightnessMult will scale the brightness of the neopixels using a number between 0-1
Adafruit_NeoPixel strip = Adafruit_NeoPixel(24, PIN, NEO_GRB + NEO_KHZ800);
float red;
float green;
float blue;

//__________________________________________________________________TEENSY AUDIO SETUP
Synth3 synth;
AudioOutputI2S        out;
AudioControlSGTL5000  audioShield;
AudioMixer4           mixerLeft;
AudioMixer4           mixerRight;
AudioPlaySdWav        playWav1;
///*route audio*/
AudioConnection       patchCord0(synth,0,mixerLeft,0);
AudioConnection       patchCord1(synth,1,mixerRight,0);
AudioConnection       patchCord2(playWav1,0,mixerRight,1);
///**/
AudioConnection       parchCord3(mixerLeft, 0, out, 0);
AudioConnection       parchCord4(mixerRight, 0, out, 1);
const int VOL_PIN = 15;
float vol = 0;
float adriftVol = 0.7;
float fmVol = 0.07;
float kickVol = 0.09;
float bassVol = 0.14;
float wavVol = 0.1;
int pitIter=0;
int pitchSet[8];
int fmEuclid[2];

//__________________________________________________________________WAV FILE SETUP
#define SDCARD_CS_PIN    10
#define SDCARD_MOSI_PIN  7
#define SDCARD_SCK_PIN   14
int wavPlaying = 0;
int wavBroken = 0;
// float wavPos=0;
float section=0;

//__________________________________________________________________GYRO SETUP
Adafruit_LSM9DS1 lsm = Adafruit_LSM9DS1();
float movThresh = 0.2;
float x=0;
float y=0;
float z=0;
float xSmooth[10];
float ySmooth[10];
float zSmooth[10];
int gyroCirc=0;
float gSlidX[2]={0,0};
float gSlidY[2]={0,0};
float gSlidZ[2]={0,0};
float movement = 0;
float pos=0;
int cruise=0;


void setupGyro(){
  /* 1.) Set the accelerometer range*/
  lsm.setupAccel(lsm.LSM9DS1_ACCELRANGE_2G);
  /* 2.) Set the magnetometer sensitivity*/
  lsm.setupMag(lsm.LSM9DS1_MAGGAIN_4GAUSS);
  /* 3.) Setup the gyroscope*/
  lsm.setupGyro(lsm.LSM9DS1_GYROSCALE_245DPS);
}

//__________________________________________________________________GAME LOGIC
long timeline = 0;
int state = 0;
int introEnd = 50000;
int collectEnd = 180000;
int cruiseEnd =  290000;
int outroEnd = 335000;
int target = -1;




//__________________________________________________________________SETUP
void setup() {  
  Serial.begin(115200); 
  Serial.println("HELLO");

  //_______________________________Start LEDs
  strip.begin();
  brightnessMult=constrain(brightnessMult,0,1);

  //_______________________________Start Gyro
  if (!lsm.begin()){
      Serial.println("Oops ... unable to initialize the LSM9DS1.");
    }
  Serial.println("Found LSM9DS1 9DOF");
  setupGyro();

  //_______________________________Start Audio
  AudioMemory(20);
  audioShield.enable();
  mixerLeft.gain(0, 0.35);
  mixerRight.gain(0, 1);
  mixerRight.gain(1, 0); 
  synth.setParamValue("adriftVol",0.4);
  synth.setParamValue("masterVol", 0);
  delay(1000);
  audioShield.volume(1);

//_______________________________Start WAV player
  SPI.setMOSI(SDCARD_MOSI_PIN);
  SPI.setSCK(SDCARD_SCK_PIN);
  delay(10);
  if (!(SD.begin(SDCARD_CS_PIN))) {
    wavBroken=1;
    Serial.println("Unable to access the SD card");
   
  }
}


//__________________________________________________________________LOOP
void loop() {
  if(wavBroken==0){
    playAudio("EXP4.WAV");  // filenames are always uppercase 8.3 format  
  } else {
    // Serial.println("WavBroken!");
    readGyro();
    readVol();
    setTimeline();
    game();
  }
}

//__________________________________________________________________PLAY AUDIO
void playAudio(const char *filename)
{
  #ifdef DbgWAV//????????????????
  Serial.print("Playing file: ");
  Serial.println(filename);
  #endif 
  /*Start playing the file. This sketch continues to run while the file plays.*/
  playWav1.play(filename);
  /* A brief delay for the library read WAV info */
  delay(250);
  /* Wait for the file to finish playing. */
  if(playWav1.isPlaying()){
    while (playWav1.isPlaying()) {
    /*The rest of the code will execute here while the file plays*/
    readGyro();
    readVol();
    setTimeline();
    game();
    }
  } else {
    wavBroken=1;
  }
}




//__________________________________________________________________GAME
void game() {
  if(state==0){
    setIntro();
  } else if(state==1){
    collecting();
//    Serial.println("collecting");  
  } else if (state==2){
    cruising();
  } else if (state==3){
    fadeOut();  
  } else if (state==4){
    resetGame();
  }
};

//__________________________________________________________________TIMELINE
void setTimeline(){
  if(wavBroken==0){
    timeline = playWav1.positionMillis();
  } else {
    timeline  = millis();
  }
  
  if (timeline<introEnd) {//intro, adrift
    state=0;
  } else if (timeline<collectEnd) {//start collecting
    state=1;
  }else if (timeline< cruiseEnd) {//batteries charged
    state=2;
  } else if (timeline< outroEnd) {//end & reset
    state=3;
  } else if (timeline>= outroEnd){
    state=4;
  }
  #ifdef DbgTIMELINE
  Serial.print("timeline: ");
  Serial.print(timeline);
  Serial.print(" ");
  Serial.println(state);
  #endif
}

//__________________________________________________________________GYRO
void readGyro(){
  /*get gyro readings*/
  lsm.read(); 
  sensors_event_t a, m, g, temp;
  lsm.getEvent(&a, &m, &g, &temp); 
  x=abs(g.gyro.x);
  y=abs(g.gyro.y);
  z=abs(g.gyro.z);
  gyroCirc=(gyroCirc+1)%10;
  float gyroTallyX=0;
  float gyroTallyY=0;
  float gyroTallyZ=0;
  xSmooth[gyroCirc]=x;
  ySmooth[gyroCirc]=y;
  zSmooth[gyroCirc]=z;
  for(int i=0; i<10; i++){
    gyroTallyX=gyroTallyX+xSmooth[i];
    gyroTallyY=gyroTallyY+ySmooth[i];
    gyroTallyZ=gyroTallyZ+zSmooth[i];
  };
  x=gyroTallyX/10;
  y=gyroTallyY/10;
  z=gyroTallyZ/10;
  if(x<movThresh){x=0;}
  if(y<movThresh){y=0;}
  if(z<movThresh){z=0;}

  movement = x+y+z;

  pos = round(pos+movement);
  cruise = round(cruise+movement);
  #ifdef DbgGYRO
  Serial.print(movement);
  Serial.print("   ");
  Serial.print(x);
  Serial.print("   ");
  Serial.print(y);
  Serial.print("   ");
  Serial.print(z);
  Serial.print("   ");
  Serial.print(cruise);
  Serial.print("   ");
  Serial.println(pos);
  #endif
}

//__________________________________________________________________VOL
void readVol(){
  float vol= analogRead(VOL_PIN);
  vol=map(vol,0,1023,0,1);
  synth.setParamValue("masterVol", vol);
  mixerRight.gain(1, vol*wavVol);
//  Serial.println(vol);
  }

void fadeOut(){
  float fade = map(float(timeline), float(cruiseEnd), float(cruiseEnd+30000), 1.0,0.0);
  fade = constrain(fade, 0.0,1.0);
  Serial.print("FADE.  ");
  Serial.println(fade);
  mixerLeft.gain(0, 0.35*fade);
  mixerRight.gain(0, fade);
}

//__________________________________________________________________GAME FUNCTIONS
void collecting(){
  pulsing();
  adrift();
  collisions();
  strip.show();
}

float pulseFade;
void pulsing(){
  float brgt = map(float(timeline),float(introEnd),float(collectEnd), 0, 1.0);
  brgt = constrain(brgt, 0, 1.0);
  float spd = map(float(timeline), float(introEnd),float(collectEnd),0.001,0.0035); 
  spd = float(timeline)*constrain(spd, 0.001, 0.0035);
  brgt = constrain(brgt,0,1.0);
  pulseFade = sin(spd) * 35 + 35 + 185;
  pulseFade = pulseFade*brgt*brightnessMult;
  pulseFade = constrain(pulseFade, 30, 255);
  green = blue = constrain(pulseFade*brgt, 100, 255)*0.8;
  red = constrain(pulseFade*brgt, 200, 255)*0.8;
  strip.setBrightness(pulseFade);
  for(int i=0; i< strip.numPixels(); i++) {
    strip.setPixelColor(i, strip.Color(red, green, blue));
  }

//==================================synth  
  synth.setParamValue("adriftVol", map(brgt, 0, 1, adriftVol, 0.1));
  
  #ifdef DbgPULSEFADE
  Serial.println(pulseFade);
  #endif
} 


void adrift(){
  float mov = constrain(movement, 0 ,5);
  mov = map(mov, 0, 5, 0, 1);
  mov = map(mov, 0,1,0.15,0.45);
  synth.setParamValue("adriftFreq", mov);
  }

int colR=0;
int colG=0;
int colB=0;

void collisions(){
  if(target== -1){
    int collisionDensityMin = map(timeline, introEnd, collectEnd, 300, 10);
    collisionDensityMin = constrain(collisionDensityMin, 10, 300);
    int collisionDensityMax = map(timeline, introEnd, collectEnd, 600, 50);
    collisionDensityMax = constrain(collisionDensityMax, 50, 600);
    target = random(collisionDensityMin, collisionDensityMax);//make this adaptive later
    colR = 0;
    colG = 0;
    colB = 0;
    while( (colR+colG+colB) <= 0){
      colR = random(2)*255;
      colG = random(2)*255;
      colB = random(2)*255;
      fmEuclid[0] = random(5, 24);
      fmEuclid[1] = random(6, 24);
    }
    
    if( (colR+colG+colB)==255 ){
        if(colR==255){
            pitchSet[0]=0;
            pitchSet[1]=2;
            pitchSet[2]=5;
            pitchSet[3]=10;
            pitchSet[4]=12;
            pitchSet[5]=5;
            pitchSet[6]=14;
            pitchSet[7]=10;
        } else if (colG==255) {
            pitchSet[0]=0;
            pitchSet[1]=4;
            pitchSet[2]=5;
            pitchSet[3]=9;
            pitchSet[4]=5;
            pitchSet[5]=12;
            pitchSet[6]=14;
            pitchSet[7]=7;
        } else {
            pitchSet[0]=2;
            pitchSet[1]=5;
            pitchSet[2]=9;
            pitchSet[3]=12;
            pitchSet[4]=5;
            pitchSet[5]=9;
            pitchSet[6]=7;
            pitchSet[7]=2;
        }
      } else if( (colR+colG+colB)==510 ){
        if((colR+colG)==510){
            pitchSet[0]=2;
            pitchSet[1]=5;
            pitchSet[2]=7;
            pitchSet[3]=11;
            pitchSet[4]=14;
            pitchSet[5]=16;
            pitchSet[6]=17;
            pitchSet[7]=19;
        } else if((colR+colB)==510) {
            pitchSet[0]=0;
            pitchSet[1]=4;
            pitchSet[2]=5;
            pitchSet[3]=8;
            pitchSet[4]=-5;
            pitchSet[5]=-7;
            pitchSet[6]=-5;
            pitchSet[7]=-2;
        } else {
            pitchSet[0]=4;
            pitchSet[1]=7;
            pitchSet[2]=9;
            pitchSet[3]=11;
            pitchSet[0]=12;
            pitchSet[1]=14;
            pitchSet[2]=16;
            pitchSet[3]=-12;
        }
        }
  }
  
  int proximity = pos-target;
  float brgt2=0;
  if(proximity>0 && proximity<157){
    brgt2 = sin(proximity*0.02)*brightnessMult;
    red = map(brgt2,0,1,red,colR);
    green = map(brgt2,0,1,green,colG);
    blue = map(brgt2,0,1,blue,colB);

    for(int i=0; i< strip.numPixels(); i++) {
      if(i%4 != 0){
        strip.setPixelColor(i, strip.Color(red, green, blue));
      }
    }
  } else if(proximity > 157){
    target = -1;
    pos = 0;
  }

  strip.setPixelColor(8, strip.Color(0, 0, 0));
  strip.setPixelColor(9, strip.Color(0, 0, 0));
  strip.setPixelColor(20, strip.Color(0, 0, 0));
  strip.setPixelColor(21, strip.Color(0, 0, 0));
  
  pitIter = (pitIter+1)%64;
  synth.setParamValue("adriftVol", map(brgt2, 0, 1, adriftVol, 0.1));
  synth.setParamValue("fmVerb", 1);
  synth.setParamValue("fmAC1",0.01);
  synth.setParamValue("fmRC1",0.05);
  synth.setParamValue("fmAM1",0.025);
  synth.setParamValue("fmRM1",0.05);
  synth.setParamValue("fmEucNo1", fmEuclid[0]);
  synth.setParamValue("fmEucNo2", fmEuclid[1]);
  synth.setParamValue("fmFreq1", 92+pitchSet[pitIter/8]);
  synth.setParamValue("fmFreq2", 92+pitchSet[pitIter/8]);
  synth.setParamValue("fmVol", fmVol*brgt2);
  synth.setParamValue("fmGateT1", brgt2>0);

  #ifdef DbgTARGETS 
     Serial.print(proximity);
     Serial.print("   ");
     Serial.print(colR);
     Serial.print("   ");
     Serial.print(colG);
     Serial.print("   ");
     Serial.print(colB);
     Serial.print("   ");
     Serial.println(brgt);
  #endif
}


void cruisingRainbow() {
  uint8_t j = round(cruise*0.25)%256;
  uint16_t i;
    for(i=0; i< strip.numPixels(); i++) {
      Wheel(((i * 256 / strip.numPixels()) + j) & 255);
      strip.setPixelColor(i, red, green, blue);
    }

    #ifdef DbgRAINBOW
    Serial.println(j);
    #endif
}


float cruiseCountin = 0;
float fadeInCruise = 0.0;
void collisions2(){
  cruiseCountin = constrain(cruiseCountin+1, 0, 2000);
  fadeInCruise = map(cruiseCountin, 0.0, 2000.0, 0.0, 1.0);
  if(target== -1){
    target = random(55, 300);//make this adaptive later
  }
  int proximity = pos-target;
  int dumR=0;
  int dumG=0;
  int dumB=0;
  int oct1=0;
  int oct2=0;
  float brgt=0;
  if(proximity>0 && proximity<314){
    brgt = sin(proximity*0.01)*brightnessMult;
  
    uint8_t j = round(cruise*0.25)%256;
    uint16_t i;
    for(i=0; i< strip.numPixels(); i++) {
      if(i%8 < 4 ){   
        Wheel(((i * 256 / strip.numPixels()) + j) & 255);
        red = map(brgt,0,1,red,colR);
        green = map(brgt,0,1,green,colG);
        blue = map(brgt,0,1,blue,colB);
        strip.setPixelColor(i, red, green, blue);
      }
    }
  } else if(proximity > 314){
    target = -1;
    pos = 0;
    while( (dumR+dumG+dumB) <= 0){
      dumR = random(2)*255;
      dumG = random(2)*255;
      dumB = random(2)*255;
      fmEuclid[0] = random(5, 24);
      fmEuclid[1] = random(6, 24);
      float aC = map(pow(random(0,1000)*0.001,4),0,1,0.005,0.1);
      float rC = map(pow(random(0,1000)*0.001,4),0,1,0.005,0.1);
      float aM = map(pow(random(0,1000)*0.001,4),0,1,0.005,0.1);
      float rM = map(pow(random(0,1000)*0.001,4),0,1,0.005,0.1);
      oct1 = random(-5,1)*12;
      oct2 = random(-5,1)*12;
      synth.setParamValue("fmAC1",aC);
      synth.setParamValue("fmRC1",rC);
      synth.setParamValue("fmAM1",aM);
      synth.setParamValue("fmRM1",rM);
    }
    
    if( (dumR+dumG+dumB)==255 ){
            if(dumR==255){
                pitchSet[0]=0;
                pitchSet[1]=2;
                pitchSet[2]=5;
                pitchSet[3]=10;
                pitchSet[4]=12;
                pitchSet[5]=5;
                pitchSet[6]=14;
                pitchSet[7]=10;
            } else if (dumG==255) {
                pitchSet[0]=0;
                pitchSet[1]=4;
                pitchSet[2]=5;
                pitchSet[3]=9;
                pitchSet[4]=5;
                pitchSet[5]=12;
                pitchSet[6]=14;
                pitchSet[7]=7;
            } else {
                pitchSet[0]=2;
                pitchSet[1]=5;
                pitchSet[2]=9;
                pitchSet[3]=12;
                pitchSet[4]=5;
                pitchSet[5]=9;
                pitchSet[6]=7;
                pitchSet[7]=2;
            }
      } else if( (dumR+dumG+dumB)==510 ){
            if((dumR+dumG)==510){
                pitchSet[0]=2;
                pitchSet[1]=5;
                pitchSet[2]=7;
                pitchSet[3]=11;
                pitchSet[4]=14;
                pitchSet[5]=16;
                pitchSet[6]=17;
                pitchSet[7]=19;
            } else if((dumR+dumB)==510) {
                pitchSet[0]=0;
                pitchSet[1]=4;
                pitchSet[2]=5;
                pitchSet[3]=8;
                pitchSet[4]=-5;
                pitchSet[5]=-7;
                pitchSet[6]=-5;
                pitchSet[7]=-2;
            } else {
                pitchSet[0]=4;
                pitchSet[1]=7;
                pitchSet[2]=9;
                pitchSet[3]=11;
                pitchSet[0]=12;
                pitchSet[1]=14;
                pitchSet[2]=16;
                pitchSet[3]=-12;
            }
        }
  }
        strip.setPixelColor(8, strip.Color(0, 0, 0));
        strip.setPixelColor(9, strip.Color(0, 0, 0));
        strip.setPixelColor(20, strip.Color(0, 0, 0));
        strip.setPixelColor(21, strip.Color(0, 0, 0));


        pitIter = (pitIter+1)%64;
        synth.setParamValue("adriftVol", map(brgt, 0, 1, adriftVol, 0.1));
        synth.setParamValue("fmDp1", map(brgt,0,1,0,3));
        synth.setParamValue("fmMm1", map(brgt,0,1,0.666,1));
        synth.setParamValue("fmVerb", 1);
        synth.setParamValue("fmEucNo1", fmEuclid[0]);
        synth.setParamValue("fmEucNo2", fmEuclid[1]);
        synth.setParamValue("fmFreq1", 68+oct1+pitchSet[pitIter/8]);
        synth.setParamValue("fmFreq2", 68+oct2+pitchSet[pitIter/8]);
        synth.setParamValue("fmVol", fmVol*brgt*0.5);
        synth.setParamValue("fmGateT1", brgt>0);
        synth.setParamValue("kdGateT2", 1);
        synth.setParamValue("kdNxtFreq2", 20+oct2+pitchSet[pitIter/8]);
        synth.setParamValue("kdEucNo2", 1); 
        synth.setParamValue("kdA2", 4); 
        synth.setParamValue("kdR2", 3); 
        synth.setParamValue("kdGateT1", 1);
        synth.setParamValue("kd2vol", bassVol*map(brgt, 0,1,0.25,1)*fadeInCruise);
        synth.setParamValue("kd1vol", kickVol*map(brgt, 0,1,0.1,1)*fadeInCruise); 
}


void _softRestart() 
{
  Serial.end();  //clears the serial monitor  if used
  SCB_AIRCR = 0x05FA0004;  //write value for restart
}

void resetGame(){
  for(int i=0; i< 1000; i++){
    float fadeOutVol;
    fadeOutVol = 1 - (i*0.001);
    synth.setParamValue("masterVol", fadeOutVol);
    delay(5); 
  }
  
  for(int i=0; i< strip.numPixels(); i++) {
      strip.setPixelColor(i, 0,0,0);
  }
  strip.show();
   _softRestart() ;
}


void cruising(){  
  cruisingRainbow();
  adrift();
  collisions2();
  strip.show();
  }
//__________________________________________________________________NEOPIXEL FUNCTIONS

void setIntro(){
    for(int i=0; i< strip.numPixels(); i++) {
      strip.setPixelColor(i, strip.Color(200,100,100));
    }
    strip.setBrightness(30);
    strip.show();
  }


// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if(WheelPos < 85) {
    red = (255 - WheelPos * 3)*0.6;
    green = 0;
    blue = (WheelPos * 3)*0.6;
    return(0);
  }
  if(WheelPos < 170) {
    WheelPos -= 85;
    red = 0;
    green = (WheelPos * 3)*0.6;
    blue = (255 - WheelPos * 3)*0.6;
    return(0);
  }
    WheelPos -= 170;
    red = (WheelPos * 3)*0.6;
    green = (255 - WheelPos * 3)*0.6;
    blue = 0;
    return(0);
}
