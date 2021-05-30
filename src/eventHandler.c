/*
This code implements a simple event handler where event types can be added to a listerner
Then events can be triggered with argument to be passed.
The code implements the event handler in a separate thread.
*/
#include "eventHandler.h"

//This is a null arguement.
argument nullArg = {0,0};
//Mutex and Conditional Variable for the Event Thread
pthread_mutex_t eLock     = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  eventLock = PTHREAD_COND_INITIALIZER;
//Mutex and Conditional Variable for the Main Thread
pthread_mutex_t mLock     = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  mainLock  = PTHREAD_COND_INITIALIZER;

//Starting with functions to work on vectors:

//The below functions are these to add elements to the vector in which the event queue is held in
void e_pushBack(event* e, size_t inId, argument* inArg) {
  size_t     newLength = e->length + 1;
  eventCase* newCase = realloc(e->container,newLength*sizeof(eventCase));
  if(!newCase) return;
  e->container = newCase;
  e->container[newLength - 1].id  = inId;
  e->container[newLength - 1].arg = inArg;
  e->length = newLength;
}

void h_pushBack(handler* h, size_t inId, void* inFunct) {
  size_t       newLength = h->length + 1;
  handlerCase* newCase   = realloc(h->container,newLength*sizeof(handlerCase));
  if(!newCase) return;
  h->container = newCase;
  h->container[newLength - 1].id    = inId;
  h->container[newLength - 1].funct = inFunct;
  h->length = newLength;
}

void e_popBack(event* e) {
  if(e->length == 0) return;
  size_t newLength = e->length - 1;
  eventCase* newCase = realloc(e->container,newLength*sizeof(eventCase));
  if(!newCase) return;
  e->container = newCase;
  e->length = newLength;
}

void h_popBack(handler* h) {
  if(h->length == 0) return;
  size_t newLength = h->length - 1;
  handlerCase* newCase = realloc(h->container,newLength*sizeof(handlerCase));
  if(!newCase) return;
  h->container = newCase;
  h->length = newLength;
}
//removes the first element of the vector.
//This function is hella inefficient using dynamically allocated arrays.
//Could rewrite to use a linked list but probably slower
void e_popForward(event* e) {
  //If the vector has no length then we cannot popFoward so just return
  if(!e->length) return;
  //Define the new length of the vector
  size_t     newLength = e->length - 1;
  //Simply the length of the vector in bytes
  size_t     byteSize = sizeof(eventCase)*newLength;
  //Make a pointer to the memory of the event
  eventCase* ptr = e->container;
  //Copy the (i+1) th address to the i th address
  memcpy(ptr,e->container+1,byteSize);
  //Realloc to reduce the size, note that when reducing memory size realloc truncates the
  //end of the memory and thus we remove the last element with the realloc.
  eventCase* newCase = realloc(e->container,byteSize);
  //Ensure the realloc was successful
  if(!newCase) return;
  //Update event parameters
  e->container = newCase;
  e->length = newLength;
}

//Functions that operate on arguements:
//Builds an argument, note due to the void* we can store any datatype in the argument
argument buildArg(void* inData ,size_t inSize, size_t inElements) {
  argument built;
  size_t byteSize = inSize * inElements;
  built.dataSize = inSize;
  built.length = inElements;
  built.data     = malloc(byteSize);
  memcpy(built.data,inData,byteSize);
  return built;
}

void deleteArg(argument* inArg) {
  free(inArg->data);
}

//Now getting to functions that operate on the event queue:

//Call this at the start of every program where you want to use events
void setupEvents() {
  //Initialise the poll and listener variables
  poll.container     = malloc(0);//(eventCase*) malloc(sizeof(eventCase));
  listener.container = malloc(0);//(handlerCase*) malloc(sizeof(handlerCase));
  //We have added an element so increase the number of elements we have
  poll.length = listener.length = 0;
  //Start the event queue loop
  startEventLoop();
  //Pause the main thread until the event loop starts
  pthread_cond_wait(&mainLock,&mLock);
}
//Used to end the events and destroy anyhting assosiated with the events
void closeEvents() {
  //Stop the event loop
  quitEvents();
  //Free memory used by the poll and listener
  free(poll.container);
  free(listener.container);
}
//Trigger an Event by adding it to the poll
void trigger(size_t id, argument* argc) {
  resumeEvents();//If events have been paused then resume them ready for the event we are calling
  e_pushBack(&poll,id,argc);
}
//define a new event type by adding it to the listener
void onEvent(size_t id, void* func) {
  //While adding events we need to pause
  pauseEvents();
  h_pushBack(&listener,id,func);
  resumeEvents();
}
//Used to process each events
void processEvent() {
  if(poll.length == 0) return;
  //get the event at the head of the poll

  eventCase e = poll.container[0];
  void (*exe)(argument*);//our function pointer
  //Scan the listener for an event that matches the trigger
  for (size_t i = 0; i < listener.length; i++) {
    //If we find the match then copy the function pointer stored in the triggered event
    //And then execute the event with the desired argument
    //Once the event has been processed then remove it from the poll.
    if(e.id == listener.container[i].id) {
      exe = listener.container[i].funct;
      exe(e.arg);
      e_popForward(&poll);
    }
  }
}

//This is the thread which contains the event processing
void* eventLoop(void* arg) {
  poll.pause = 1;//We start off by indicating we want to pause the event queue
  //Setup the thread condition variable
  pthread_cond_init(&eventLock,NULL);
  //setup the thread mutex
  pthread_mutex_init(&eLock,NULL);
  //Indicate that if the code has reached this point the event queue Thread has
  //Setup successfully and thus the main code can continue.
  pthread_cond_broadcast(&mainLock);
  //Pause the event queue until the main thread gives us the go ahead.
  pthread_cond_wait(&eventLock,&eLock);
  //Hold the thread until we got the go ehead at least 1 event listerer has
  //Been created.
  while (poll.nQuit) {
    if(poll.pause)  {pthread_cond_wait(&eventLock,&eLock);}
    //if(poll.length) {processEvent();}
    processEvent();
  }
  //If we have reached this point then the event loop wants to be shutdown
  pthread_exit(&eventThread);
}

void startEventLoop() {
  //Initialise the flag variables
  poll.pause = 0;
  poll.nQuit = 1;
  //Create the event loop function in the eventThread
  pthread_create(&eventThread,NULL,eventLoop,NULL);
}

//A function that does nothing if the event runs into an error
void* doNothing(void *p) {return NULL;}
//Call this to exit the event queue
void quitEvents() {
  poll.nQuit = 0;
}
//Call this to pause the event queue
void pauseEvents() {
  poll.pause = 1;
}
//Call this to resume the event queue.
void resumeEvents() {
  pthread_cond_broadcast(&eventLock);
  poll.pause = 0;
}
