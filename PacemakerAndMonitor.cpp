#include "mbed.h"
#include <stdio.h>
#include <stdlib.h>
#include <map>
#include <vector>
#include <iostream>
#include <string>
#include <time.h>       /* time_t, time, ctime */

#include "ESP8266Interface.h"
#include "MQTTNetwork.h"
#include "MQTTmbed.h"
#include "MQTTClient.h"
#include "mbed_mem_trace.h"

#define MQTTCLIENT_QOS2 1
#define IAP_LOCATION 0x1FFF1FF1

#include "ESP8266Interface.h"
#include "MQTTNetwork.h"
#include "MQTTmbed.h"
#include "MQTTClient.h"
#include "TextLCD.h"
#include "mbed.h"
#include <stdlib.h>     /* srand, rand */
#include <time.h>       /* time */

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
DigitalOut errorLED(LED1);
DigitalOut tachyLED(LED3);

int RI    = 2000;

int URI_L = 592;
int URI_H = 608;
int URI   = 600;

int LRI_L = 1492;
int LRI_H = 1508;
int LRI   = 1500;

int HRI   = 2000;

int VRP   = 300;



TextLCD lcd(p15, p16, p17, p18, p19, p20, TextLCD::LCD16x2);//rs,e,d4-d7
char uuid[100];

bool natural_beat = true;
bool started      = false;
bool alarmSet     = false;

int oldTimeForURI = 700;

Thread pacemakerThread(osPriorityNormal, 1024); // Thread to operate the monitor

// Topic Names
const char* dataPubTopic = "group8/data";
const char* slowAlarmPubTopic = "group8/slowHeartBeatAlarm";
const char* fastAlarmPubTopic = "group8/fastHeartBeatAlarm";
//const char* tachychardiaAlarmPubTopic = "group8/tachychardiaAttackAlarm";

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
      oldTimeForURI = savedTime;
      timer.reset();
      RI = LRI;
      natural_beat = false;
      break;
    case 1:
//        syncMap[VSenseS] = false;
        senseLED = !senseLED;
        oldTimeForURI = timer.read_ms();
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
        tachyLED = !tachyLED;
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
        tachyLED = !tachyLED;
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
    case 3:  return oldTimeForURI < URI;
    case 4:  return oldTimeForURI >= URI;
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


// Check all global properties for all execution paths
int checkGlobalProperties() {
    // A[] not deadlock
    // This is not verified here

    // A[] (Ventricle.x $<$ VRP \&\& Ventricle.started) imply (Ventricle.WaitVRP $||$ Ventricle.PostWaitPreSense $||$ Ventricle.Sensed)
    int curTime = timer.read_ms();
    bool errorExists = false;

    if ((curTime < VRP && started)) {
        if ((TRANS[2].active == true) || ((TRANS[3].active == true)||(TRANS[4].active == true)) || ((TRANS[5].active == true))) {
            // All good
        }
        else {
            errorExists = true;
        }
    }

    // A[] ((Ventricle.WaitRI \&\& Ventricle.started \&\& !Ventricle.natural\_beat) imply Ventricle.x $<=$ Ventricle.LRI)
    if (((TRANS[0].active == true) || ((TRANS[1].active == true))) && started && !natural_beat) {
        if (curTime <= LRI+8) {
            // All good
        }
        else {
            errorExists = true;
        }
    }

    // A[] ((Ventricle.WaitRI \&\& Ventricle.started \&\& Ventricle.natural\_beat) imply Ventricle.x $<=$ Ventricle.HRI)
    if (((TRANS[0].active == true) || ((TRANS[1].active == true))) && started && natural_beat) {
        if (curTime <= HRI+8) {
            // All good
        }
        else {
            errorExists = true;
        }
    }

    // A[] ((Ventricle.WaitRI \&\& Ventricle.started)) imply Ventricle.x $>=$ VRP
    if (((TRANS[0].active == true) || ((TRANS[1].active == true))) && started) {
        if (curTime >= VRP) {
            // All good
        }
        else {
            errorExists = true;
        }
    }

    // A[] (Ventricle.x $<$ Ventricle.URI \&\& Ventricle.Sensed imply Observer.alarmSet $==$ true)
    if (curTime < URI && TRANS[5].active == true) {
        if (syncMap[tachychardiaR] == true) {
            // All good
        }
        else {
            errorExists = true;
        }
    }

    // If error, turn off LED4
//    if (errorExists) {
      errorLED = errorExists;
//    }

    return 0;
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
  checkGlobalProperties();
  return count_trans;
}

void pacemaker_thread(){
  timer.start();
  while (true) {
    check_trans();
  }
}

void publishData(MQTT::Client<MQTTNetwork, Countdown> &client, const char* topic, char* buf)
{
//   char buf[10];
   MQTT::Message message;

   message.qos = MQTT::QOS0;
   message.payload = (void*)buf;
   message.payloadlen = strlen(buf)+1;
   message.dup = false;
   message.retained = false;
   int err = client.publish(topic, message);
   if(err != 0){
       printf("Error publishing data for the topic: %s, %d\r\n", buf, err);
    }
   else
   {
       printf("Data published is: %s.\r\n", buf);
//       client.yield(2);
   }
}

int runObserver(MQTT::Client<MQTTNetwork, Countdown> &client) {
     int numTimeSegments = W/P;
     int arr[numTimeSegments];

     for(int i=0;i < numTimeSegments; i++)
        arr[i] = 0;

     int value = 0;
     int oldest = 0;
     float avg = 0;
     int dataTimeStamp = 0;
     int dataStartTime = 0;
     int dataEndTime = P;
     char buf[1000];

     Timer observerTimer;
     observerTimer.start();

     dataPass *d;



     while (true) {

          time_t rawtime;
          struct tm * timeinfo;
          time (&rawtime);


          messageQMutex.lock();
          osEvent evt = messageQ.get(0);
          messageQMutex.unlock();

//          printf("The value is: %d at: %d\n", value, oldest);
          //If the observerTimer value is 5 sec or a little above 5 sec, then send the avg data to the cloud and reset the observerTimer
          if(observerTimer.read() >= P){
                arr[oldest] = value;

                avg = 0;
                for(int i=0;i < numTimeSegments; i++)
                    avg = avg + arr[i];
                avg = avg / (W + 0.0);
                avg *= 60;

//                printf("a[0] = %d, a[1] = %d, a[2] = %d, a[3] = %d\n",arr[0],arr[1],arr[2], arr[3]);
                printf("avg = %f\n", avg);
                //If avg is above URI, then the heart is beating very fast, send the fastheartbeart alarm to the cloud
                if(avg > 60*1000/URI){
                   memset(buf, 0, sizeof(buf));
                   timeinfo = localtime (&rawtime);
                  // std::cout<<"Time is: "<<asctime(timeinfo)<<"\n";
                    sprintf(buf, "%s= %s\n", asctime(timeinfo), "On average, the heart is beating fast");
                  //sprintf(buf, "%s\n", "The heart is beating fast");
                    publishData(client, fastAlarmPubTopic, buf);
                }

                oldest++;
                if(oldest == numTimeSegments)
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
                sprintf(buf, "%d= %f\n", dataTimeStamp, avg);
                publishData(client, dataPubTopic, buf);
                observerTimer.reset();
          }
          if(evt.status == osEventMessage) {
              d = (dataPass*)evt.value.p;
              timeinfo = localtime (&rawtime);
              // If its pacing channel then send the pacing alarm to the cloud
              if(d->c == VPaceR){
                    memset(buf, 0, sizeof(buf));
                    //printf("Time is: %s\n", asctime(timeinfo));
                    sprintf(buf, "%s= %s\n", asctime(timeinfo), "The heart is beating slow");
                    //sprintf(buf, "%s\n", "The heart is beating slow");
                    publishData(client, slowAlarmPubTopic, buf);
                    value ++;
              } else if(d->c == VSenseR){
                  value ++;
              } else if(d->c == tachychardiaR){
                    memset(buf, 0, sizeof(buf));
                    sprintf(buf, "%s= %s\n", asctime(timeinfo), "The heart is beating fast in this cycle");
                    publishData(client, fastAlarmPubTopic, buf);
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

    errorLED = 1; // Initialize this to on

    pacemakerThread.start(pacemaker_thread);

    // timer.start();

//    MQTT::Client<MQTTNetwork,Countdown> client =
    wifi.set_credentials(MBED_CONF_APP_WIFI_SSID, MBED_CONF_APP_WIFI_PASSWORD,
                                                        NSAPI_SECURITY_NONE);

    MQTTNetwork cubnet(&wifi);
    printf("Trying to connect\n");
    if (wifi.connect() == 0) {
        printf("Connected to WiFi\n");

        MQTT::Client<MQTTNetwork,Countdown> client(cubnet);
//        client = *_client;
        const char* hostname = "35.188.242.1";
        int port = 1883;
        int rc = cubnet.connect(hostname, port);
        if (rc != 0)
            return -1;
        printf("Connected to Broker\n");

        MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
        data.MQTTVersion = 3;
        data.username.cstring = "mbed";
        data.password.cstring = "homework";
        data.clientID.cstring = uuid;
        if ((rc = client.connect(data)) != 0) {
            return -1;
        }
        printf("Client Connection Success\n");


        char *dat = "1";

        runObserver(client);
    }
    else printf("Fail to connect to network: %s\n", wifi.get_mac_address());
    return 0;
}
