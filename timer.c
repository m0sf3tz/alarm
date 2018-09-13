#include <stdint.h>
#include <stdbool.h>
#include <timer.h>


//varabiale to keep track of time, will
//increment at every tic
//be careful when reading this value outside of IRQ,
static volatile uint32_t timerNow = 0;

//array of callbacks
static timerCallBack_t    callBackArr[MAX_CALL_BACKS];

//array to manage callbacks
static callBackManager_t  callBackManager;


void initAlarm()
{
    //set up IRQs, Timers, interrupts, etc.. lets pretend we have timer0 we are using
 
    //HalIrqInstall(TIMER0) <- or something like this, etc...
    //HalTimerSet(TIMER0,MODE_COUNT_DOWN | AUTO_RELOAD);
    //HalTimerPeriod(TIMER0,1_MS_PERIOD); //1ms period request
    //HalTimerEnableIrq(TIMER0);
    //HalTimerStart(TIMER0);  //finally kick off the timer
    
    //Mark all of our linked-list as free
    //for simlpicity, index 0 will be the head of our free list
    
    
    //go through the and put every element in the free list
    uint8_t iterIndex = 0;
    
    while(iterIndex < MAX_CALL_BACKS)
    {
        callBackArr[iterIndex].next = iterIndex + 1;
        callBackArr[iterIndex].used = false;
        
        iterIndex++;
    }
    
    //correct the last index, set it to point to NULL value
    callBackArr[MAX_CALL_BACKS - 1].next = NULL_SENTINEL;
    
    callBackManager.freeHead       = 0;              //entire elements are free
    callBackManager.usedHead       = NULL_SENTINEL;  //nothing stored
    callBackManager.totalCallBacks = 0;
     
}

// @return - will return the index of the first free call back 
//           slot within callBackArr
static uint8_t popHeadStaticFreeLinkedList()
{
    uint8_t retVal = callBackManager.freeHead;
    
    //we popped, the head off the free array,
    //we need to update where our free head is
    callBackManager.freeHead = callBackArr[callBackManager.freeHead].next;
    
    return retVal;

}

// @param  - Index is a index that was just free, add it to the head of 
//           the free linked list
static void insertHeadStaticFreeLinkedList(uint8_t index)
{
    uint8_t oldHead = callBackManager.freeHead;
    
    callBackManager.freeHead = index;

    callBackArr[index].next = oldHead;  
}


// @return - will return the index of the first used 
//           slot within callBackArr
static uint8_t popHeadStaticUsedLinkedList()
{
    uint8_t retVal = callBackManager.usedHead;
    
    //we popped, the head off the used array,
    //we need to update where our used head is
    callBackManager.usedHead = callBackArr[callBackManager.usedHead].next;
    
    return retVal;

}

//helper function
//@param    alarmTime   - time in furetur to request alarm
//@param    callBackFun - Timer callBack function
static void insertStaticLinkedList(uint32_t time, void (*callBackFun)(void))
{
    if(callBackManager.totalCallBacks == 0)
    {

        //no callbacks installed yet
        
        //lets get a free head
        uint8_t index = popHeadStaticFreeLinkedList();
        
        callBackArr[index].next          = NULL_SENTINEL;
        callBackArr[index].callBackFun   = callBackFun;
        callBackArr[index].ticks         = time;
        
        callBackManager.usedHead = index;

    }
    else
    {
        //the code below just insterts a value in a statically defined linked list 
        
        //iterate through the used list and insert at first timer tick that is appropriate (see timer.h for details)
        uint8_t iterIndexLag  = callBackManager.usedHead;
        uint8_t iterIndexlead = callBackManager.usedHead;
        
        while(callBackArr[iterIndexlead].ticks < time && iterIndexlead != NULL_SENTINEL)
        {
            iterIndexLag  = iterIndexlead;
            iterIndexlead = callBackArr[iterIndexlead].next;

            //break, we got to end of list     
            if(iterIndexlead == NULL_SENTINEL)
            {

                break;   
            } 
        }


        //now we know where to insert our new element in the linked list 
        //lets get a free node and populate it correctly. 
        uint8_t insertIndex = popHeadStaticFreeLinkedList();
        
        printf("free index is: %d \n", insertIndex);   
        
        //insertion into last node must be taken care of by addding sentilel value
        if(iterIndexlead == NULL_SENTINEL)
        {
                callBackArr[insertIndex].next          = NULL_SENTINEL;
        }
        else
        {
                callBackArr[insertIndex].next          = callBackArr[iterIndexlead].next;
        }


        callBackArr[insertIndex].callBackFun   = callBackFun;
        callBackArr[insertIndex].ticks         = time;
        callBackArr[insertIndex].used          = true;
        //fix up the lagging node to point to the newly inserted callback node
        callBackArr[iterIndexLag].next   = insertIndex;
    }
    
    callBackManager.totalCallBacks++;
}

//timer0 IRQ handler
static void intTimer0() 
{
    timerNow++; //increment global timercounter
        
    if(callBackManager.totalCallBacks == 0)
    {
        return;  //no work to do, return
    }
    else
    {
        //could have multiple alarms in the same time quanta
        while(timerNow == callBackArr[callBackManager.usedHead].ticks && callBackManager.usedHead != NULL_SENTINEL)
        {
            callBackArr[callBackManager.usedHead].callBackFun(); //call callback
            callBackArr[callBackManager.usedHead].used = false;
            callBackManager.totalCallBacks--;
            
            uint8_t freedIndex = popHeadStaticUsedLinkedList();       
            insertHeadStaticFreeLinkedList(freedIndex);               
            callBackManager.usedHead = callBackArr[freedIndex].next;
                     
            //break, we got to end of list     
            if(callBackManager.usedHead == NULL_SENTINEL)
            {
                break;   
            }
            
        }
    }    
}


//@param    alarmTime   - time in furetur to request alarm
//@param    callBackFun - Timer callBack function
bool requestAlarm(uint32_t delay, void (*callBackFun)(void))
{
    if(NULL == callBackFun)
    {
        //nefarious failiure, without logging we would just get an interrupt
        //and try to execute code from the 0x0000_0000, not trivial to trace 
        //since sometime int the future we would end up w/ a prefetch abort assuming
        //we are on ARM
        //ASSERT(0);
        return false;
    }
    if(delay > UINT32_MAX)
    {
        //our logic will overflow in this case
        //ASSERT(0);
        return false;
    }
    if(callBackManager.totalCallBacks >= MAX_CALL_BACKS)
    {
        //can't keep track of this much call backs
        //we should catch this in design, debatable if 
        //we should ASSERT here, since it is not really an "error"
        //but it's an indication that we need to increase MAX_CALL_BACKS
        return false;
    }  
    
    
    //start critical section
    
    halIrq_disable(timer0);   //we need to do this so we can gurantee that we have atomic 
                               
                                //assumption is that this critical region is FAST, and it will be since we are only dealing with 64 long linked list at WORST.
                                //the approach that will follow will have O(N) insertion time, but 0(1) deletion count. since our 0(1) time will be in an IRQ
                                //while the o(N) time will be outside of an IRQ (I.E - here) I think this is a good comprimise.
   
    uint32_t alarmTime = timerNow + delay;  //calculate how many ticks in the future this alarm will be raised.                         
                                    
    //insert in our linked-list
    insertStaticLinkedList(alarmTime, callBackFun);
    
    //exit critical section
    halIrq_enable(timer0);   //we need to do this so we can gurantee that we have atomic 
    
    return true;
}





