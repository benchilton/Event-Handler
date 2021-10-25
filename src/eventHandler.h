#ifndef EVENTHANLDER_HEADER
#define EVENTHANLDER_HEADER

/****************************** - Library Includes - *******************************/
/******************************** - User Includes - ********************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
//We need the event loop to run in a separte thread so we can do stuff while it is running
#include <pthread.h>
#include <unistd.h>
/*********************************** - Defines - ***********************************/

#define EVENT_MAX_PRIORITY (-1)

/************************************ - Enums - ************************************/
/********************************** - Typedefs - ***********************************/
/********************************* - Structures - **********************************/

typedef union _eventFlags {
  unsigned char group : 8;
  struct {
    unsigned char installed : 1;
    unsigned char paused    : 1;
    unsigned char quit      : 1;
    unsigned char running   : 1;
    unsigned char bit4      : 1;
    unsigned char bit5      : 1;
    unsigned char bit6      : 1;
    unsigned char bit7      : 1;
  };
}eventFlags_t;

typedef eventFlags_t* eventContext_t;

typedef struct _arguments {
  size_t          dataSize;
  size_t          length;
  pthread_mutex_t mutex;
  void*           data;
}internal_argument;

typedef internal_argument* argument_t;

/*Functions that the event runs*/
typedef void (*eventHandle_t)(eventContext_t this, argument_t arg);

/**************************** - Function Prototypes - ******************************/

argument_t event_buildArgument(void* data, size_t numberOfElements, size_t sizeOfData);
void       event_destroyArgument(argument_t toDelete);

bool       event_installDriver();
bool       event_deinstallDriver();

bool       event_onEvent(size_t id, eventHandle_t event);
bool       event_trigger(size_t id, size_t priority, argument_t arg);

bool       event_isHandleInstalled();

void       event_pause();
void       event_resume();

/****************************** - Global Variables - *******************************/

#endif
