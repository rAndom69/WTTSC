#!/usr/bin/python
import sys, time, threading, signal, hashlib, array, traceback, logging, logging.handlers
from daemon import Daemon
from eventwriter import RpcCommands
from gpiopollreader import ReadGPIOEvents
from idreader import ReadKeyboardEvents
import config
 
class MyDaemon(Daemon):

  def continueRunCallback(self) :
    return self._run

  def Terminate(self, _signo, _stack_frame) :
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
        ReadKeyboardEvents(config.id_reader,
          self.publishMessageId, self.continueRunCallback)
      except :
        logging.error(traceback.format_exc())
        continue

  def runBtnReader(self) :
    while self.continueRunCallback() :
      try :
        ReadGPIOEvents(config.gpio_reader,
          self.publishMessageButton, self.continueRunCallback)
      except :
        logging.error(traceback.format_exc())
        continue
  
  def run(self):
    logging.info("init")
    self._commands = RpcCommands(config.rpc_uri)
    try:
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
        self.Terminate(0, None)
    except:
      logging.error(traceback.format_exc())
    finally:
      logging.info("shutdown")
      self._commands.close()
 
if __name__ == "__main__":
  logger = logging.getLogger('')
  logger.setLevel(logging.DEBUG)
  handler = logging.handlers.RotatingFileHandler('rpi-wttsc-controller.log',  maxBytes=1048576, backupCount=1)
#  handler = logging.StreamHandler()
  handler.setFormatter(logging.Formatter("%(asctime)s %(message)s"))
  logger.addHandler(handler)
  logger.info("create daemon")
  daemon = MyDaemon('/tmp/rpi-wttsc-controller.pid')
#logging.basicConfig(format='%(asctime)s %(message)s', filename='rpi-wttsc-controller.log', level=logging.DEBUG)
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
