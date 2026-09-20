#pragma once
#include <Arduino.h>

class OtaManager {
public:
  void begin();
  void loop();
private:
  bool started_ = false;
};
