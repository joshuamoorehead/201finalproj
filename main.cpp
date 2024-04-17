/* mbed Microcontroller Library
 * Copyright (c) 2019 ARM Limited
 * SPDX-License-Identifier: Apache-2.0
 */

/* mbed Microcontroller Library
 * Copyright (c) 2019 ARM Limited
 * SPDX-License-Identifier: Apache-2.0
 */
 
#include "mbed.h"
#include <cstdio>
#include <iostream>
#include <type_traits>
#include <vector>
 
AnalogIn Mic(PA_0); //CN8/A0
AnalogIn Gain(PA_1); //CN8/A1 
AnalogIn Speed(PC_1); //CN8/A4

AnalogOut Speaker(PA_4); //D5
//AnalogOut Speaker(PA_1);

DigitalIn RECORD_BUTTON(PA_8, PullDown); //CN5/D7
DigitalIn Play(PA_9); //CN5/D8
DigitalIn Graph(PC_4, PullDown); //D1

DigitalOut OUTPUT(PB_4); //CN9/D1
DigitalOut Recording(PB_5); //D4
DigitalOut Error(PA_10); //D2
 
#define Vsupply 3.3f //microcontroller voltage supply 3.3V
const int SAMPLE_TIME = 2000;
unsigned long millisCurrent;
unsigned long millisLast = 0;
unsigned long millisElapsed = 0;

float maxVol;

unsigned long millisCurrentRec;
unsigned long millisLastRec = 0;
unsigned long millisElapsedRec = 0;

int sampleBufferValue = 0;
float Volume;
vector<float> Audio;
float AvgVolume = 0;
float TotalVolume = 0;
unsigned long long VolumeIndex = 0;

bool released = true;
const int LONGPRESS_TIME = 2000;

bool stabilize = true;

float GainValue;
float SpeedValue;

DigitalOut leds[] = {DigitalOut(PC_7), DigitalOut(PA_6), DigitalOut(PA_7), DigitalOut(PB_6)};



std::vector<float> FrequencySamples; // Separate vector for frequency analysis



const int NUM_LEDS = 4;
const int NUM_BANDS = 4; // Number of frequency bands


// Function to perform ADC sampling and populate the frequency samples vector
void sampleAndPopulateFrequencySamples() {
    // Perform ADC sampling and populate the frequency samples vector
    Volume = Mic.read() * Vsupply; // Convert normalized value to voltage (assuming Vsupply is 3.3V)
    FrequencySamples.push_back(Volume);
}

float estimateFrequency() {
    int numCrossings = 0;
    bool aboveThreshold = false; 
    float frequency = 0.0;

    //Iterating over the frequency samples to detect crossings at the threshold
    for (size_t i = 1; i < FrequencySamples.size(); i++) {
        if (FrequencySamples[i] > 2.5) { //Threshold at 2.5 V
            aboveThreshold = true;
        } else if (FrequencySamples[i] < 2.5 && aboveThreshold) { 
            numCrossings++;
            aboveThreshold = false;
        }
    }

    // Calculate frequency based on crossings at the threshold
    if (numCrossings > 0) {
        frequency = 0.5 * FrequencySamples.size() / numCrossings; // Sample rate divided by number of crossings
    }

    return frequency;
}

void updateLEDs(float freq) {
    // Define frequency ranges for each LED
    const float ranges[NUM_LEDS][2] = {
        {20, 100},   // LED 1 (Low frequency range)
        {200, 800},  // LED 2 (Mid frequency range)
        {800, 4000}, // LED 3 (High frequency range)
        {4000, 20000} // LED 4 (Very high frequency range)
    };

    // Check if the estimated frequency falls within each range and update LEDs accordingly
    for (int i = 0; i < NUM_LEDS; i++) {
        if (freq >= ranges[i][0] && freq <= ranges[i][1]) {
            leds[i] = 1; // Turn on LED
        } else {
            leds[i] = 0; // Turn off LED
        }
    }
}

void GetGain (void) {
    GainValue = Gain.read()*2;
}

void GetSpeed (void) {
    if (Speed.read() > 0.8)
        SpeedValue = 1;
    else if (Speed.read() > 0.6)
        SpeedValue = 0.5;
    else if (Speed.read() > 0.4)
        SpeedValue = 0.25;
    else if (Speed.read() > 0.2)
        SpeedValue = 0.125;
    else if (Speed.read() >= 0.0)
        SpeedValue = 0.0625;

}

void GetAvgVolume (void) {
    TotalVolume = TotalVolume + Volume;
    VolumeIndex++;
    AvgVolume = TotalVolume / VolumeIndex;
}

void GetVolume(void) {
    Volume = Mic.read() * Vsupply;
    GetAvgVolume();
}

void PlayBack(void) {
    if(Audio.size() == 0){
        Error = 1;
        cout << "Empty Audio" << endl;
        return;
    }
    Error = 0;
    cout << "Playing :" << Audio.size() << endl;

    int i = 0;
    while (i < Audio.size()) {
        millisCurrent = Kernel::get_ms_count();
        millisElapsed = millisCurrent - millisLast;
        GetGain();
        GetSpeed();

        if (millisElapsed >= 1/SpeedValue) {
            Speaker = Audio[i]*GainValue;
            millisLast = millisCurrent;
            i++;
        }
    }
    cout << "Playing Finished" << endl;
}


void Draw() {
    if(Audio.size() == 0){
        Error = 1;
        cout << "No Audio Recording" << endl;
        return;
    }

    Error = 0;
    vector<float> avg;
    const int width = 225;
    const int height = 50;

    double sum = 0;
    double total = 0;

    vector<vector<char>> graph(height, vector<char>(width));
    for (int i = 0; i < height; ++i) {
        for (int j = 0; j < width; ++j) {
            graph[i][j] = ' ';
        }
    }
    avg.clear();
    for(int i = 0; i < 4500; i += 20){
        total = 0;
        for (int j = i; j < (i+20); j++) {
            total = total + Audio[j]*Vsupply;
        }
        avg.push_back(floor((total/20)*10));
    }
    cout << avg.size() << endl;
    for(int i =0; i < avg.size(); i ++){
        if((50 - avg[i] - 2) < 50 && avg[i] - 5 > 0){
            graph[50 - (avg[i] - 2)][i] = '*';
        }
    }
    std::cout << "\033[2J\033[H";
    for(int i = 0; i < height; i++){ // Use height as the upper limit
    for(int j = 0; j < width; j++){
        std::cout << graph[i][j];
    }
    std::cout << std::endl;
    }
}

void LongPressRecord(void) {

    cout << "Recording" << endl;
    int i = 0;
    Audio.clear();
    while (i < 4500) {
        millisCurrent = Kernel::get_ms_count();
        millisElapsed = millisCurrent - millisLast;
        if (millisElapsed >= 1) {
            GetVolume();

            if (RECORD_BUTTON.read() <= 0) {
                cout << "Release Stop" << endl;
                Recording = 0;
                return;
            }
        
            Audio.push_back(Mic.read());
            sampleBufferValue = 0;
            millisLast = millisCurrent;
            i++;
        }
    }

}

void ShortPressRecord(void) {

    cout << "Recording" << endl;
    int i = 0;
    Audio.clear();
    while (i < 4500) {
        millisCurrent = Kernel::get_ms_count();
        millisElapsed = millisCurrent - millisLast;
        if (millisElapsed >= 1) {
            GetVolume();

            if (RECORD_BUTTON.read() > 0) {
                cout << "Press Stop" << endl;
                Recording = 0;
                return;
            }
        
            Audio.push_back(Mic.read());
            sampleBufferValue = 0;
            millisLast = millisCurrent;
            i++;
        }
    }
}

void SpikeRecord(void) {

    cout << "Recording" << endl;
    int i = 0;
    Audio.clear();
    while (i < 4500) {
        millisCurrent = Kernel::get_ms_count();
        millisCurrentRec = Kernel::get_ms_count();
        millisElapsed = millisCurrent - millisLast;
        millisElapsedRec = millisCurrentRec - millisLastRec;
        if (millisElapsed >= 1) {
            GetVolume();
        
            Audio.push_back(Mic.read());
            sampleBufferValue = 0;
            millisLast = millisCurrent;
            i++;
        }
    }
}

void RecordPress (void) {

    cout << "Record Press" << endl;
    millisLast = Kernel::get_ms_count();
    
    while (RECORD_BUTTON.read() == 1) {
        millisCurrent = Kernel::get_ms_count();
        millisElapsed = millisCurrent - millisLast;
        if (millisElapsed > LONGPRESS_TIME) {
            cout << "Longpress" << endl;
            Recording = 1;
            LongPressRecord();
            Recording = 0;
            cout << "Long Press Recording Finished" << endl;
            return;
        }
    }

    cout << "Shortpress" << endl;
    Recording = 1;
    ShortPressRecord();
    Recording = 0;
    cout << "Short Press Recording Finished" << endl;
    return;
}

int main(void)
{

 while(true) {

     if (RECORD_BUTTON.read() == 0 &&
         Graph.read() == 0 &&
         Play.read() == 0)
        released = true;

     if (RECORD_BUTTON.read() > 0 && released) {
         RecordPress();
         released = false;
     }

     if (Graph.read() == 1 && released) {
         released = false;
         Draw();
     }

     if (Play.read() == 1 && released) {
         released = false;
         PlayBack();
     }

     Speaker = 0;
     millisCurrent = Kernel::get_ms_count();
     millisElapsed = millisCurrent - millisLast;

     GetVolume();

     if (Volume > 3.2) {
        sampleBufferValue++;
     }

     if (millisElapsed > SAMPLE_TIME && stabilize) {
         if (sampleBufferValue >= 1 && sampleBufferValue < 3) {
             sampleBufferValue = 0;
             wait_us(1000000);
             Recording = 1;
             cout << "Spike Record" << endl;
             SpikeRecord();
             Recording = 0;
             cout << "Spike Press Recording Finished" << endl;
             millisLast = millisCurrent;
        }
        sampleBufferValue = 0;
     }

    // Sample and populate frequency samples vector
    sampleAndPopulateFrequencySamples();

    // Estimate frequency using zero-crossing detection
    float freq = estimateFrequency();

    // Update LEDs based on frequency ranges
    updateLEDs(freq);

    // Reset the frequency samples vector for the next iteration
    FrequencySamples.clear();
    // LED MATRIX
    const int NUM_LEDS = 4;
    float voltageStep = (Vsupply - 2.7f) / (NUM_LEDS - 2); // Divide by (NUM_LEDS - 2) to create a larger gap between led0 and led1
    int ledIndex = static_cast<int>((Volume - 2.7f) / voltageStep);
    ledIndex = (ledIndex < 0) ? 0 : ledIndex;
    ledIndex = (ledIndex >= NUM_LEDS) ? NUM_LEDS - 1 : ledIndex;

    for (int i = 0; i < NUM_LEDS; i++) {
        if (i == 0 || i == 1) {
            leds[i] = (i <= ledIndex) ? 1 : 0;
        } else {
            leds[i] = 0; // Turn off other LEDs
        }
    }

 }
}
// End of HardwareInterruptSeedCode
