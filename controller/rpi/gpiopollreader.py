#!/usr/bin/python
import time, signal, logging
import RPi.GPIO as GPIO

DEBOUNCE_MILISEC = 20   #debounce time in miliseconds (NOTE: button press will be delayed by this time...)
POLL_MILISEC = 5		#poll time in miliseconds

ON = GPIO.LOW
OFF = GPIO.HIGH

def time_ms() :
  return time.time() * 1000

def ReadGPIOEvents(inMap, dataCallback, runCallback) :

  class InputPin :
    def __init__(self, pin, key, dataCallback) :
      self._pin = pin;
      self._key = key;
      self._time = 0;
      self._dataCallback = dataCallback
      self._state = OFF
      self._lastState = OFF
      self._timeDebounce = 0
      GPIO.setup(self._pin, GPIO.IN, pull_up_down=GPIO.PUD_DOWN)

    def poll(self) :
      state = GPIO.input(self._pin)
      if state != self._lastState :
        self._timeDebounce = time_ms()
      if (time_ms() - self._timeDebounce) >= DEBOUNCE_MILISEC :
        if state != self._state :
          self._state = state
#button was pressed. store press time
          if state == ON :
            self._time = self._timeDebounce
          if state == OFF :
#button was unpressed. send button press
            self._dataCallback(self._key, self._timeDebounce - self._time)
      self._lastState = state  

  logging.debug("gpiopollreader initialization")
  inputPins = [];
  try :
    GPIO.setmode(GPIO.BOARD)

    for key in inMap.keys() :
      inputPins.append(InputPin(inMap[key], key, dataCallback));

    while runCallback() :
      for pin in inputPins :
        pin.poll();
      time.sleep(POLL_MILISEC * 0.001)

  finally:
    logging.debug("gpiopollreader shutdown")
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
  
