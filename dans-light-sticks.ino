#include <FastLED.h>

#include <ESPAsyncE131.h>
#include <ESP8266WiFiMulti.h>
#include <WiFiClient.h>
#include <WiFiServer.h>
#include <WiFiUdp.h>
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino

// needed for wifimanager
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager

#include <U8x8lib.h>
#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif

// TODO: Fix the bug where the oled stops outputting if showLeds() is called
//        - suspect the problem is FastLED messing with timing

FASTLED_USING_NAMESPACE

#define DATA_PIN    D2
// #define CLK_PIN   4
// #define LED_TYPE    WS2811
#define LED_TYPE    TM1804
#define COLOR_ORDER RGB
#define NUM_LEDS    8
CRGB leds[NUM_LEDS];

#define INITIAL_BRIGHTNESS          96
#define FRAMES_PER_SECOND  120
#define UNIVERSE_COUNT       1


// Based on the FastLED "100-lines-of-code" demo reel, showing just a few 
// of the kinds of animation patterns you can quickly and easily 
// compose using FastLED.  
//
// This example also shows one easy way to define multiple 
// animations patterns and have them automatically rotate.
//
// -Mark Kriegsman, December 2014


U8X8_SSD1306_128X32_UNIVISION_SW_I2C u8x8(/* clock=*/ 14, /* data=*/ 2, /* reset=*/ 4);

/**
 * Fast LED Defs
 */
#if defined(FASTLED_VERSION) && (FASTLED_VERSION < 3001000)
#warning "Requires FastLED 3.1 or later; check github for latest code."
#endif

#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))


ESPAsyncE131 e131(UNIVERSE_COUNT);

#define ARTNET_UNIVERSE 4
#define ARTNET_ADDRESS 1

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  u8x8.begin();
  //u8x8.setPowerSave(0);
  u8x8.setFont(u8x8_font_chroma48medium8_r);
  u8x8.drawString(0,0,"Connecting...");
  
  // WiFiManager
  WiFiManager wifiManager;
  // reset settings - for testing
  //wifiManager.resetSettings();

  // Sets timeout (seconds) until configuration portal gets turned off
  // useful to make it all retry or go to sleep
  wifiManager.setTimeout(30);
  
  // wifiManager.autoConnect() fetches ssid and password and tries to connect
  // to the last access point. 
  // If it does not connect it starts an access point with the specified name 
  // (here "AutoConnectAP") and goes into a blocking loop awaiting configuration
  if(!wifiManager.autoConnect("AutoConnectAP")) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  } 

  // If we here you have connected to the WiFi
  Serial.println("connected...:)");
  u8x8.drawString(0,0,WiFi.SSID().c_str());
  u8x8.drawString(0,1,WiFi.localIP().toString().c_str());
  
 
   
  // tell FastLED about the LED strip configuration
  FastLED.addLeds<LED_TYPE,DATA_PIN,COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);

  // set master brightness control
  FastLED.setBrightness(INITIAL_BRIGHTNESS);
  
  // Listen to e1.31
  if (!e131.begin(E131_MULTICAST, ARTNET_UNIVERSE, UNIVERSE_COUNT)) {
    u8x8.drawString(0,2,"e1.31 failed");
  }  
  
}


// List of patterns to cycle through.  Each is defined as a separate function below.
typedef void (*SimplePatternList[])();
SimplePatternList gPatterns = { lightning, rainbow, rainbowWithGlitter, confetti, sinelon, juggle, bpm };
uint8_t numPatterns = ARRAY_SIZE(gPatterns);

uint8_t gCurrentPatternNumber = 0; // Index number of which pattern is current
uint8_t gHue = 0; // rotating "base color" used by many of the patterns
  
void loop()
{
  // Setup The LEDs  
  showLeds();
  parseE131();
  //parseArtNet(); 
}

bool packet_toggle = false;
void toggleGotPacket() {
  packet_toggle = !packet_toggle;
  char val = ' ';
  if (packet_toggle) {
    val = '.';
  }
  u8x8.drawString(0,2,(char*)val);
  
}

void parseE131() {
  if (!e131.isEmpty()) {
    e131_packet_t packet;
    e131.pull(&packet);     // Pull packet from ring buffer
    
    //toggleGotPacket();
    
    if (htons(packet.universe) == ARTNET_UNIVERSE) {
      uint8_t brightness = packet.property_values[ARTNET_ADDRESS];
      uint8_t patternDmx = packet.property_values[ARTNET_ADDRESS+1];
      uint8_t patternIdx = scale8(patternDmx, numPatterns);

      if (gCurrentPatternNumber != patternDmx) {
        u8x8.drawString(0,2,"Pattern: ");
        u8x8.drawString(9,2, String(patternIdx).c_str());
      }
            
      // set LED state
      FastLED.setBrightness(scale8(brightness, 100));
      gCurrentPatternNumber = patternIdx;
    }
        
//        Serial.printf("Universe %u / %u Channels | Packet#: %u / Errors: %u / CH1: %u\n",
//                htons(packet.universe),                 // The Universe for this packet
//               htons(packet.property_value_count) - 1, // Start code is ignored, we're interested in dimmer data
//                e131.stats.num_packets,                 // Packet counter
//                e131.stats.packet_errors,               // Packet error counter
//                packet.property_values[1]);             // Dimmer data for Channel 1
  }
}


void showLeds() {
  // Call the current pattern function once, updating the 'leds' array
  gPatterns[gCurrentPatternNumber]();

  // send the 'leds' array out to the actual LED strip
  FastLED.show();  
  // insert a delay to keep the framerate modest
  FastLED.delay(1000/FRAMES_PER_SECOND); 

  // do some periodic updates
  EVERY_N_MILLISECONDS( 20 ) { gHue++; } // slowly cycle the "base color" through the rainbow
}

void nextPattern()
{
  // add one to the current pattern number, and wrap around at the end
  gCurrentPatternNumber = (gCurrentPatternNumber + 1) % numPatterns;
}

void rainbow() 
{      
  // FastLED's built-in rainbow generator
  fill_rainbow( leds, NUM_LEDS, gHue, 7);
}

void rainbowWithGlitter() 
{
  // built-in FastLED rainbow, plus some random sparkly glitter
  rainbow();
  addGlitter(80);
}

void addGlitter( fract8 chanceOfGlitter) 
{
  if( random8() < chanceOfGlitter) {
    leds[ random16(NUM_LEDS) ] += CRGB::White;
  }
}

void confetti() 
{
  // random colored speckles that blink in and fade smoothly
  fadeToBlackBy( leds, NUM_LEDS, 10);
  int pos = random16(NUM_LEDS);
  leds[pos] += CHSV( gHue + random8(64), 200, 255);
}

void sinelon()
{
  // a colored dot sweeping back and forth, with fading trails
  fadeToBlackBy( leds, NUM_LEDS, 20);
  int pos = beatsin16( 13, 0, NUM_LEDS-1 );
  leds[pos] += CHSV( gHue, 255, 192);
}

void bpm()
{
  // colored stripes pulsing at a defined Beats-Per-Minute (BPM)
  uint8_t BeatsPerMinute = 80;
  CRGBPalette16 palette = PartyColors_p;
  uint8_t beat = beatsin8( BeatsPerMinute, 64, 255);
  for( int i = 0; i < NUM_LEDS; i++) { //9948
    leds[i] = ColorFromPalette(palette, gHue+(i*2), beat-gHue+(i*10));      
  }
}

void juggle() {
  // eight colored dots, weaving in and out of sync with each other
  fadeToBlackBy( leds, NUM_LEDS, 20);
  byte dothue = 0;
  for( int i = 0; i < 8; i++) {
    leds[beatsin16( i+7, 0, NUM_LEDS-1 )] |= CHSV(dothue, 200, 255);
    dothue += 32;
  }
}

void lightning() {
  fill_solid( leds, NUM_LEDS, CRGB(0, 0, 0));
  addGlitter(80);
  
}

