/**********************************************************************************************
 ***************************** CLASS CUBEAREA
 ********************************************************************/

#include <Adafruit_NeoPixel.h>


class CubeArea : public Adafruit_NeoPixel {
    
public:
    
    byte intensities[9]    = {0,0,0,0,0,0,0,0,0};
    byte endIntensities[9] = {0,0,0,0,0,0,0,0,0};
    int pixHues[9] = {0,0,0,0,0,0,0,0,0};
    
    byte fadingFlag = 0;
    unsigned long lastUpdate = 0;
    byte interval = 20;
    
    
    CubeArea(uint16_t pixels, uint8_t pin, uint8_t type) :Adafruit_NeoPixel(pixels, pin, type) {
        begin();
    }
    
    void setIntensityPeak(byte intensityPeak, byte pixPos, int hue) {
        endIntensities[pixPos] = intensityPeak;
        pixHues[pixPos] = hue;
        fadingFlag = 1;
    }
    
    void setIntensity(byte intensity, byte pixPos, int hue) {
        pixHues[pixPos] = hue;
        setPixelColor(pixPos, pixelHSVtoRGBColor(intensity, hue));
        show();
    }
    
    int getHue(byte pixPos) {
        return pixHues[pixPos];
    }
    
    
    void update() {
        
        if((millis() - lastUpdate >= interval) && (fadingFlag == 1)) {          //fade depentent on timer//
            updateFade();
            lastUpdate = millis();
        }
        
    }
    
private:
    void updateFade() {
        
        
        //durch clear leuchtet immer nur 1 pixel
        fadingFlag=0;                                          //Reset fadingFlag
        //Serial.println("--------");
        for(byte i=0; i<sizeof(intensities); i++) {    //fade all pixels on area
            
            if(intensities[i] < endIntensities[i]) {     // Fade up
                intensities[i]++;

            }
            
            if(intensities[i] > endIntensities[i]) {
                intensities[i]--;
            }
            
            setPixelColor(i, pixelHSVtoRGBColor(intensities[i], pixHues[i]));
            show();
            
            if(intensities[i] != endIntensities[i]) fadingFlag = 1; //if only one pixel hasn't finished - set flag to 1;
            
        }
        
    }
    
    uint32_t pixelHSVtoRGBColor(byte intensity, int hue) {
        // Implemented from algorithm at http://en.wikipedia.org/wiki/HSL_and_HSV#From_HSV
        
        // Modified algorithm according to reduce floating point calculations
        
        // INFO: Saturation is always 1
        
       
        
            int h1, mod, x, r, g, b;
        
            h1 = hue/60;
            mod = ((hue*100)/60) % 200;                //fmod(h1, 2.0))
            x = intensity*(100 - abs(mod-100))/100;
        
            if (h1 < 1) {
                r = intensity;
                g = x;
                b = 0;
            } else if (h1 < 2) {
                r = x;
                g = intensity;
                b = 0;
            } else if (h1 < 3) {
                r = 0;
                g = intensity;
                b = x;
            } else if (h1 < 4) {
                r = 0;
                g = x;
                b = intensity;
            } else if (h1 < 5) {
                r = x;
                g = 0;
                b = intensity;
            } else { // h1 <= 6
                r = intensity;
                g = 0;
                b = x;
            }
        
            return Color((255*r)/100, (255*g)/100, (255*b)/100);
       
        
    }
    
};