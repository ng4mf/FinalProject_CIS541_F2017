#include "mbed.h"
#include <stdio.h>
#include <stdlib.h>
#include <map>
#include <vector>
#include <string>
#include <time.h>       /* time_t, time, ctime */

#include "ESP8266Interface.h"
#include "MQTTNetwork.h"
#include "MQTTmbed.h"
#include "MQTTClient.h"
#include "mbed_mem_trace.h"

#define NB_TRANS 14
#define MQTT_BROKER_NAME "35.188.242.1"
#define MQTT_BROKER_PORT 1883
#define P 5
#define W 20

ESP8266Interface wifi(p28, p27);

using namespace std;

int savedTime;

DigitalOut paceLED(LED4);
DigitalOut senseLED(LED2);

int RI    = 2000;

int URI_L = 592;
int URI_H = 608;
int URI   = 600;

int LRI_L = 1492;
int LRI_H = 1508;
int LRI   = 1500;

int HRI   = 2000;

int VRP   = 300;

bool natural_beat = true;
bool started      = false;
bool alarmSet     = false;

Thread monitorThread(osPriorityNormal, 1024); // Thread to operate the monitor

typedef enum {
  WaitRI, WaitVRP, Ready, VSensed, VPaced, WaitRL, PostWaitPreSense, Sensed
} state;

typedef enum {
  VPaceS  = 0,
  VPaceR  = 1,
  VSenseS = 2,
  VSenseR = 3,
  tachychardiaS = 4,
  tachychardiaR = 5,
  none = -1
} chan;

typedef struct {
  bool active;
  state from;
  state to;
  chan  sync;
  bool  isRemote;
} trans_t;

trans_t TRANS[NB_TRANS] = {
//   active state_from          state_to          sync_chan      on_remote_mbed
    {true,  WaitRI,             WaitVRP,          VPaceS,        false},//0
    {true,  WaitRI,             PostWaitPreSense, VSenseR,       false},//1-
    {false, WaitVRP,            WaitRI,           none,          false},//2
    {false, PostWaitPreSense,   Sensed,           tachychardiaS, false},//3
    {false, PostWaitPreSense,   Sensed,           none,          false},//4
    {false, Sensed,             WaitVRP,          none,          false},//5 ____
    {true,  Ready,              Ready,            VSenseS,       true },//6
    {true,  Ready,              Ready,            VPaceR,        true },//7-
    {false, VSensed,            VSensed,          tachychardiaR, false},//8-____
    {false, VSensed,            WaitRL,           none,          false},//9
    {true,  WaitRL,             VSensed,          VSenseR,       false},//10-
    {true,  WaitRL,             WaitRL,           tachychardiaR, false},//11-
    {true,  WaitRL,             VPaced,           VPaceR,        false},//12-
    {false, VPaced,             WaitRL,           none,          false} //13
};

typedef struct {
  int curTime;
  chan c;
} dataPass;

Mutex messageQMutex;
Queue<dataPass, 30> messageQ;

map<chan, bool> syncMap;

//TextLCD lcd(p15, p16, p17, p18, p19, p20, TextLCD::LCD16x2);//rs,e,d4-d7

//DigitalOut myled(LED1);             // Indicator for heart beat

InterruptIn heartbeatRecved(p10);   // Signal from heart that beat happened
DigitalOut  pacePin(p20);           // Pin to output pace signal on

Timer timer;

void interruptFunction() {
  syncMap[VSenseS] = true;
}
//
void addToQ(int curTime, chan c){
      //printf("Adding to queue\n");
      dataPass *d = (dataPass*)malloc(sizeof(dataPass));
      d->curTime = curTime;
      d->c = c;
      messageQMutex.lock();
      messageQ.put(d);
      messageQMutex.unlock();
}
//

void sendVPace()
{
    //printf("Sending pace\n");
    Timer timer2;
    timer2.start();
    pacePin = true;
    paceLED = !paceLED;
    while (timer2.read_ms() < 2);
    pacePin = false;
//    paceLED = false;

    // Need to make sure next cyclic VSense doesn't immediately trigger this
    syncMap[TRANS[6].sync] = false;
}

//
void ASSIGN(int t) {
  switch(t) {
    case 0:
      sendVPace();
      savedTime = timer.read_ms();
      //printf("Time before reset is: %d\n", savedTime);
      timer.reset();
      RI = LRI;
      natural_beat = false;
      break;
    case 1:
//        syncMap[VSenseS] = false;
        senseLED = !senseLED;
        timer.reset();
        break;
    case 2:
      started = true;
//      syncMap[VSenseS] = false;
      break;
    case 3: break;
    case 4: break;
    case 5:
      savedTime = timer.read_ms();
      //printf("Time before reset is: %d\n", savedTime);

      RI = HRI;
      natural_beat = true;
      break;
    case 6: break;
    case 7:

        break;
    case 8:
        alarmSet = true;
        addToQ(savedTime, tachychardiaR);
        break;
    case 9: break;
    case 10:
//        //printf("Transitioning on Arrow 10\n");
        alarmSet = false;
        addToQ(savedTime, VSenseR);
        break;
    case 11:
        alarmSet = true;
        addToQ(savedTime, tachychardiaR);
        break;
    case 12:
        alarmSet = false;
        addToQ(savedTime, VPaceR);
        break;
    case 13:break;
    default:break;
  }
}
//
void ASSIGN_VEC(vector<int> v) {
  for (vector<int>::iterator it = v.begin(); it!= v.end(); ++it) {
    if (TRANS[*it].active == true)
        ASSIGN(*it);
  }
  return;
}

void TRANS_TAKEN_HOOK(int t) {
  state goTo = TRANS[t].to;
  int minInd = 0;
  int maxInd = 13;

  //printf("Transitioning on arrow: %d\n", t);

  if (t <= 5) {
      maxInd = 5;
  } else if (t <= 7) {
      minInd = 6;
      maxInd = 7;
  } else {
      minInd = 8;
      maxInd = 13;
  }

  for (int i = minInd; i <= maxInd; i++) {
    if (TRANS[i].from != goTo) {
        //printf("Updating to activeness false: %d\n", i);
      TRANS[i].active = false;
    } else {
        //printf("Updating to activeness true : %d\n", i);
        TRANS[i].active = true;
        // For receiving sync channels, update to true

    }
  }
  //printf("Done transitioning\n");
}
//
void TRANS_TAKEN_HOOK_VEC(vector<int> v) {
  for (vector<int>::iterator it = v.begin(); it!= v.end(); ++it) {
    if (TRANS[*it].active == true)
        TRANS_TAKEN_HOOK(*it);
  }
}
//
bool EVAL_GUARD(int t) {
  //printf("Eval Guard for Arrow %d | Time is %d\n", t, timer.read_ms());
  switch(t) {
    case 0:  return timer.read_ms() >= RI;//difftime(time(NULL), oldTime) >= RI;
    case 1:  return true;
    case 2:  return timer.read_ms() >= VRP;
    case 3:  return timer.read_ms() < URI;
    case 4:  return timer.read_ms() >= URI;
    case 5:  return true;
    case 6:  return true; // Whether heart sent a sensed beat
    case 7:  return true; // Heart can't control a pace, so always ready to take one
    case 8:  return true;
    case 9:  return true;
    case 10: return true; // Always ready to take a sync signal
    case 11: return true;
    case 12: return true;
    case 13: return true;
  }
  return true;
}
//
void clear_and_set(int t) {
    // Set this nodes and all its complements sync channels to false
//  for (int i = 0; i <= 13; i++) {
//    if (i == 1 || i == 7 || i== 8 || i == 10 || i == 11 || i == 12)
//        syncMap[TRANS[i].sync] = true;
//    else
//        syncMap[TRANS[i].sync] = false; // Clear all sync flags
//  }
//  syncMap[none] = true;
    syncMap[VPaceS]        = false;
    syncMap[VPaceR]        = true;
    syncMap[VSenseS]       = false;
    syncMap[VSenseR]       = true;
    syncMap[tachychardiaS] = false;
    syncMap[tachychardiaR] = true;
    syncMap[none]          = true;
}
//
vector<int> getComplements(int t) {
//  //printf("Getting complements for %d\n", t);
  vector<int> v;
  switch(t) {
    case 0:
      v.push_back(7);
      v.push_back(12);
      break;
    case 1:
      v.push_back(6);
      v.push_back(10);
      break;
    case 2:
      break;
    case 3:
      v.push_back(8);
      v.push_back(11);
      break;
    case 4:
      break;
    case 5:
      break;
    case 6:
      v.push_back(1);
      v.push_back(10);
      break;
    case 7:
      v.push_back(0);
      v.push_back(12);
      break;
    case 8:
      v.push_back(3);
      v.push_back(11);
      break;
    case 9:
      break;
    case 10:
      v.push_back(1);
      v.push_back(6);
      break;
    case 11:
      v.push_back(3);
      v.push_back(8);
      break;
    case 12:
      v.push_back(0);
      v.push_back(7);
      break;
    case 13:
      break;
    default:
      break;
  }
  return v;
}
//
bool IS_SEND(int t) {
  if (t==0 || t==3 || t==6){
    return true;
  } else return false;
}
//
bool check_complement_sync(int t) {
    if (!IS_SEND(t)) return false;
//    if (syncMap[TRANS[t].sync] == false) return false; // If this arrows sync is false,

    vector<int> v = getComplements(t);
    for (vector<int>::iterator it = v.begin(); it != v.end(); ++it) {
      //printf("Complement for %d is arrow %d, has value %d, is active? %d\n", t, *it, syncMap[TRANS[*it].sync],TRANS[*it].active);
      if (syncMap[TRANS[*it].sync] == false && TRANS[*it].active == true)
        return false;
    }
    return true;
}
//
int check_trans() {
  int trn;
  int compl_trn;
  int count_trans = 0;
  vector<int> complements;

//  //printf("Current time: %d\n", timer.read_ms());
  // Loop through the set of edges in all templates
  for (trn = 0; trn < NB_TRANS; trn++) {
//    printf("%d | %d | %d\n", trn, syncMap[VSenseS], timer.read_ms());
    // If an edge we examine is active, i.e. we are in a state such that this
    //// edge can be taken, then
    if ( TRANS[trn].active ) {
      //printf("Arrow %d is active\n", trn);
      // If we have met all the conditions of the guards on a transition
      if (EVAL_GUARD(trn)) { // WRITTEN
        //printf("Passed guard evaluation for arrow %d\n", trn);
        // If there is a synchronization channel on the edge, ensure no violation
        if (TRANS[trn].sync != none) {
          //printf("There is a sync channel for arrow %d with value %d\n", trn, syncMap[TRANS[trn].sync]);

          // If there is a channel, get all complementary channels
          complements = getComplements(trn);

          // If the number of complements is >0, and sync channels are all true
          if (complements.size() > 0) {
            //printf("There are complements for arrow %d\n", trn);
            if ((!TRANS[trn].isRemote || syncMap[TRANS[trn].sync] == true) && check_complement_sync(trn)) {
                //printf("Syncs all good (arrow %d)\n", trn);
                if (IS_SEND(TRANS[trn].sync)) { // If this arrow is syncer
                  //printf("This is the syncer (arrow %d)\n", trn);
                  ASSIGN( trn );
                  ASSIGN_VEC( complements );

                  TRANS_TAKEN_HOOK(trn);
                  TRANS_TAKEN_HOOK_VEC(complements);
                } else { // If not the transition arrow doing syncings
                  // Complements vector must be size 1 then
                  // Need to check if the sync is true or not

                  ASSIGN_VEC( complements );
                  ASSIGN( trn );

                  TRANS_TAKEN_HOOK_VEC(complements);
                  TRANS_TAKEN_HOOK(trn);
                }
                clear_and_set(trn);
//                clear_and_set(compl_trn);
                count_trans+=2;
//                 trn=-1;
            }
          }
        } else { // If no sync channel on this transition arrow
//              //printf("No sync channel for arrow %d\n", trn);
              ASSIGN(trn);
              TRANS_TAKEN_HOOK(trn);
              clear_and_set(trn);
              count_trans++;
//               trn = -1;
        }
      }
    }
  }
  return count_trans;
}

void monitor_thread(){
    dataPass *d;
//    //printf("SAMPLE\n");

    //Setting the credentials
    wifi.set_credentials(MBED_CONF_APP_WIFI_SSID, MBED_CONF_APP_WIFI_PASSWORD);
    //Connect to wifi
    int err=wifi.connect();
    if(err!=0){
        printf("Could not connect to the wifi as err is %d\n", err);
    }
    
    MQTTNetwork mqttNetwork(&wifi);
    int rc = mqttNetwork.connect(MQTT_BROKER_NAME, MQTT_BROKER_PORT);
    
    //Create MQTT client
    MQTT::Client<MQTTNetwork, Countdown> client = MQTT::Client<MQTTNetwork, Countdown>(mqttNetwork);
    
    //Initialize the data required for connection to MQTT as a publisher
    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;    
    data.username.cstring = "pacemaker";
    data.password.cstring = "project";
    data.clientID.cstring = "Group8";
    
    //Connect to MQTT as a client
    rc = client.connect(data);  
    if(rc != 0){
        printf("Client could not connect to MQTT Broker as rc is: %d\n", rc);
    }
    
    const char* dataPubTopic = "group8/data";
    const char* slowAlarmPubTopic = "group8/slowHeartBeatAlarm";
    const char* fastAlarmPubTopic = "group8/fastHeartBeatAlarm";
    
    char buf[100];
    
    int arr[4];
    arr[0]=arr[1]=arr[2]=arr[3]=0;
    int value = 0;
    int oldest = 0;
    float avg = 0;
    int dataTimeStamp = 0;
    int dataStartTime = 0;
    int dataEndTime = P;
    
    time_t rawtime;
    struct tm * timeinfo;
    time (&rawtime);
   
    MQTT::Message message;
    message.qos = MQTT::QOS0;
    message.retained = false;
    message.dup = false;
    
    //Timer for sending values on the cloud
    Timer timer;
    timer.start();
    while (1) {
      messageQMutex.lock();
      osEvent evt = messageQ.get(0);
      messageQMutex.unlock();
      //If the timer value is 5 sec or a little above 5 sec, then send the avg data to the cloud and reset the timer
      if(timer.read() >= P){
          arr[oldest] = value;
          avg = (arr[0]+arr[1]+arr[2]+arr[3])/20000;
          //If avg is above URI, then the heart is beating very fast, send the fastheartbeart alarm to the cloud
          if(avg > URI){
            memset(buf, 0, sizeof(buf));
            timeinfo = localtime (&rawtime);
            sprintf(buf, "%s: %s", asctime(timeinfo), "The heart is beating fast");
            message.payload = (void*) buf;
            message.payloadlen = strlen(buf);
            if((rc=client.publish(fastAlarmPubTopic, message))!=0){
                printf("Could not publish as rc is %d\n", rc);
                }
              }
            oldest++;
            if(oldest == 4)
                oldest = 0;
            value = 0;
            //Time calculation to be send for avg data
            dataTimeStamp = (dataStartTime+dataEndTime)/2;
            if(dataEndTime >= W){
                dataStartTime = dataStartTime+5;
                }
            dataEndTime = dataEndTime+5;
            //Send the avg value to the cloud
            memset(buf, 0, sizeof(buf));
            sprintf(buf, "%d: %f", dataTimeStamp, avg);
            message.payload = (void*) buf;
            message.payloadlen = strlen(buf);
            if((rc=client.publish(dataPubTopic, message))!=0){
                printf("Could not publish as rc is %d\n", rc);
                }            
            timer.reset();
          }
      if(evt.status == osEventMessage) {
        d = (dataPass*)evt.value.p;
        // USE
        //printf("[%d] CHAN VAL: %d\n", d->curTime, d-> c);
        // If its pacing channel then send the pacing alarm to the cloud
        if(d->c == VPaceR){
        memset(buf, 0, sizeof(buf));
        timeinfo = localtime (&rawtime);
        sprintf(buf, "%s: %s", timeinfo = localtime (&rawtime), "The heart is beating slow");
        message.payload = (void*) buf;
        message.payloadlen = strlen(buf);
        if((rc=client.publish(slowAlarmPubTopic, message))!=0){
            printf("Could not publish as rc is %d\n", rc);
                } 
        value += LRI;
        } else if(d-> c == VSenseR){
            value += HRI;
            }
        
        free(d);
      }//end of event driven loop
    }//end of while loop
}
    
int main() { // This is the pacemaker thread after initialization
    heartbeatRecved.rise(&interruptFunction);
    paceLED = true;

    syncMap[VPaceS]        = false;
    syncMap[VPaceR]        = true;
    syncMap[VSenseS]       = false;
    syncMap[VSenseR]       = true;
    syncMap[tachychardiaS] = false;
    syncMap[tachychardiaR] = true;
    syncMap[none]          = true;

    monitorThread.start(monitor_thread);

    timer.start();
    while(1) {
      check_trans();
    }

}