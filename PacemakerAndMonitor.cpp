#include "mbed.h"
#include <stdio.h>
#include <stdlib.h>
#include <map>
#include <vector>

#define NB_TRANS 14

using namespace std;

time_t oldTime;
unsigned int RI    = 2000;

unsigned int URI_L = 592;
unsigned int URI_H = 608;
unsigned int URI   = 600;

unsigned int LRI_L = 1492;
unsigned int LRI_H = 1508;
unsigned int LRI   = 1500;

unsigned int HRI   = 2000;

unsigned int VRP   = 300;

bool natural_beat = true;
bool started      = false;
bool alarmSet     = false;

Thread monitorThread(osPriorityNormal, 1024); // Thread to operate the monitor

typedef enum {
  WaitRI, WaitVRP, Ready, VSensed, VPaced, WaitRL, PostWaitPreSense, Sensed
} state;

typedef enum {
  VPaceS, VPaceR, VSenseS, VSenseR, tachychardiaS, tachychardiaR, none
} chan;

typedef struct {
  bool active;
  state from;
  state to;
  chan  sync;
  bool  isRemote;
} trans_t;

trans_t TRANS[NB_TRANS] = {
    {true,  WaitRI,             WaitVRP,          VPaceS,        false},
    {true,  WaitRI,             PostWaitPreSense, VSenseR,       false},
    {false, WaitVRP,            WaitRI,           none,          false},
    {false, PostWaitPreSense,   Sensed,           tachychardiaS, false},
    {false, PostWaitPreSense,   Sensed,           none,          false},
    {false, Sensed,             WaitVRP,          none,          false},
    {true,  Ready,              Ready,            VSenseS,       true },
    {true,  Ready,              Ready,            VPaceR,        true },
    {false, VSensed,            VSensed,          tachychardiaR, false},
    {false, VSensed,            WaitRL,           none,          false},
    {true,  WaitRL,             VSensed,          VSenseR,       false},
    {true,  WaitRL,             WaitRL,           tachychardiaR, false},
    {true,  WaitRL,             VPaced,           VPaceR,        false},
    {false, VPaced,             WaitRL,           none,          false}
};

typedef struct {
  time_t refTime;
  time_t curTime;
  chan c;
} dataPass;

Mutex messageQMutex;
Queue<dataPass, 3> messageQ;

map<chan, bool> syncMap;

//TextLCD lcd(p15, p16, p17, p18, p19, p20, TextLCD::LCD16x2);//rs,e,d4-d7

DigitalOut myled(LED1);             // Indicator for heart beat

InterruptIn heartbeatRecved(p10);   // Signal from heart that beat happened
DigitalOut  pacePin(p11);           // Pin to output pace signal on

Timer t;

void interruptFunction() {
  syncMap[TRANS[6].sync] = true;
}

void addToQ(time_t refTime, time_t curTime, chan c){
      dataPass *d = (dataPass*)malloc(sizeof(dataPass));
      d->refTime = refTime;
      d->curTime = curTime;
      d->c = c;
      messageQMutex.lock();
      messageQ.put(d);
      messageQMutex.unlock();
}

void ASSIGN(int t) {
  switch(t) {
    case 0:
      oldTime = time(NULL);
      RI = LRI;
      natural_beat = false;
      break;
    case 1:
      break;
    case 2:
      started = true;
      break;
    case 3:
      break;
    case 4:
      break;
    case 5:
      oldTime = time(NULL);
      RI = HRI;
      natural_beat = true;
      break;
    case 6:
      // Do nothing, this is the heart
      break;
    case 7:
      // DO nothing, this is the heart
      break;
    case 8:
      alarmSet = true;
      addToQ(oldTime, time(NULL), tachychardiaR);
      break;
    case 9:
      // Nothing
      break;
    case 10:
      alarmSet = false;
      addToQ(oldTime, time(NULL), VSenseR);
      break;
    case 11:
      alarmSet = true;
      addToQ(oldTime, time(NULL), tachychardiaR);
      break;
    case 12:
      alarmSet = false;
      addToQ(oldTime, time(NULL), VPaceR);
      break;
    case 13:
      // Nothing
      break;
    default:
        break;
  }
}

void ASSIGN_VEC(vector<int> v) {
  for (vector<int>::iterator it = v.begin(); it!= v.end(); ++it) {
    ASSIGN(*it);
  }
  return;
}

void TRANS_TAKEN_HOOK(int t) {
  state goTo = TRANS[t].to;
  int minInd = 0;
  int maxInd = 13;

  if (t <= 6) {
      maxInd = 6;
  } else if (t <= 8) {
      minInd = 7;
      maxInd = 8;
  } else {
      minInd = 9;
      maxInd = 13;
  }

  for (int i = minInd; i <= maxInd; i++) {
    if (TRANS[i].from != goTo) {
      TRANS[i].active = false;
    } else TRANS[i].active = true;
  }
}

void TRANS_TAKEN_HOOK_VEC(vector<int> v) {
  for (vector<int>::iterator it = v.begin(); it!= v.end(); ++it) {
    TRANS_TAKEN_HOOK(*it);
  }
}

bool EVAL_GUARD(int t) {
  switch(t) {
    case 0:
      return difftime(time(NULL), oldTime) >= RI;
      break;
    case 1:
      return true;
      break;
    case 2:
      return time(NULL) >= VRP;
      break;
    case 3:
      return time(NULL) < URI;
      break;
    case 4:
      return time(NULL) >= URI;
      break;
    case 5:
      return true;
      break;
    case 6:
      return true; // Whether heart sent a sensed beat
      break;
    case 7:
      return true; // Heart can't control a pace, so always ready to take one
      break;
    case 8:
      return true;
      break;
    case 9:
      return true;
      break;
    case 10:
      return true; // Always ready to take a sync signal
      break;
    case 11:
      return true;
      break;
    case 12:
      return true;
      break;
    case 13:
      return true;
      break;
  }
}

void clear_and_set(int t) {
  for (int i = 0; i <= 13; i++) {
    syncMap[TRANS[i].sync] = false; // Clear all sync flags
  }
  syncMap[none] = true;
}

vector<int> getComplements(int t) {
  vector<int> v;
  switch(t) {
    case 0:
      v.push_back(7);
      v.push_back(12);
      break;
    case 1:
      v.push_back(6);
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
      break;
    case 8:
      v.push_back(11);
      break;
    case 9:
      break;
    case 10:
      v.push_back(6);
      break;
    case 11:
      v.push_back(8);
      break;
    case 12:
      v.push_back(0);
      break;
    case 13:
      break;
    default:
      break;
  }
  return v;
}

bool check_sync(int t) {
    vector<int> v = getComplements(t);
    for (vector<int>::iterator it = v.begin(); it != v.end(); ++it) {
      if (TRANS[*it].sync == false)
        return false;
    }
    return true;
}

bool IS_SEND(int t) {
  if (t==0 || t==3 || t==6){
    return true;
  } else return false;
}

int check_trans() {
  int trn;
  int compl_trn;
  int count_trans = 0;
  vector<int> complements;

  for (trn = 0; trn < NB_TRANS; trn++) {
    if ( TRANS[trn].active ) {
      if (EVAL_GUARD(trn)) { // WRITTEN
        if (TRANS[trn].sync != none) {
          complements = getComplements(trn);
          if (complements.size() > 0) {
            if (IS_SEND(TRANS[trn].sync)) {
              ASSIGN( trn );
              ASSIGN_VEC( complements );

              TRANS_TAKEN_HOOK(trn);
              TRANS_TAKEN_HOOK(compl_trn);
            } else {
              ASSIGN_VEC( complements );
              ASSIGN( trn );

              TRANS_TAKEN_HOOK_VEC(complements);
              TRANS_TAKEN_HOOK(trn);
            }
          }
        } else {
          ASSIGN(trn);
          TRANS_TAKEN_HOOK(trn);
          clear_and_set(trn);
          count_trans++;
          trn = -1;
        }
      }
    }
  }
  return count_trans;
}

void monitor_thread(){
    dataPass *d;
    printf("SAMPLE\n");
    while (1) {
      messageQMutex.lock();
      osEvent evt = messageQ.get(0);
      messageQMutex.unlock();
      if(evt.status == osEventMessage) {
        d = (dataPass*)evt.value.p;
        // USE
        printf("CHAN VAL: %d\n", d->refTime);
        free(d);
      }
    }
}

int main() { // This is the pacemaker thread after initialization
    heartbeatRecved.rise(&interruptFunction);
    set_time(1256729737);
    oldTime = time(NULL);

    syncMap[VPaceS]        = false;
    syncMap[VPaceR]        = false;
    syncMap[VSenseS]       = false;
    syncMap[VSenseR]       = false;
    syncMap[tachychardiaS] = false;
    syncMap[tachychardiaR] = false;
    syncMap[none]          = true;

    monitorThread.start(monitor_thread);

    while(1) {
      check_trans();
    }
}
