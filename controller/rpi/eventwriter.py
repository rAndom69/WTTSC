#!/usr/bin/python
import requests, json, threading, Queue, time, logging, socket, traceback, logging

#monkey patch no delay for sockets (we're sending little data so nagle would be counterproductive). shold help some
original_connect = socket.socket.connect
def monkey_connect(self, address):
    result = original_connect(self, address)
    try :
        self.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
    except :
        logging.error(traceback.format_exc())
socket.socket.connect = monkey_connect

#commands are delivered to server asynchronously
class RpcCommands :
    def __init__(self, uri) :
        logging.debug("rpccommands initialization")
        self._quit = False;
        self._uri = uri
        self._queue = Queue.Queue()
        self._thread = threading.Thread(target=self.worker)
        self._thread.start()

    def close(self) :
        logging.debug("rpccommands shutdown")
        self._quit = True
        self._thread.join()

    def sendCommand(self, command) :
        message = json.dumps({"command":command})
        logging.debug("command queud %s" % message)
        self._queue.put(message)

    def sendButtonPress(self, index, time) :
        self.sendCommand({"Press":{"Side":index, "Time":time}});

    def sendID(self, index, ID) :
        self.sendCommand({"ID":{"Side":index, "ID":ID}});

    #worker terminates without emptying queue (not really important to do it anyway)
    def worker(self) :
        s = requests.session()  #doh. connection pooling only works within session
        while not self._quit :
            try :
                message = self._queue.get(True, 1)
                s.put(self._uri, data=message, timeout=1)
                logging.debug("command sent %s" % message)
            except Queue.Empty :
                continue
            except :
                logging.error(traceback.format_exc())
                continue


#debug mode
if __name__ == "__main__" :
    try :
        commands = RpcCommands("http://192.168.0.100:9090/rpc/command")
        for idx in range(0,1000) :
            commands.sendButtonPress(0, 500)
        commands.sendButtonPress(0, 7000)
        time.sleep(10);
    finally :
        commands.close();
