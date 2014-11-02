#!/usr/bin/python
import sys, time, threading, signal, hashlib, array, traceback, logging
from daemon import Daemon
from eventwriter import RpcCommands
from gpioreader import ReadGPIOEvents
from idreader import ReadKeyboardEvents
 
class MyDaemon(Daemon):

  def continueRunCallback(self) :
    return self._run

  def Terminate(self) :
    self._run = False
  
  def publishMessageId(self, key, id) :
    m = hashlib.md5()
    m.update(array.array('B', id).tostring())
    self._commands.sendID(key, m.hexdigest())

  def publishMessageButton(self, key, time) :
    self._commands.sendButtonPress(key, time)

  def runIDReader(self) :
    while self.continueRunCallback() :
      try :
        ReadKeyboardEvents({"aa":"/dev/input/event0", 1:"/dev/input/event0"},
          self.publishMessageId, self.continueRunCallback)
      except :
        logging.error(traceback.format_exc())
        continue

  def runBtnReader(self) :
    while self.continueRunCallback() :
      try :
        ReadGPIOEvents({0:5, 1:19},
          self.publishMessageButton, self.continueRunCallback)
      except :
        logging.error(traceback.format_exc())
        continue
  
  def run(self):
    self._commands = RpcCommands("http://192.168.0.100:9090/rpc/command")
    try:
      logging.info("initialization")
      self._run = True
      #termination as daemon
      signal.signal(signal.SIGTERM, self.Terminate)
      self._idreader = threading.Thread(target=self.runIDReader)
      self._idreader.start()
      self._btnreader = threading.Thread(target=self.runBtnReader)
      self._btnreader.start()
      logging.info("started")
      
      try:
        while self.continueRunCallback() :
          time.sleep(1)         
      except KeyboardInterrupt:
        #termination from command-line session
        self.Terminate()
    except:
      logging.error(traceback.format_exc())
    finally:
      self._commands.close()
 
if __name__ == "__main__":
  daemon = MyDaemon('/tmp/rpi-wttsc-controller.pid')
  logging.basicConfig(level=logging.INFO)
  if len(sys.argv) == 2:
    if 'start' == sys.argv[1]:
      daemon.start()
    elif 'stop' == sys.argv[1]:
      daemon.stop()
    elif 'restart' == sys.argv[1]:
      daemon.restart()
    elif 'run' == sys.argv[1]:
      daemon.run()
    else:
      print "Unknown command"
      sys.exit(2)
    sys.exit(0)
  else:
    print "usage: %s start|stop|restart|run" % sys.argv[0]
    sys.exit(2)
