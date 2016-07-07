/*

/////////////////////////////////// THIS IS CUBE C ///////////////////////////////////////////

V 2.1.4:   - FFT dependent on timer1
           - Illumination
           - Buttons debouncing!
           - Spacebrew connection for buttons & microphone
             - only works with spacbrew nano library
           - LESS FLOATING POINT CALCULATIONS
               - Also for intensities!!!!
           - Extended neopixel class for fading the cubes
             - one CubeArea element for every surface N,W,S,E;T
           - Two step shutdown of the cube
           - logging messages
           - send out number of pixels instead of meanIntensity derived by meanintensity
           - attempt to get a more stable connection
             - activate monitor();
             - check connection before send();
             - try to connect after switching on;
           
           

Lib: FFT by guest openmusiclabs.com 7.7.14
      and interrupt off ffft piccolo example

LED Hues from http://colorizer.org
      A: 81
      B: 167
      C: 304

LED Numbering:  0-44;
                0-8   -> Area North   (Index: 0, Pin: 9)
                9-17  -> Area West    (Index: 1, Pin: 10) 
                18-26 -> Area South   (Index: 2, Pin: 11)
                27-35 -> Area East    (Index: 3, Pin: 12)
                36-44 -> Area Top     (Index: 4, Pin: 13)
                
                LED Number on CubeArea = LED Number mod 9
*/

#define ADC_CHANNEL 7          // microphone input channel
#define LIN_OUT 1              // use the lin output function
#define FFT_N 64               // set to 64 point ff     

//Libraries
#include <FFT.h>
#include <Adafruit_NeoPixel.h>
#include <CubeArea.h>
#include <Bridge.h>
#include <SpacebrewYun.h>


/**********************************************************************************************
***************************** SETUP STUFF
********************************************************************/

//cube globals
char THISCUBE = 'C';                           // Cube ID
const unsigned int hueA = 81;              // Hue for cube A
const unsigned int hueB = 167;             // Hue for cube B
const unsigned int hueC = 304;             // Hue for cube C
int thisHue = 0;

//button globals
#define NUMBUTTONS 9           // Number of buttons per cube
byte buttons[] = {A4, A5, 2, 3, 4, 5, 6, 7, 8};
boolean buttonStates[NUMBUTTONS];
boolean lastButtonStates[] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
long lastDebounceTime = 0;
byte debounceDelay = 50;

boolean allowSensing;
boolean allowDisplay;
byte switchedOnFlag;

// Audio sampling globals
volatile byte samplePos = 0;
volatile byte sampleFlag;
int SAMPLE_RATE_HZ = 9615;             // Sample rate of the audio in hertz.
int SPECTRUM_MIN_DB = 24;              // Audio intensity (in decibels) that maps to low LED brightness. between 30-40
int SPECTRUM_MAX_DB = 60;              // Audio intensity (in decibels) that maps to high LED brightness.
byte intensityOffset = 10;
int frequencyWindow[] = {300, 675, 1050, 1425, 1800, 2175, 2550, 2925, 3300, 3675, 4050, 4425, 4800};
byte numPixelsToSend = 0;
byte b4Intensity = 0;

byte peakFlag;
byte sendFlag;

// neopixel globals
const byte PIXELS_PER_MIC = 12;         // Number of neo pixels per cube (12 pixels for each cube)
//Pixelnumbers assigned to each cube; will be shown in different colors
byte PIXNUMS_A[] =   {1,  17,   22,    33,    5,   13,   25,   29,   8,   15,   23,   32};
byte PIXNUMS_B[] =   {12,  24,  30,   4,   9,    26,   34,   6,    10,    19,   27,    3};
byte PIXNUMS_C[] =   {28,  7,   11,   18,   35,   0,    16,   21,   31,   2,    14,   20};
byte thisPixnums[PIXELS_PER_MIC];

CubeArea areas[] = {   CubeArea(9, 9, NEO_GRB + NEO_KHZ800),    //init with pin 9 - pin ist set bei CubeAreaClass
                       CubeArea(9, 10, NEO_GRB + NEO_KHZ800),
                       CubeArea(9, 11, NEO_GRB + NEO_KHZ800),
                       CubeArea(9, 12, NEO_GRB + NEO_KHZ800),
                       CubeArea(9, 13, NEO_GRB + NEO_KHZ800)
                     };

// Spacebrew globals
SpacebrewYun sb = SpacebrewYun(String(THISCUBE));
//String server = "pure-headland-6826.herokuapp.com:80";
//String server = "192.168.0.101:9000";
//String server = "sanbox.spacebrew.cc";

// timing globals
long previousMillis = 0;  

void setup() {  
  b4Intensity = 10;
  areas[4].setIntensity(b4Intensity, 4, 35);      //set led to orange to indicate loading & connecting
  
  allowSensing = true;
  allowDisplay = true;
  
  switchedOnFlag = 0;
  
  //init buttons
  for(byte i = 0; i<NUMBUTTONS; i++) {
    pinMode(buttons[i], INPUT);
  }

  //Setup timers and interrupts
  cli();      //disable global interrupts
  TCCR1A = 0;    // set entire TCCR1A register to 0
  TCCR1B = 0;    // set entire TCCR1A register to 0

  /* Timer Calculation:
  *  timer res = 1/(16*10^6)/256  // 256 = prescaler; 16*10^=16MHz arduino clock frequency
  *             = 0.000016
  *  target time = 1/3 sec
  *  timer count + 1 = (target time) / (timer res)
  *                  = (1/3) / 0.000016
  *                  = 20833.33333
  *  timer count     = 20832      // The count compared to the timer and invokes interrupt!
  */
  OCR1A = 20832;
  TCCR1B |= (1 << WGM12);           // Turn on CTC-Mode: Clear Timer on compare match
  TCCR1B |= (1 << CS12);            // Sets bit CS12 in TCCR1B -> prescaler 256: ((1/16Mhz)/256)*65535 = 1.04... sec (65535 max num of timer1)
  // see for info: https://arduinodiy.wordpress.com/2012/02/28/timer-interrupts/
  TIMSK1 |= (1 << OCIE1A);          // Enable Timer compare interrupt

  // Setup the Analog-Digital-Converter; f = ( 16MHz/prescaler ) / 13 cycles/conversion; started in free-run mode when timer1 overflows
  // See for info:   http://modelleisenbahn-steuern.de/controller/atmega8/18-7-adcsra-atmega8.htm
  //                 http://www.glennsweeney.com/tutorials/interrupt-driven-analog-conversion-with-an-atmega328p
  //                 http://www.avrfreaks.net/forum/does-adate-means-free-running-adc-atmega32
  ADMUX  = ADC_CHANNEL;                             // Channel sel, right-adj, use AREF pin
  ADCSRA =  _BV(ADEN)  |                            // Set ADEN in ADCSRA (0x7A) to enable the ADC.
            //          _BV(ADSC)  |                            // Start the ADC
            //          _BV(ADATE) |                            // Set Auto-Trigger to trigger die ADC-interrupt on timer1
            _BV(ADIE)  |                            // Enable the interrupt, that it will be triggered
            _BV(ADPS2) | _BV(ADPS1) | _BV(ADPS0);   // Set the Prescaler to 128; 128:1 / 13 = 9615 Hz
  //ADCSRB =  _BV(ADTS2) | _BV(ADTS1);                // Set the Trigger in ADCSRB to timer1 overflow
  DIDR0  =  1 << ADC_CHANNEL;                       // Turn off digital input for ADC pin

  sei();                            // enable global interrupts

  sampleFlag = 0;
  peakFlag = 0;
  sendFlag = 0;
  
  
  //Setup Spacebrew
  Bridge.begin(); 
  b4Intensity = 40;
  areas[4].setIntensity(b4Intensity, 4, 35);
  //sb.addPublish("Mout", "string");  // Mic out
  //sb.addSubscribe("Min", "string"); // Mic in
  sb.onStringMessage(handleCloudInput); 
  sb.connect();
  delay(1000); //give the client a little time to connect
  
  setupThisCube();

} 

void setupThisCube() {
    
  switch (THISCUBE) {
    case 'A':
      thisHue = hueA;
      for(byte i=0; i<sizeof(PIXNUMS_A); i++) {
        thisPixnums[i] = PIXNUMS_A[i];
      }
      break;
      
    case 'B':
      thisHue = hueB;
      for(byte i=0; i<sizeof(PIXNUMS_B); i++) {
        thisPixnums[i] = PIXNUMS_B[i];
      }
      break;
    case 'C':
      thisHue = hueC;
      for(byte i=0; i<sizeof(PIXNUMS_C); i++) {
        thisPixnums[i] = PIXNUMS_C[i];
      }
      break;
  }
  b4Intensity = 80;
  areas[4].setIntensity(b4Intensity, 4, 35);

}

/**********************************************************************************************
***************************** LOOP
********************************************************************/

void loop() {
  
  listenToButtons();
  
  sb.monitor();
  //areas[4].setIntensity(b4Intensity, 4, 270);
 
  //listenToButtons();
  
  if (sampleFlag == 1)  {   // if a new sample is available do fft on it & get the intensities!
    updatefft();
    updateIntensities();
  }

  if (peakFlag == 1) {      // if a intensity peak occured send out data & display change
    peakFlag = 0;
  }
 
  
  unsigned long currentMillis = millis();
  if((currentMillis - previousMillis) >= 1000) {
    
    if(!sb.connected() && allowDisplay) {    //not connected
       reconnect();
    } else if (sb.connected() && allowDisplay) {
      areas[4].setIntensity(b4Intensity, 4, thisHue);
      
      //Only send things out if theres a connection
      
      //send out RE for logging 
      if(switchedOnFlag == 1) {
        sb.send("RE: " + String(THISCUBE));
        switchedOnFlag = 0;
      }
      
      //Send out mean Intensity to other cubes
      if(sendFlag == 1) {
        
        String moutmsg = 'M'+String(THISCUBE)+String(numPixelsToSend); 
        sb.send(moutmsg);    //Send out the peak
      }
          
    }
    
    previousMillis = currentMillis;
    
    sendFlag = 0;
    
  }
  
    areas[0].update();
    areas[1].update();
    areas[2].update();
    areas[3].update();

}

/**********************************************************************************************
***************************** AUDIO SAMPLING & FFT & INTENSITIES
********************************************************************/

// Interrupt is called when timer1 counter overflows
ISR(TIMER1_COMPA_vect) {

  if(allowSensing) { //if sensing is allowed
    cli();
    ADCSRA |= _BV(ADSC) | _BV(ADATE); //start the adc; set auto trigger!
  }
}

// Collects audio data
ISR(ADC_vect) { // Audio-sampling interrupt

  byte m = ADCL; // fetch adc data
  byte j = ADCH;

  int k = (j << 8) | m; // form into an int;;;; Um 8 bit nach links verschieben und dann bitweises OR
  k -= 0x0200; // form into a signed int 0x0200=512 hex
  k <<= 6; // form into a 16b signed int

  fft_input[samplePos] = k; // put real data into even bins
  fft_input[samplePos + 1] = 0; // set odd bins to 0
  if (++samplePos >= FFT_N) {
    ADCSRA &= ~(_BV(ADIE) | _BV(ADATE));//Buffer full, disable the interrupt; set auto trigger off
    sampleFlag = 1;
    sei();
  }

}

//Performs Fourier-Transform on Audio-Sampling Data
void updatefft() {
  sampleFlag = 0;
  samplePos = 0;                   // Reset sample counter
  ADCSRA |= _BV(ADIE);             // Resume sampling interrupt; enable the interrupt
  fft_window(); // window the data for better frequency response
  fft_reorder(); // reorder the data before doing the fft
  fft_run(); // process the data in the fft
  fft_mag_lin(); // take the output of the fft
}

//Transforms the spectrum-data to intensities for each of the 12 leds -> see also frequencyWindow
void updateIntensities() {
  int intensity, otherMean;

  //Equalizer for frequency Bin #2, #3 because the sensed magnitudes are a little bit too high.
  byte equalizer[] = {100, 50, 50, 100, 100, 100, 100, 100, 100, 100, 100, 100};
  byte highestIntensity = 0;
  
  for (byte i = 0; i < PIXELS_PER_MIC; ++i) {

    windowMean(fft_lin_out, frequencyToBin(frequencyWindow[i]), frequencyToBin(frequencyWindow[i + 1]), &intensity, &otherMean);
    // Convert intensity to decibels.
    intensity = 20.0 * log10(intensity);
    // Scale the intensity and clamp between 0 and 100.
    intensity -= SPECTRUM_MIN_DB;
    intensity = intensity < 0 ? 0 : intensity;
    intensity *= 100;
    intensity /= (SPECTRUM_MAX_DB - SPECTRUM_MIN_DB);
    intensity = intensity > 100 ? 100 : intensity;
    intensity = (intensity * equalizer[i]) / 100;
    
    byte areaIndex = getAreaIndex(thisPixnums[i]);
    byte areaPix = getPixelNumber(thisPixnums[i]);
    byte endIntensity = areas[areaIndex].endIntensities[areaPix];
    
    //Check for peak in intensity: at least for the amount of offset higher or lower then actual intensity
    if (intensity > (endIntensity + intensityOffset) || intensity < (endIntensity - intensityOffset)) {
      
      if(intensity < intensityOffset) intensity = 0;
      areas[areaIndex].setIntensityPeak(intensity, areaPix, thisHue);
      
      if(intensity > highestIntensity) highestIntensity = intensity;
      
      if (peakFlag == 0) peakFlag = 1;               //Only set the peak flag if it hasn't been set before
      if (sendFlag == 0) sendFlag = 1;
    }
  }
  
  numPixelsToSend = (PIXELS_PER_MIC*highestIntensity) / 100;
  
}

/**********************************************************************************************
***************************** HELPER FUNCTIONS
********************************************************************/

// Compute the average magnitude of a target frequency window vs. all other frequencies.
void windowMean(uint16_t* spectrum, int lowBin, int highBin, int* windowMean, int* otherMean) {

  *windowMean = 0;
  *otherMean = 0;
  // Notice the first magnitude bin is skipped because it represents the
  // average power of the signal.
  for (int i = 1; i < FFT_N / 2; ++i) {

    if (i >= lowBin && i <= highBin) {
      *windowMean += spectrum[i];
    }
    else {
      *otherMean += spectrum[i];
    }
  }
  *windowMean /= (highBin - lowBin) + 1;
  *otherMean /= (FFT_N / 2 - (highBin - lowBin));
}


// Convert a frequency to the  FFT bin it will fall within.
// the given frequency gets to bin #
int frequencyToBin(int frequency) {
  int binFrequency = SAMPLE_RATE_HZ / FFT_N;
  return (frequency / binFrequency);
}

uint8_t getAreaIndex(byte pixNum) {
   if(pixNum<9) return 0;             // North: 0-8; pin 9
   else if(pixNum<18) return 1;        // West: 9-17; pin 10
   else if(pixNum<27) return 2;        // South: 18-26; pin 11
   else if(pixNum<36) return 3;        // East: 27-35; pin 12
   else if(pixNum>=36) return 4;       // Top: 36-44; pin 13
}
    
uint16_t getPixelNumber(byte pixNum) {
  return pixNum % 9;
}
/**********************************************************************************************
***************************** BUTTON FUNCTIONS
********************************************************************/

void listenToButtons() {
  
  for(byte i=0; i<NUMBUTTONS; i++) {
    
    int reading = digitalRead(buttons[i]);
    
    
    if(reading != lastButtonStates[i]) {
       lastDebounceTime = millis();
    }
    
    // state has changed
    if ((millis() - lastDebounceTime) > debounceDelay) {
      if(reading != buttonStates[i]) {
        
        buttonStates[i] = reading;
        
        //only toggle LED if state is HIGH
        if(buttonStates[i] == HIGH) {
          
          if(i == 4) {
            switchOnOff();
          }
          
          if(allowDisplay && i!=4) {
          
            //get the color of the LED
            int pixHue = areas[4].getHue(i);
            String buttonMsg;
  
            if(pixHue != thisHue) {
              areas[4].setIntensity(100, i, thisHue);
              buttonMsg = 'B'+String(THISCUBE)+String(i)+'1';
            } else {
              areas[4].setIntensity(0, i, 0);
              buttonMsg = 'B'+String(THISCUBE)+String(i)+'0';
            }
            
            if(sb.connected()) sb.send(buttonMsg);
            else reconnect();

          } //allowdisplay
        }
      }
    }//debounce
 
    lastButtonStates[i] = reading;
  }
  
}

void switchOnOff() {

  //several cases that can be
  if(allowSensing && allowDisplay) { //alowSensing AND allowDisplay is true
    allowSensing = false;
    
    if(sb.connected()) sb.send("Idle: "+String(THISCUBE));
    else reconnect();

    //turn off own pixels 
    for(byte i=0; i<PIXELS_PER_MIC; i++) {
        setPixelIntensity(thisPixnums[i], thisHue, 0);
    }
    
    //Tell other cubes to turn off their pixels
    numPixelsToSend = 0;
    sendFlag = 1;
    b4Intensity = 20;  //lower the intensity of the button to indicate the mode
    areas[4].setIntensity(b4Intensity, 4, thisHue);
    

  } else if(!allowSensing && allowDisplay) {    //Sensing already turned off, displaying still allowed
    allowDisplay = false;
    if(sb.connected()) sb.send('O'+String(THISCUBE));
    else reconnect();
    
    //Turn off buttons
    for(byte i=0; i<9; i++) {
       int hue = areas[4].getHue(i);
       areas[4].setIntensity(0, i, hue);
    }
    
    //turn off the light
    for(byte i=0; i<4; i++) {
      for(byte j=0; j<9; j++) {
        int hue = areas[i].getHue(j);
        areas[i].setIntensityPeak(0, j, hue);
      }
    }
    b4Intensity = 0; //lower the intensity of the button to indicate the mode
    areas[4].setIntensity(b4Intensity, 4, thisHue);
   
  } else if(!allowSensing && !allowDisplay) {  //Sensing and displaying disabled -> turn them on
    if(!sb.connected()) reconnect();
    
    b4Intensity = 80;
    areas[4].setIntensity(b4Intensity, 4, thisHue);
    switchedOnFlag = 1;
    allowSensing = true;
    allowDisplay = true;   
  }
  
}

/**********************************************************************************************
***************************** SPACEBREW FUNCTIONS
********************************************************************/

void handleCloudInput (String route, String value) {
  char action = value.charAt(0);
  value.remove(0,1);
  if (action == 'B' && allowDisplay) showBin(value);
  if (action == 'M' && allowDisplay) showMin(value);
  if (action == 'O') turnOffRemote(value);
  
}

void reconnect() {
   areas[4].setIntensity(b4Intensity, 4, 0);    //turn led red
   sb.connect(); //check connection
   delay(1000); //give it some time to connect
}

void showBin(String value) {
   // Value looks like: A40; where "A" is the cube identifiere, "4" the number of the button and "0" the mode (0=off, 1=on);
  
  char cube = value.charAt(0);
  int button = value.charAt(1) - '0';
  char mode = value.charAt(2);
  
  switch(cube) {
    case 'A':
        if (mode == '1') areas[4].setIntensity(100, button, hueA);
        else if(mode == '0') areas[4].setIntensity(0, button, 0);
        break;
    case 'B':
        if (mode == '1') areas[4].setIntensity(100, button, hueB);
        else if(mode == '0') areas[4].setIntensity(0, button, 0);
        break;
    case 'C':
        if (mode == '1') areas[4].setIntensity(100, button, hueC);
        else if(mode == '0') areas[4].setIntensity(0, button, 0);
        break;
  }
}

void showMin(String msg) {

  char cube = msg.charAt(0);    // Get the cube identifier
  msg.remove(0,1);              // remove the leading cube identifier and the first separator
  byte pixelsToDisplay = msg.toInt();
  byte intensity = (pixelsToDisplay*100) / PIXELS_PER_MIC;
 
  switch(cube) {
    case 'A':
        for(byte i=0; i<PIXELS_PER_MIC; i++) {
          if(i<=pixelsToDisplay) setPixelIntensity(PIXNUMS_A[i], hueA, intensity);
          else setPixelIntensity(PIXNUMS_A[i], hueA, 0);
        }
        break;
   
    case 'B':
        for(byte i=0; i<PIXELS_PER_MIC; i++) {
          if(i<=pixelsToDisplay) setPixelIntensity(PIXNUMS_B[i], hueB, intensity);
          else setPixelIntensity(PIXNUMS_B[i], hueB, 0);
        }
        break;
    
    case 'C':
        for(byte i=0; i<PIXELS_PER_MIC; i++) {
          if(i<=pixelsToDisplay) setPixelIntensity(PIXNUMS_C[i], hueC, intensity);
          else setPixelIntensity(PIXNUMS_C[i], hueC, 0);
        }
        break;
  }
}

void setPixelIntensity(byte pixNum, int hue, byte intensity) {
      byte areaIndex = getAreaIndex(pixNum);
      byte areaPix = getPixelNumber(pixNum);
      areas[areaIndex].setIntensityPeak(intensity, areaPix, hue);
}

void turnOffRemote(String msg) {
   char cube = msg.charAt(0);    // Get the cube identifier
   switch(cube) {
     case 'A':
         //Turn off buttons
         for(byte i=0; i<9; i++) {
           if(areas[4].getHue(i) == hueA) areas[4].setIntensity(0, i, hueA);
         }
        break;
   
    case 'B':
         //Turn off buttons
         for(byte i=0; i<9; i++) {
           if(areas[4].getHue(i) == hueB) areas[4].setIntensity(0, i, hueB);
         }
       
        break;
    
    case 'C':
         //Turn off buttons
         for(byte i=0; i<9; i++) {
           if(areas[4].getHue(i) == hueC) areas[4].setIntensity(0, i, hueC);
         }
        
        break;
  }
}
