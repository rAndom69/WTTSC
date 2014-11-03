#!/usr/bin/python
import time, signal, logging
import RPi.GPIO as GPIO

DEBOUNCE_MILISEC = 20   #debounce time in miliseconds

def time_ms() :
  return time.time() * 1000

def ReadGPIOEvents(inMap, dataCallback, runCallback) :

  class InputPin :
    def __init__(self, pin, key, dataCallback) :
      self._pin = pin;
      self._key = key;
      self._time = 0;
      self._dataCallback = dataCallback
      GPIO.setup(self._pin, GPIO.IN, pull_up_down=GPIO.PUD_DOWN)
      GPIO.add_event_detect(self._pin, GPIO.BOTH, callback=self.handleStateChange, bouncetime=DEBOUNCE_MILISEC);

    def close(self) :
      GPIO.remove_event_detect(self._pin)

    #invoke callback when button was pressed
    def handleStateChange(self,pin) :
      #we should already get stable state here as GPIO should do debouncing
      #TODO check on real thing to find out how debouncing of GPIO works...
      #if it only sends first event and then ignores them we're fucked and this won't work well and we'll need to do our own debouncing and wait thread      
      if not GPIO.input(self._pin) :
        self._time = time_ms()
      else :
        if self._time != 0 :
          self._dataCallback(self._key, time_ms() - self._time)
          self._time = 0

  logging.debug("gpioreader initialization")
  inputPins = [];
  try :
    GPIO.setmode(GPIO.BOARD)

    for key in inMap.keys() :
      inputPins.append(InputPin(inMap[key], key, dataCallback));

    while runCallback() :
      time.sleep(1)

  finally:
    logging.debug("gpioreader shutdown")
    for pin in inputPins :
      pin.close()
    GPIO.cleanup()

#debug mode
if __name__ == "__main__" :
  try :
    RUN = True

    def PrintCallback(key, time) :
      print "Button", key, "pressed for", time,"ms"

    def RunCallback() :
      global RUN
      return RUN

    def Terminate(_signo, _stack_frame) :
      global RUN
      RUN = False

    signal.signal(signal.SIGTERM, Terminate)
    ReadGPIOEvents({0:5, 1:19}, PrintCallback, RunCallback)
  except KeyboardInterrupt : ""
  
