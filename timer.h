

/****************************** DESIGN NOTES **********************

-This alarm call back library works by have two statically defined linked
list - an unsorted linked list of "free" callbacks, and a sorted linked
list of callbacks. the timerCallBack.ticks field is used to store how
far into the future an alarm needs to be raised. For example,
if we had the following request when there are no alarms stored at 
when our program has been running for 3 timer interrupts

    requestAlarm(2,foo);
    requestAlarm(4,foo);
    requestAlarm(2,foo);
    requestAlarm(5,bar);
    requestAlarm(3,baz);
   
we would go and pop 5 callbacks from our free list and create
a sorted linked list that would look like this

(5,foo) -> (5,foo) -> (8,baz) -> (9,foo) -> (10,bar)

where the numerical values in the above chain are attained by adding 3 (current time) to the
delay requested before an alarm is raised

at every timer interrupt, current time will be incremented by 1.
so in 2 timer interrupts, current time will be 5 and this will cause
foo too be popped off the head of the used linked list twice (since we queued it up twice)

once these alarms are processed, they are popped off the head of the used linked list
and stored in a "free" linked list. we don't care about sorting in this case,
we just append to the head so we can grab things out of our used
linked list in o(1) time.

Insertion in the used linked list is o(n), but since we only incur this
debt outside the IRQ this is fine, and inside the IRQ it's o(1) to check the used-link
list, since we only care about the tip and we stop going down the linked list quickly since we have
sorted it.


*******************************************************************/

#pragma once


//We are going to limit ourselves to 64 different callbacks
//since our device only has 20kB of memory and
//I believe 64 callbacks will be sufficient 
#define MAX_CALL_BACKS (64)

//used as an sentinel index value, IE, our final node in the linked-list
//will point to NULL_SENTINEL as it's "next" node index.
#define NULL_SENTINEL (0xFF)


//the stucture used to keep track of
//our callbacks
typedef struct timerCallBack{
   uint32_t  ticks;     
   uint8_t   next;
   bool      used;
   void      (*callBackFun)(void);
}timerCallBack_t;

//we will have 2 linked lists to keep track of call backs.
//one linked-list will be just to keep track of which array indexes are free,
//and another linked list will be used by the actuall call backk.
//the sum of the elements in both of these linked-list will equal MAX_CALL_BACKS
typedef struct callBackManager{
    uint8_t  usedHead;  //start of the linked-list which contains the used call-backs
    uint8_t  freeHead;  //start of the linked-list which contains a list of empty call-backs that we can use
    uint8_t  totalCallBacks;   //how many callbacks we have used.
}callBackManager_t;




//public API
void initAlarm();
void stopTimer();  //not implemented - bascailly call HalTimerStop(TIMER0);
void startTimer(); //not implemented - bascailly call HalTimerStart(TIMER0);
bool requestAlarm(uint32_t delay, void (*callBackFun)(void));

//private
static uint8_t popHeadStaticFreeLinkedList(void);
static void insertHeadStaticFreeLinkedList(uint8_t index);
static uint8_t popHeadStaticUsedLinkedList(void);
static void insertStaticLinkedList(uint32_t time, void (*callBackFun)(void));
static void intTimer0(void);
