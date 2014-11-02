#!/usr/bin/python
import struct
import time
import sys
import os
import select
import signal

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
    infile      = []
    data        = []
    indexToKey  = []
    
    for key in inMap.keys() :
        infile.append(open(inMap[key], "rb"))
        data.append([]);
        indexToKey.append(key)
        
    try :
    	while runCallback() :
            try :
                read = select.select(infile, [], [], 1)[0];
            except select.error as e :
            #handle EINTR
                if e[0] == 4 :
                    continue
                else :
                    raise

	    for f in read :
                index = infile.index(f)
            #use blocking call. this should not be issue as events should come in expected size
            #EINTR ?
                event = f.read(EVENT_SIZE);
                (tv_sec, tv_usec, type, code, value) = struct.unpack(FORMAT, event)

                if type == EV_KEY and value in [KEY_PRESS, KEY_REPEAT] :
                    data[index].append(code);
                    if code in [KEYCODE_ENTER, KEYCODE_RENTER] :
                        dataCallback(indexToKey[index], data[index])
                        data[index] = [];
    finally:
        for f in infile :
            f.close()

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
	ReadKeyboardEvents({"aa":"/dev/input/event0", "bb":"/dev/input/event0"}, PrintCallback, RunCallback)
    except KeyboardInterrupt : ""
