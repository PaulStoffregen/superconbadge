/*
 * Copyright (c) 2015 Nathaniel Quillin
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */


#include <Audio.h>
#include <Bounce.h>
#include <ILI9341_t3.h>
#include <SD.h>
#include <SerialFlash.h>
#include <SPI.h>
#include <Wire.h>


#define TFT_DC      20
#define TFT_CS      21
#define TFT_RST    255  // 255 = unused, connect to 3.3V
#define TFT_MOSI     7
#define TFT_SCLK    14
#define TFT_MISO    12


Bounce button0 = Bounce(0, 15);
Bounce button1 = Bounce(1, 15);  // 15 = 15 ms debounce time
Bounce button2 = Bounce(2, 15);

ILI9341_t3 tft = ILI9341_t3(TFT_CS, TFT_DC, TFT_RST, TFT_MOSI, TFT_SCLK, TFT_MISO);

AudioSynthWaveformSine   sine1;
AudioInputI2S            i2s2;
AudioMixer4              mixer1;
AudioOutputI2S           i2s1;
AudioAnalyzeFFT1024      fft1024_1;
AudioAnalyzePeak         peak1;
AudioConnection          patchCord2(sine1, 0, mixer1, 2);
AudioConnection          patchCord3(i2s2, 0, mixer1, 3);
AudioConnection          patchCord5(mixer1, 0, i2s1, 0);
AudioConnection          patchCord6(mixer1, 0, i2s1, 1);
AudioConnection          patchCord7(mixer1, fft1024_1);
AudioConnection          patchCord9(mixer1, peak1);
AudioControlSGTL5000     sgtl5000_1;


static int count = 0;
static uint16_t line_buffer[320];
static float scale = 10.0;
static uint16_t waterfall[160][120];
static int knob = 0;
static int vol = 0;
static int filenumber = 0;
const char * filelist[4] = {
    "SDTEST1.WAV", "SDTEST2.WAV", "SDTEST3.WAV", "SDTEST4.WAV"
};

const char * images[3] = {
    "HACK1.RAW", "HACK2.RAW", "HACK3.RAW"
};
static int image_idx = 2;

enum Mode {
    MODE_EXT_FFT = 0,
    MODE_INT_FFT,
    MODE_EXT_PEAK,
    MODE_INT_PEAK,
    MODE_IMAGES,
    MODE_BADGE,
    MODE_SIZE,
};

static Mode mode;// = Mode.MODE_EXT_FFT;

uint16_t colorMap(uint16_t val);
void enableExternalAudio(void);
void enableInternalAudio(void);
void enterState(void);
void loadImage(const char* fname);
void updateFft(void);
void updatePeak(void);
void updateState(void);

void setup(void)
{
    // Initialize the peripherals.
    tft.begin();
    Serial.begin(9600);
    Serial.println("what");
    AudioMemory(10);
    sgtl5000_1.enable();
    sgtl5000_1.volume(0.5);
    sgtl5000_1.inputSelect(AUDIO_INPUT_MIC);
    sgtl5000_1.micGain(36);
    SPI.setMOSI(7);
    SPI.setSCK(14);
    if (!(SD.begin(10))) {
        while (1) {
            Serial.println("Error opening SD card.");
            delay(500);
        }
    }

    pinMode(0, INPUT_PULLUP);
    pinMode(1, INPUT_PULLUP);
    pinMode(2, INPUT_PULLUP);

    mixer1.gain(0, 0.0);
    mixer1.gain(1, 0.0);
    mixer1.gain(2, 0.0);
    mixer1.gain(3, 1.0);

    sine1.frequency(440);
    sine1.amplitude(0.75);

    // Uncomment one these to try other window functions
    fft1024_1.windowFunction(NULL);
    // fft1024_1.windowFunction(AudioWindowBartlett1024);
    // fft1024_1.windowFunction(AudioWindowFlattop1024);

    // Display a popular quote.
    tft.fillScreen(ILI9341_BLACK);
    tft.setTextColor(ILI9341_YELLOW);
    tft.setTextSize(2);
    tft.println("A long time ago in a"
                "    galaxy far,     "
                "        far away... ");
    for (int i = 0; i < 320; i++) {
        tft.setScroll(count++);
        count = count % 320;
        delay(10);
    }

    // Display the wave.
    loadImage("WAVE.RAW");
    for (int i = 0; i < 640; i++) {
        tft.setScroll(count++);
        count = count % 320;
        delay(1);
    }

    tft.setRotation(0);
    updateState();
}

void loop() {
    button0.update();
    button1.update();
    button2.update();

    // Initialize state transition.
    if (button0.fallingEdge()) {
        enterState();
    }

    knob = analogRead(A2);
    vol = analogRead(A3);

    updateState();

    // Display Waves
    if (button2.fallingEdge()) {
        loadImage("WAVE.RAW");
        tft.setRotation(2);
        delay(100);
        button2.update();
        while (!button2.fallingEdge()) {
            float temp = analogRead(A2)/1024.0;
            button2.update();
            tft.setScroll(count++);
            count = count % 320;
            delayMicroseconds(temp*10000);
        }
        enterState();
    }
}

uint16_t colorMap(uint16_t val)
{
    float red;
    float green;
    float blue;

    float temp = val / 65536.0 * scale;

    if (temp < 0.5) {
        red = 0.0;
        green = temp * 2;
        blue = 2 * (0.5 - temp);
    } else {
        red = temp;
        green = (1.0 - temp);
        blue = 0.0;
    }

    return tft.color565(red * 256, green * 256, blue * 256);
}

void enableInternalAudio(void)
{
    mixer1.gain(0, 0.0);
    mixer1.gain(1, 0.0);
    mixer1.gain(2, 1.0);
    mixer1.gain(3, 0.0);
}

void enterState(void)
{
    mode = static_cast<Mode>((static_cast<int>(mode) + 1) % MODE_SIZE);
    Serial.printf("Mode: %d\n", static_cast<int>(mode));
    switch (mode) {
    case MODE_EXT_FFT:
        enableExternalAudio();
        tft.setRotation(0);
        break;
    case MODE_INT_FFT:
        scale = 10.0;
        enableInternalAudio();
        tft.setRotation(0);
        break;
    case MODE_EXT_PEAK:
        enableExternalAudio();
        tft.setRotation(0);
        break;
    case MODE_INT_PEAK:
        enableInternalAudio();
        tft.setRotation(0);
        break;
    case MODE_IMAGES:
        tft.setRotation(0);
        tft.setScroll(0);
        loadImage(images[image_idx]);
        break;
    case MODE_BADGE:
        tft.setRotation(0);
        tft.fillScreen(ILI9341_BLACK);
        tft.setTextColor(ILI9341_WHITE);
        tft.setTextSize(5);
        tft.setRotation(3);
        tft.println("Nathaniel "
                    " Quillin  "
                    "          "
                    "HACKADAY! "
                    "#SUPERCON ");
        break;
    }
}

void enableExternalAudio(void)
{
    mixer1.gain(0, 0.0);
    mixer1.gain(1, 0.0);
    mixer1.gain(2, 0.0);
    mixer1.gain(3, 1.0);
}

void loadImage(const char* fname)
{
    Serial.println(fname);
    File image_file = SD.open(fname);
    tft.setScroll(0);

    tft.setRotation(2);
    for (int i = 0; i < 240; i++) {
        image_file.read(&line_buffer, 320 * 2);
        tft.writeRect(i, 0, 1, 320, (uint16_t*) &line_buffer);
    }
    image_file.close();
}

void scrollDisplay(void)
{
    tft.setScroll(count++);
    count = count % 320;
    delay(10);
}

void updateFft(void)
{
    if (fft1024_1.available()) {
        for (int i = 0; i < 240; i++) {
            line_buffer[240 - i - 1] = colorMap(fft1024_1.output[i]);
        }

        tft.writeRect(0, count, 240, 1, (uint16_t*) &line_buffer);
        tft.setScroll(count++);
        count = count % 320;
    }
}

void updatePeak(void)
{
    if (peak1.available()) {
        float val = peak1.read();

        tft.drawFastHLine(0, count, 240, 0);
        tft.drawFastHLine((1 - val) * 120, count, val * 240, 0xFFFF);
        tft.setScroll(count++);
        count = count % 320;
    }
}

void updateState(void)
{
    switch(mode) {
    case MODE_EXT_FFT:
        scale = 1024 - knob;
        sgtl5000_1.micGain((1024-vol) / 4);
        updateFft();
        break;
    case MODE_INT_FFT:
        sine1.frequency((1024 - knob) * 8);
        sine1.amplitude((1024 - vol) / 1280.0);
        updateFft();
        break;
    case MODE_EXT_PEAK:
        sgtl5000_1.micGain((1024 - vol) / 4);
        updatePeak();
        break;
    case MODE_INT_PEAK:
        sine1.frequency((1024 - knob)* 8);
        sine1.amplitude((1024 - vol) / 1280.0);
        updatePeak();
        break;
    case MODE_IMAGES:
        if (button1.fallingEdge()) {
            loadImage(images[image_idx++]);
            image_idx = image_idx % 3;
        }
        scrollDisplay();
        break;
    case MODE_BADGE:
        scrollDisplay();
        break;
    }

}