#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "mbed.h"

#define MAXWAIT 1500
#define MINWAIT 500

//Here p10 and p11 are the pins for connecting heart and pacemaker's mbeds, can change it if required
InterruptIn paceRecved(p10);
DigitalOut sendBeat(p11);
DigitalOut blinkBeat(LED1);
DigitalOut paceLED(LED2);
int random_no = (rand() % (MAXWAIT - MINWAIT + 1)) + MINWAIT;

Timer t;
bool pace = false;

void sendVSense()
{
    Timer timer2;
    timer2.start();
    sendBeat = true;
    blinkBeat = true;
    while (timer2.read_ms() < 2);
    sendBeat = false;
    blinkBeat = false;
}

void interruptFunction() {
  t.reset();
  paceLED = !paceLED;
  random_no = (rand() % (MAXWAIT - MINWAIT + 1)) + MINWAIT;
}

int main() {

    paceRecved.rise(&interruptFunction);
    // Use current time as seed for random generator
    srand(time(0));
    //Generate a random number between MINWAIT and MAXWAIT as heart should beat(if it beats naturally) randomly between MIN and MAX wait

    //printf("Rand no. is: %d\n", random_no);

    //Start a timer
    t.start();

    while(true){
        //If time in milli-seconds is greater(due to time taken in executing other commands) or equal to the random number then
        //send the sensing signal to the pacemaker
        if(t.read_ms() >= random_no){
//            printf("Time passed is: %d\n", t.read_ms());
            //Send sensing signal
            sendVSense();

            //Change the random no. now
            random_no = (rand() % (MAXWAIT - MINWAIT + 1)) + MINWAIT;

            //printf("New rand no. is: %d\n", random_no);
            t.reset();
        }
    }

    //Stop the timer
//    t.stop();
}
