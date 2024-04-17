/* mbed Microcontroller Library
 * Copyright (c) 2019 ARM Limited
 * SPDX-License-Identifier: Apache-2.0
 */

/* mbed Microcontroller Library
 * Copyright (c) 2019 ARM Limited
 * SPDX-License-Identifier: Apache-2.0
 */
 
#include "mbed.h"
//#include "kiss_fft.h"
//#include <fftw3.h>
//#include "arm_math.h" // Include CMSIS DSP library
#include <cstdio>
#include <iostream>
#include <type_traits>
#include <vector>
 
AnalogIn Mic(PA_0); //CN8/A0
AnalogIn Gain(PA_1); //CN8/A1 
AnalogIn Speed(PC_1); //CN8/A4

AnalogOut Speaker(PA_4);
//AnalogOut Speaker(PA_1);

DigitalIn RECORD_BUTTON(PA_8); //CN5/D7
InterruptIn Play(PA_9); //CN5/D4
InterruptIn Graph(PC_4);

DigitalOut OUTPUT(PB_4); //CN9/D5
DigitalOut Recording(PB_5);
DigitalOut Error(PA_10);
DigitalOut leds[] = {DigitalOut(PC_7), DigitalOut(PA_6), DigitalOut(PA_7), DigitalOut(PB_6)}; // Example LED pins

 
#define Vsupply 3.3f //microcontroller voltage supply 3.3V
const int SAMPLE_TIME = 200;
unsigned long millisCurrent;
unsigned long millisLast = 0;
unsigned long millisElapsed = 0;

unsigned long millisCurrentRec;
unsigned long millisLastRec = 0;
unsigned long millisElapsedRec = 0;

int sampleBufferValue = 0;
float Volume;
vector<float> Audio;
std::vector<float> FrequencySamples; // Separate vector for frequency analysis
float AvgVolume = 0;
float TotalVolume = 0;
unsigned long long VolumeIndex = 0;

const int NUM_LEDS = 4;
const int NUM_BANDS = 4; // Number of frequency bands

const int BAND_FREQS[NUM_BANDS][2] = {
    {20, 200},   // Bass frequencies
    {200, 800},  // Midrange frequencies
    {800, 4000}, // High-mid frequencies
    {4000, 20000} // Treble frequencies
};

bool released = true;
const int LONGPRESS_TIME = 2000;

float GainSensor;
float GainValue;

float SpeedSensor;
float SpeedValue;



EventQueue event_queue;
Thread event_thread(osPriorityNormal);

void GetGain (void) {
    GainSensor = Gain.read()/2;
    if (GainSensor > 1) {
        GainValue = 1 + GainSensor;
    } else {
        GainValue = 1 - GainSensor;
    }
}

void GetSpeed (void) {
        SpeedSensor = Speed.read()/2;
    if (SpeedSensor > 1) {
        SpeedValue = 1 - SpeedSensor;
    } else {
        SpeedValue = 1 + SpeedSensor;
    }
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
    for(int i = 0; i < Audio.size(); i++){
        millisCurrent = Kernel::get_ms_count();
        millisLast = Kernel::get_ms_count();
        millisElapsed = millisCurrent - millisLast;
        while(millisElapsed < 10){
            millisCurrent = Kernel::get_ms_count();
        }
        cout << Audio[i] << endl;
        Speaker = Audio[i] * 10;
    }
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
            total = total + Audio[j];
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

/*void sampleAndPopulateFFTInput() {
    // Perform ADC sampling and populate fft_input array
    for (int i = 0; i < FFT_SIZE; i++) {
        // Sample microphone input voltage and convert to digital value
        float voltage = Mic.read() * 3.3f; // Convert normalized value to voltage (assuming Vsupply is 3.3V)
        // Populate the real part of the complex input
        fft_input[i][0] = voltage;
        // Imaginary part is set to 0 for simplicity
        fft_input[i][1] = 0;
    }
}


// Function to perform FFT and update LEDs based on frequency analysis results
void updateLEDsFromFFT() {
    // Update our FFT input
    sampleAndPopulateFFTInput();
    // Perform FFT on the input signal
    fftw_execute(fft_plan);

    // Calculate magnitude of each frequency bin
    for (int i = 0; i < FFT_SIZE; i++) {
        fft_output[i][0] = sqrt(fft_output[i][0] * fft_output[i][0] + fft_output[i][1] * fft_output[i][1]);
        fft_output[i][1] = 0; // Clear imaginary part
    }

    // Divide the FFT output into frequency bands and calculate energy levels
    std::vector<float> frequency_bands(NUM_BANDS, 0.0);
    for (int i = 0; i < NUM_BANDS; i++) {
        int start_index = i * (FFT_SIZE / NUM_BANDS);
        int end_index = (i + 1) * (FFT_SIZE / NUM_BANDS);
        for (int j = start_index; j < end_index; j++) {
            frequency_bands[i] += fft_output[j][0];
        }
        frequency_bands[i] /= (end_index - start_index); // Average energy level within the band
    }

    // Update LEDs based on frequency analysis results
    for (int i = 0; i < NUM_LEDS; i++) {
        // Calculate LED brightness based on the energy level of the corresponding frequency band
        float energy = frequency_bands[i];

        // Map energy level to LED brightness based on frequency band
        int brightness = 0;
        if (energy >= BAND_FREQS[i][0] && energy <= BAND_FREQS[i][1]) {
            // Energy falls within the frequency band range
            brightness = 1; // Set LED brightness to a non-zero value to indicate presence of energy
        } else {
            // Energy level is outside the frequency band range
            brightness = 0; // Set LED brightness to 0
        }

        // Update LED brightness
        leds[i] = brightness;
    }
}*/

// Function to perform ADC sampling and populate the frequency samples vector
void sampleAndPopulateFrequencySamples() {
    // Perform ADC sampling and populate the frequency samples vector
    Volume = Mic.read() * Vsupply; // Convert normalized value to voltage (assuming Vsupply is 3.3V)
    FrequencySamples.push_back(Volume);
}

// Function to estimate frequency using zero-crossing detection
float estimateFrequency() {
    int numCrossings = 0;
    bool aboveZero = false;
    float frequency = 0.0;

    // Iterate over the frequency samples to detect zero crossings
    for (size_t i = 1; i < FrequencySamples.size(); i++) {
        if (FrequencySamples[i] > 0) {
            aboveZero = true;
        } else if (FrequencySamples[i] < 0 && aboveZero) {
            numCrossings++;
            aboveZero = false;
        }
    }

    // Calculate frequency based on zero crossings
    if (numCrossings > 0) {
        frequency = 0.5 * FrequencySamples.size() / numCrossings; // Sample rate divided by number of crossings
    }

    return frequency;
}

void updateLEDs(float freq) {
    // Define frequency ranges for each LED
    const float ranges[NUM_LEDS][2] = {
        {20, 200},   // LED 1 (Low frequency range)
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



void LongPressRecord(void) {

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

            if (RECORD_BUTTON.read() <= 0) {
                cout << "Release Stop" << endl;
                Recording = 0;
                return;
            }
        
            Audio.push_back(Volume);
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
        millisCurrentRec = Kernel::get_ms_count();
        millisElapsed = millisCurrent - millisLast;
        millisElapsedRec = millisCurrentRec - millisLastRec;
        if (millisElapsed >= 1) {
            GetVolume();

            if (RECORD_BUTTON.read() > 0) {
                cout << "Press Stop" << endl;
                Recording = 0;
                released = false;
                return;
            }
        
            Audio.push_back(Volume);
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

            if (Volume >= AvgVolume + 0.8)
                sampleBufferValue++;

            if (millisElapsedRec > SAMPLE_TIME) {
                if (sampleBufferValue >= 1 && sampleBufferValue < 3) {
                sampleBufferValue = 0;
                wait_us(1000000);
                Recording = 0;
                millisElapsedRec = 0;
                millisLastRec = 0;
                cout << "Spike Stop" << endl;
                return;
                }
            }
        
            Audio.push_back(Volume);
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
            released = false;
            return;
        }
    }

    cout << "Shortpress" << endl;
    Recording = 1;
    ShortPressRecord();
    Recording = 0;
    cout << "Short Press Recording Finished" << endl;
}

int main(void)
{

event_thread.start(callback(&event_queue, &EventQueue::dispatch_forever));

Play.rise(event_queue.event(&PlayBack));
Graph.rise(event_queue.event(&Draw));


 while(true) {

     if (RECORD_BUTTON.read() == 0)
        released = true;

     if (RECORD_BUTTON.read() > 0 && released)
         RecordPress();

     Speaker = 0;
     millisCurrent = Kernel::get_ms_count();
     millisElapsed = millisCurrent - millisLast;

     GetVolume();
     if (Volume >= AvgVolume + 0.8)
        sampleBufferValue++;

     if (millisElapsed > SAMPLE_TIME) {
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

    wait_us(100000); // Adjust delay as needed
    }
}
// End of HardwareInterruptSeedCode
