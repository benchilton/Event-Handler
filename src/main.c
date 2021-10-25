#include "main.h"

#define  MICRO   1000

void eventA(eventContext_t this, argument_t argc){
  printf("event A: Hello World!\n");
  printf("%s\n", (char*)argc->data);
  strcpy((char*) argc->data, "New string XD");
  printf("%s\n", (char*)argc->data);

}

void eventB(eventContext_t this, argument_t argc){
  printf("event B: Hello World!\n");
  size_t len = argc->length + 1;
  char buffer[len];
  strcpy(buffer,argc->data);
  printf("%s\n", buffer);
}

void eventC(eventContext_t this, argument_t argc) {
  printf("Here sir in eventC\n");
  while ( this->running == 1 ) {
    printf("Loop\n");
    usleep(50*1000);
  }
}

void eventD(eventContext_t this, argument_t argc) {
  size_t** increment = (size_t**) argc->data;
  size_t*  val = *increment;
  while (this->running == 1) {
    usleep(50*1000);
    printf("%zu\n", *val);
    if( *val == 20 ) {
      printf("Increment == 20\n");
    }
  }
}

int main(void) {

  char* str = "Here is a string";
  argument_t argA = event_buildArgument((void*) str , strlen(str) + 1 , sizeof(char) );
  size_t increment = 0;
  size_t* incPtr = &increment;

  argument_t arg  = event_buildArgument(&incPtr, 1, sizeof(size_t*));

  event_installDriver();

  event_onEvent(0, eventA);
  event_onEvent(1, eventB);
  event_onEvent(2, eventC);
  event_onEvent(3, eventD);

  event_pause();

  event_trigger(0, 10,  argA);
  event_trigger(1, 12,  argA);
  event_trigger(2, 200, argA);
  event_trigger(3, 200, arg);

  event_resume();

  sleep(1);

  for (size_t i = 0; i < 30; i++) {
    increment++;
    usleep(50*1000);
  }

  sleep(1);

  event_deinstallDriver();

  event_destroyArgument(argA);
  event_destroyArgument(arg);

  return 0;
}
