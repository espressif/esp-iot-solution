import SocketServer as ss
import sys
import struct
import time

class SensorDataHandler(ss.StreamRequestHandler):
    def handle(self):
        for line in self.rfile.readlines():
            print (line),
        print ("\n")

if __name__ == "__main__":
    if sys.argv[1:]:
        port = int(sys.argv[1])
    else:
        port = 8000
    server_addr = ('', port)
    sensor_ser = ss.TCPServer(server_addr, SensorDataHandler)
    sa = sensor_ser.socket.getsockname()
    print ("Serving sensor data on", sa[0], "port", sa[1], "...")
    sensor_ser.serve_forever()

