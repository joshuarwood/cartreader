
import glob
import time
import serial

class Arduino:

    def __init__(self, port=None, baud=9600, timeout=0.5):
        """ Constructor """
        self.port = port if port else glob.glob("/dev/cu.usbmodem*")[0]
        self.baud = baud
        self.timeout = timeout
        self.connect()

    def open(self):
        """ Open the serial port """
        print("Opening %s" % self.port)

        self.sp = serial.Serial(
            port=self.port, baudrate=self.baud, timeout=self.timeout)

        #time.sleep(0.1)


    def read(self, num_bytes, decode_type=None):
        """ Read contents with sanity checks """
        rsp = self.sp.read(num_bytes + 2) # read bytes + 1 header byte and 1 tail byte
        #print(rsp)

        if len(rsp) == 0 or chr(rsp[0]) != '#' or chr(rsp[-1]) != '$':
            raise ValueError(f"Bad response {rsp}")
        if decode_type is not None:
            return rsp.decode(decode_type)
        return rsp[1:-1]

    def write(self, cmd, binary=False):
        """ Write a command """
        if not binary:
            return self.sp.write(bytes(cmd, 'ascii'))
        return self.sp.write(cmd)

    def connect(self):
        """ Initiate serial connection """
        # read counter
        cnt = 0
        t0 = time.time()

        # open the connection
        self.open()

        while True:

            # receive message
            message = self.sp.read(6);
            print(message)
            # break for successful connection
            if "#INIT$" in message.decode('ascii'):
                break

            # readout didn't initialize properly
            # if cnt > 5, retry
            cnt = cnt + 1
            if cnt > 5:
                print("  - retrying")

                del self.sp
                #time.sleep(0.1)
                self.open()

                # reset the counter
                cnt = 0

        # clean up
        self.sp.flushInput()
 
        #time.sleep(0.1)
        print("  - opened in %.2f sec" % (time.time() - t0))
