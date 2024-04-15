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
#include <vector>
 
AnalogIn Mic(PA_0, PullDown); //CN8/A0
AnalogOut Speaker(PA_4);
//AnalogOut Speaker(PA_1);

InterruptIn LED_Reset(PA_8, PullDown); //CN5/D4
InterruptIn Play(PA_9, PullDown); //CN5/D4
InterruptIn Graph(PC_4, PullDown);

DigitalOut OUTPUT(PB_4); //CN9/D5
DigitalOut Recording(PB_5);
DigitalOut Error(PA_10);
 
#define Vsupply 5.0f //microcontroller voltage supply 3.3V
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


void GetAvgVolume (void) {
    TotalVolume = TotalVolume + Volume;
    VolumeIndex++;
    AvgVolume = TotalVolume / VolumeIndex;
}

void GetVolume(void) {
    Volume = (1-Mic.read()) * 5;
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


int Draw() {
    const int graph_width = 100; // Width of the graph
    const int num_points = 500;  // Number of points on the graph
    const int max_value = 200;   // Maximum value on the graph

    // Generate some example data

    double phase = 0.0;

    while (true) {
        std::cout << "\x1B[2J\x1B[H"; // ANSI escape codes for clearing the screen and moving the cursor to the top-left corner

        // Print the graph
        for (int y = max_value; y >= 0; --y) {
            for (int x = 0; x < num_points; ++x) {
                char c = ' ';
                if (std::round(Audio[x]*100) == y) {
                    c = '*';
                }
                std::cout << c;
            }
            std::cout << std::endl;
        }

        millisCurrent = Kernel::get_ms_count();
        millisLast = Kernel::get_ms_count();
        millisElapsed = millisCurrent - millisLast;
        while(millisElapsed < 10){
            millisCurrent = Kernel::get_ms_count();
        }
       
        phase += 0.1;
    }

    return 0;
}


void Record(void) {
    cout << "Recording" << endl;
    int i = 0;
    Audio.clear();
    while (i < 500) {
        millisCurrent = Kernel::get_ms_count();
        millisCurrentRec = Kernel::get_ms_count();
        millisElapsed = millisCurrent - millisLast;
        millisElapsedRec = millisCurrentRec - millisLastRec;
        if (millisElapsed >= 10) {
            GetVolume();

            // if (Volume >= AvgVolume + 0.8)
            //     sampleBufferValue++;

            // if (millisElapsedRec > SAMPLE_TIME) {
            //     if (sampleBufferValue >= 1 && sampleBufferValue < 3) {
            //     sampleBufferValue = 0;
            //     wait_us(1000000);
            //     Recording = 0;
            //     millisElapsedRec = 0;
            //     millisLastRec = 0;
            //     break;
            //     }
            // }
            Audio.push_back(Volume);
            //sampleBufferValue = 0;
            millisLast = millisCurrent;
            i++;
        }
    }
}

int main(void)
{
    EventQueue event_queue;
    Thread event_thread(osPriorityNormal);
    event_thread.start(callback(&event_queue, &EventQueue::dispatch_forever));

    Play.rise(event_queue.event(&PlayBack));
    Graph.rise(event_queue.event(&Draw));
 
 while(true) {
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
             Record();
             Recording = 0;
             millisLast = millisCurrent;
         }
         sampleBufferValue = 0;
     }
 }
}
// End of HardwareInterruptSeedCode