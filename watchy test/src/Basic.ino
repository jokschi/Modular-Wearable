#include <Watchy.h>

Watchy watchy;

void setup(){
  Serial.begin(9600);
  watchy.init();
}

void loop(){}