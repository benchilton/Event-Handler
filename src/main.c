#include "main.h"

#define  MICRO   1000

void eventA(argument* argc){
  printf("event A: Hello World!\n");
  printf("%s %zu \n",argc->data, argc->dataSize * 2);
}

void eventB(argument* argc){
  printf("event B: some data:\n");
  printf("%zu %f \n", *((size_t*) argc->data), *((float*) argc->data));
}

void eventC(argument* argc){
  printf("event C: All prints are occuring in the events\n");
  argc->dataSize = 1200;
  memcpy(argc->data,"sample string",20);
}

void eventD(argument* argc){
  printf("event D: <something>\n");
}

int main(void) {
  setupEvents();
  float f = 539823.1;

  argument myArg = buildArg("This is an Event",sizeof(char),20);
  argument arg2  = buildArg(&f,sizeof(float),1);

  onEvent(200,&eventA);
  onEvent(1,&eventB);
  onEvent(100,&eventC);
  trigger(200,&myArg);
  trigger(1,&arg2);
  trigger(100,&myArg);
  onEvent(2,&eventD);
  trigger(2,&myArg);

  usleep(500*1000);
  closeEvents();

  usleep(500*1000);

  deleteArg(&myArg);
  deleteArg(&arg2);
  return 0;
}
