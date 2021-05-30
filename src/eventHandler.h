#ifndef EVENTHANLDER_HEADER
#define EVENTHANLDER_HEADER

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//We need the event loop to run in a separte thread so we can do stuff while it is running
#include <pthread.h>
#include <unistd.h>

void* doNothing(void *p);

typedef struct _arguements {
  size_t  dataSize;
  size_t  length;
  void*   data;
}argument;

typedef struct _handlerCase {
  size_t            id;
  void              (*funct)(argument*);//our function pointer
}handlerCase;

typedef struct _eventCase {
  size_t            id;
  argument*        arg;
}eventCase;

typedef struct _event {
  eventCase*    container;
  //Number of elements in the event queue
  size_t        length;
  unsigned char pause;
  unsigned char nQuit;
}event;

typedef struct _handler {
  handlerCase* container;
  //Number of elements in the event queue
  size_t       length;
}handler;

argument       nullArg;
//Define the polls and listener lists
event           poll;
handler         listener;
//Define the thread in which the event loop occurs in
pthread_t       eventThread;
//Define the mutex for the thread to prevent undefined behaviour.

void deleteArg(argument* inArg);
argument buildArg(void* inData ,size_t inSize, size_t inElements);

//Functions used to handler events

//Start the event handler
void setupEvents();
//Stop the event handler
void closeEvents();
//Define a new event for the event handler to handle
void onEvent(size_t id, void* func);
//Trigger the event that has the corresponding id with the passed arguements
void trigger(size_t id, argument* argc);


// function protoypes
// Internal Functions not to be used else where.
void e_pushBack(event* e, size_t inId, argument* inArg);
void h_pushBack(handler* h, size_t inId, void* inFunct);

void e_popBack(event* e);
void h_popBack(handler* h);

void e_popForward(event* e);

void addEventToPoll(event e);
void addHandlerToListener(handler h);
void processEvent();
void startEventLoop();

//End the event Loop
void quitEvents();
//Pause the event queue
void pauseEvents();
//Continue the event queue
void resumeEvents();

//
//
//
//

#endif
