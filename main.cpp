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
DigitalIn Graph(PC_4); //D1

DigitalOut OUTPUT(PB_4); //CN9/D1
DigitalOut Recording(PB_5); //D4
DigitalOut Error(PA_10); //D2
 
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
float AvgVolume = 0;
float TotalVolume = 0;
unsigned long long VolumeIndex = 0;

bool released = true;
const int LONGPRESS_TIME = 2000;

bool stabilize = true;

float GainValue;

float SpeedSensor;
float SpeedValue;

EventQueue event_queue;
Thread event_thread(osPriorityNormal);

void GetGain (void) {
    GainValue = Gain.read()*2;
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

    int i = 0;
    while (i < Audio.size()) {
        millisCurrent = Kernel::get_ms_count();
        millisElapsed = millisCurrent - millisLast;
        GetGain();

        if (millisElapsed >= 1) {
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

            if (Volume < AvgVolume)
                stabilize = true;
            if (Volume >= 3.0 && stabilize)
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

     if (RECORD_BUTTON.read() == 0)
        released = true;

     if (RECORD_BUTTON.read() > 0 && released) {
         RecordPress();
         released = false;
     }

     if (Graph.read() == 1) {
         Draw();
     }

     if (Play.read() == 1) {
         PlayBack();
     }

     Speaker = 0;
     millisCurrent = Kernel::get_ms_count();
     millisElapsed = millisCurrent - millisLast;

     GetVolume();
     if (Volume <= AvgVolume)
        stabilize = true;


     if (Volume > 3.0 && stabilize)
        sampleBufferValue++;

     if (millisElapsed > SAMPLE_TIME) {
         if (sampleBufferValue >= 1 && sampleBufferValue < 3) {
             sampleBufferValue = 0;
             wait_us(1000000);
             Recording = 1;
             cout << "Spike Record" << endl;
             SpikeRecord();
             stabilize = false;
             Recording = 0;
             cout << "Spike Press Recording Finished" << endl;
             millisLast = millisCurrent;
         }
         sampleBufferValue = 0;
     }
 }
}
// End of HardwareInterruptSeedCode
