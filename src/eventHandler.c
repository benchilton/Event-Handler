/****************************** - Library Includes - *******************************/
/******************************** - User Includes - ********************************/
#include "eventHandler.h"
/*********************************** - Defines - ***********************************/
/************************************ - Enums - ************************************/
/********************************** - Typedefs - ***********************************/
/********************************* - Structures - **********************************/

typedef struct _handlerCase_t {
  size_t      id;
  eventHandle_t func;//our function pointer
}handlerCase_t;

typedef struct _eventCase_t {
  size_t      priority;
  size_t      id;
  argument_t  arg;
}eventCase_t;

typedef struct _event {
  eventCase_t*  container;
  //Number of elements in the event queue
  size_t      length;
}event_t;

typedef struct _handler {
  handlerCase_t* container;
  //Number of elements in the event queue
  size_t       length;
}handler_t;

typedef struct _eventThread
{
  pthread_t       thread;
  pthread_cond_t  condition;
  pthread_mutex_t mutex;
  eventFlags_t    flags;
  argument_t      arg;
  eventHandle_t   func;
}eventThread_t;

/**************************** - Function Prototypes - ******************************/

static void*       event_handler_thread();

static bool        event_add_to_list(eventCase_t eventToAdd);
static bool        event_to_remove(size_t indexToRemove);
static bool        event_popback();

static bool        handler_add_to_list(handlerCase_t handlerToAdd);
static bool        handler_to_remove(size_t indexToRemove);
static bool        handler_popback();

static inline void event_trigger_deletion(pthread_cond_t* condition, eventThread_t* eventThreads);
static size_t      event_get_highest_priority();
static void        event_process(eventThread_t* threads);
static void        event_thread_create(eventThread_t* thread, eventHandle_t func, argument_t arg);
static void*       event_thread_spawner(void* arg);

/********************************* - Constants - ***********************************/
/********************************* - Variables - ***********************************/
//This holds the thread the event listener
static pthread_t       eventThread;
static pthread_cond_t  eventCondition;//Holds the condition of the event thread
static pthread_mutex_t eventMutex;
//Holds the condition of the calling thread, (whatever is calling stuff like deinstallEvent)
static pthread_mutex_t callerMutex;
static pthread_cond_t  callerCondition;
//Events to be triggered are added to this list
static event_t         poll;
//The list of what an event runs is stored in this list
static handler_t       listener;

static bool            eventHandleInstalled = false;
/*This needs to be volatile as there are multiple threads that access this boolean*/
static volatile bool   eventThreadDeletion = false;
static volatile bool   eventThreadPause    = false;

static volatile size_t numActiveEvents = 0;

/***************************** - Public Functions - ********************************/

/*Functions that build and destroy arguments for events*/
argument_t event_buildArgument(void* data, size_t numberOfElements, size_t sizeOfData) {
  /*Create the new argument*/
  argument_t builtArg;
  /*Compute the size in bytes of the argument's data*/
  size_t   size = numberOfElements * sizeOfData;
  /*Allocate the memory needed for the argument data*/
  builtArg      = malloc(1 * sizeof(internal_argument));
  /*Allocate memory for the pointer to the argument*/
  builtArg->data = malloc(size);
  /*Copy the passed data into the argument's storage*/
  memcpy(builtArg->data, data, size);
  /*Set the internal parameters of the argument*/
  builtArg->dataSize = sizeOfData;
  builtArg->length   = numberOfElements;
  /*Init the mutex*/
  pthread_mutex_init(&builtArg->mutex, NULL);

  return builtArg;
}

void event_destroyArgument(argument_t toDelete) {
  /*Destroy the argument's mutex*/
  pthread_mutex_destroy(&toDelete->mutex);
  /*Free up the allocate memory used to store the data*/
  free(toDelete->data);
  /*Free up the pointer to the argument*/
  free(toDelete);
}

/*Functions that install and deinstall the event handler*/

bool event_installDriver() {
  /*Initialise poll and listener objects*/
  poll.container     = malloc(0);
  poll.length        = 0;
  listener.container = malloc(0);
  listener.length    = 0;
  /*Create event thread*/
  pthread_create(&eventThread, NULL, event_handler_thread, NULL);
  /*Set up the conditional variables and mutexes for the caller and event threads*/
  pthread_mutex_init(&eventMutex, NULL);
  pthread_mutex_init(&callerMutex, NULL);
  pthread_cond_init(&eventCondition, NULL);
  pthread_cond_init(&callerCondition, NULL);
  /*We have allocated memory for the event handler and successfully created a thread for it*/
  eventHandleInstalled = true;
  /*We have spawned a new thread, wait here until the thread has began and is functionals*/
  pthread_cond_wait(&callerCondition,&callerMutex);

  return eventHandleInstalled;
}

bool event_deinstallDriver() {
  /*Say that we want to delete the event queue*/
  eventThreadDeletion = true;
  /*Delete event thread*/
  if(eventThreadPause == true)
  {
    pthread_cond_signal(&eventCondition);
    eventThreadPause = false;
  }
  /*The event thread is not in a cancelable state until we free up any resources needed by the thread*/
  pthread_cancel(eventThread);
  /*Halt the caller thread while we move to delete the event thread*/
  pthread_cond_wait(&callerCondition, &callerMutex);

  pthread_cond_destroy(&eventCondition);
  pthread_cond_destroy(&callerCondition);
  pthread_mutex_destroy(&eventMutex);
  pthread_mutex_destroy(&callerMutex);

  poll.length = 0;
  listener.length = 0;
  free(poll.container);
  free(listener.container);
  eventHandleInstalled = false;

  return eventHandleInstalled;
}

bool event_isHandleInstalled() {
  return eventHandleInstalled;
}

bool event_onEvent(size_t id, eventHandle_t event) {
  handlerCase_t newHandle;
  newHandle.id   = id;
  newHandle.func = event;

  bool retVal = handler_add_to_list(newHandle);

  return retVal;
}

bool event_trigger(size_t id, size_t priority, argument_t arg) {
  eventCase_t newEvent;
  newEvent.id       = id;
  newEvent.priority = priority;
  newEvent.arg      = arg;

  bool retVal = event_add_to_list(newEvent);

  return retVal;
}

void event_pause() {
  eventThreadPause = true;
}

void event_resume() {
  eventThreadPause = false;
  pthread_cond_signal(&eventCondition);
}

/***************************** - Private Functions - *******************************/

/*Event thread*/
static void* event_handler_thread() {
  /*Set up some event stuff*/
  //Make the thread uncancelable
  pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
  size_t         highestPriority = 0;
  size_t         i = 0;
  eventThread_t* activeEvents = malloc(0);
  eventCase_t    event;
  handlerCase_t  handle;
  eventHandle_t  exe;
  /*Signal to the caller thread that the this thread has successfully set itself up */
  pthread_cond_signal(&callerCondition);

  while ( true ) {
    if(eventThreadPause == true) {pthread_cond_wait(&eventCondition, &eventMutex);}
    /*First thing we do is check if the event thread needs deleting*/
    if( eventThreadDeletion == true ) {
      event_trigger_deletion(&callerCondition, activeEvents);
    }
    event_process(activeEvents);
  }
  return NULL;
}

static void event_process(eventThread_t* threads) {
  if( poll.length != 0 ) {
    size_t handleToUse     = 0;
    size_t highestPriority = event_get_highest_priority();
    eventCase_t event        = poll.container[highestPriority];
    eventHandle_t exe;
    for (size_t idx = 0; idx < listener.length; idx++) {
      if( listener.container[idx].id == event.id ) {
        handleToUse = idx;
      }
    }
    /*We are about to spawn a new event in a new thread so increment the number active threads*/
    exe = listener.container[handleToUse].func;

    numActiveEvents++;
    threads = realloc(threads, sizeof(eventThread_t) * numActiveEvents);
    event_thread_create(&threads[numActiveEvents - 1], exe, event.arg);

    event_to_remove(highestPriority);
  }
}

static void event_thread_create(eventThread_t* thread, eventHandle_t func, argument_t arg) {
  pthread_mutex_init(&thread->mutex, NULL);
  pthread_cond_init(&thread->condition, NULL);
  /*Initialise the thread flags*/
  thread->flags.installed = false;
  thread->flags.paused    = false;
  thread->flags.quit      = false;
  thread->arg             = arg;
  thread->func            = func;
  /*We start the thread with a wrapper function that does thread setting up then runs the desired thread*/
  pthread_create(&thread->thread, NULL, event_thread_spawner, (void*) thread);
  pthread_cond_wait(&thread->condition,&thread->mutex);
}

static void event_thread_delete(eventThread_t* thread) {
  /*Clean up resources used by the thread*/
  pthread_mutex_destroy(&thread->mutex);
  pthread_cond_destroy(&thread->condition);
  numActiveEvents = numActiveEvents - 1;
  /*Delete our thread now*/
  pthread_exit(NULL);
}

/*This function is a wrapper for the event thread that does some setting up that need to be done*/
static void* event_thread_spawner(void* arg) {
  eventThread_t* thread = (eventThread_t*) arg;
  pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

  /*Thread successfully */
  pthread_cond_signal(&thread->condition);

  thread->flags.running   = true;
  thread->flags.installed = true;

  /*Lock the mutex for the argument passed in as we want to prevent other events from operating
    operating on the same argument passed in.*/
  pthread_mutex_lock(&thread->arg->mutex);
  /*Function the function the thread should execute*/
  thread->func( &thread->flags ,thread->arg );

  pthread_mutex_unlock(&thread->arg->mutex);

  event_thread_delete(thread);

  return NULL;
}

static size_t event_get_highest_priority() {
  size_t currentPos = 0;
  size_t currentMax = 0;
  eventCase_t* event  = poll.container;
  for (size_t i = 0; i < poll.length; i++) {
    if (event[i].priority > currentMax) {
      currentMax = event[i].priority;
      currentPos = i;
    }
  }
  return currentPos;
}
/*
This should be the last function called in the main event
*/
static inline void event_trigger_deletion(pthread_cond_t* condition, eventThread_t* eventThreads) {

  for (size_t i = 0; i < numActiveEvents; i++) {
    eventThreads[i].flags.running = false;
  }

  while (numActiveEvents != 0) {}

  pthread_cond_signal(condition);
  pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
}
/*
Appends the poll with the eventCase_t passed in
Note we need to pause the event thread while this occurs
*/
static bool event_add_to_list(eventCase_t eventToAdd) {

  bool   success = false;
  size_t newLength = poll.length + 1;

  eventCase_t* newContainer = realloc(poll.container, sizeof(eventCase_t) * newLength);
  if( NULL == newContainer) {
    success = false;
  }
  else {
    poll.container = newContainer;
    poll.length    = newLength;
    /*Copy the event onto the poll list*/
    memcpy( &poll.container[poll.length - 1] , &eventToAdd , sizeof(eventCase_t) );
    success = true;
  }

  return success;
}

static bool event_to_remove(size_t indexToRemove) {
  if( (poll.length == 0) || (indexToRemove > poll.length) ) {return true;}
  if(indexToRemove == poll.length) {return event_popback();}

  bool   success = false;
  size_t remainingElements = poll.length - indexToRemove;
  size_t newLength = poll.length - 1;

  for (size_t i = indexToRemove; i < poll.length - 1; i++) {
    memcpy( (poll.container + i) , (poll.container + i + 1) , sizeof(eventCase_t));
  }
  eventCase_t* newContainer = realloc(poll.container, newLength * sizeof(eventCase_t));

  if( NULL == newContainer) {
    success = false;
  }
  else {
    poll.container = newContainer;
    poll.length    = newLength;
    success = true;
  }

  return success;
}

static bool event_popback() {
  size_t newLength = poll.length - 1;
  bool   success   = false;
  eventCase_t* newContainer = realloc(poll.container, newLength * sizeof(eventCase_t));
  if( NULL == newContainer) {
    success = false;
  }
  else {
    poll.container = newContainer;
    poll.length    = newLength;
    success = true;
  }
  return success;
}

static bool handler_add_to_list(handlerCase_t handlerToAdd) {
  bool   success = false;
  size_t newLength = listener.length + 1;

  handlerCase_t* newContainer = realloc(listener.container, sizeof(handlerCase_t) * newLength);
  if( NULL == newContainer) {
    success = false;
  }
  else {
    listener.container = newContainer;
    listener.length    = newLength;
    /*Copy the event onto the poll list*/
    memcpy( &listener.container[listener.length - 1] , &handlerToAdd , sizeof(handlerCase_t) );
    success = true;
  }

  return success;
}

static bool handler_to_remove(size_t indexToRemove) {
  if( (listener.length == 0) || (indexToRemove > listener.length) ) return true;
  if(indexToRemove == listener.length) {return handler_popback();}

  bool   success = false;
  size_t remainingElements = listener.length - indexToRemove;
  size_t newLength = listener.length - 1;

  memmove( &listener.container[indexToRemove - 1] , &listener.container[indexToRemove] , remainingElements * sizeof(handlerCase_t));

  handlerCase_t* newContainer = realloc(listener.container, newLength * sizeof(handlerCase_t));
  if( NULL == newContainer) {
    success = false;
  }
  else {
    listener.container = newContainer;
    listener.length    = newLength;
    success = true;
  }
  return success;
}

static bool handler_popback() {
  size_t newLength = listener.length - 1;
  bool   success   = false;
  handlerCase_t* newContainer = realloc(listener.container, newLength * sizeof(handlerCase_t));
  if( NULL == newContainer) {
    success = false;
  }
  else {
    listener.container = newContainer;
    listener.length    = newLength;
    success = true;
  }
  return success;
}
