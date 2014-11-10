#!/usr/bin/python
import struct, time, sys, os, select, signal, logging

#long int, long int, unsigned short, unsigned short, unsigned int
FORMAT      = 'llHHI'
EVENT_SIZE  = struct.calcsize(FORMAT)
EV_KEY      = 1
KEY_RELEASE = 0
KEY_PRESS   = 1
KEY_REPEAT  = 2

KEYCODE_ENTER   = 28
KEYCODE_RENTER  = 96

#read keyboard events (from multiple input devices) and deliver keycodes to dataCallback
def ReadKeyboardEvents(inMap, dataCallback, runCallback) :
  logging.debug("idreader initialization")

  class InputEventReader :
    def __init__(self, device, key, dataCallback) :
      self._device = device
      self._key = key
      self._fileno = os.open(device, os.O_RDONLY | os.O_NONBLOCK)
      self._buf = ""
      self._data = []

    def read(self) :
      self._buf += os.read(self._fileno, 1024)
      while (len(self._buf) >= EVENT_SIZE) :
        event = self._buf[:EVENT_SIZE]
        self._buf = self._buf[EVENT_SIZE:]
        (tv_sec, tv_usec, type, code, value) = struct.unpack(FORMAT, event)

        if type == EV_KEY and value in [KEY_PRESS, KEY_REPEAT] :
          self._data.append(code);
          print code
          if code in [KEYCODE_ENTER, KEYCODE_RENTER] :
            dataCallback(self._key, self._data)
            self._data = [];

    def fileno(self) :
      return self._fileno

    def close(self) :
      os.close(self._fileno)
      self._fileno = -1

  pollFilesToInputMap = {}
  try :
    poll = select.poll()
    pollFilesToInputMap = {}
        
    for key in inMap.keys() :
      inputDevice = InputEventReader(inMap[key], key, dataCallback)
      pollFilesToInputMap[inputDevice.fileno()] = inputDevice
      poll.register(inputDevice.fileno(), select.POLLIN | select.POLLPRI)
        
    while runCallback() :
      try :
        for x, e in poll.poll(1000) :
          inputDevice = pollFilesToInputMap[x]
          inputDevice.read()
      except select.error as e :
      #handle EINTR which would otherwise terminate us
        if e[0] == 4 :
          continue
        else :
          raise
        
  finally:
    logging.debug("idreader shutdown")
    for inputDevice in pollFilesToInputMap.values() :
      inputDevice.close()
        

#debug mode
if __name__ == "__main__" :
  try :
    RUN = True

    def PrintCallback(key, data) :
      print key, data

    def RunCallback() :
      global RUN
      return RUN

    def Terminate(_signo, _stack_frame) :
      global RUN
      RUN = False;

    signal.signal(signal.SIGTERM, Terminate)
    ReadKeyboardEvents({0:"/dev/input/event0"}, PrintCallback, RunCallback)
    
  except KeyboardInterrupt : ""
