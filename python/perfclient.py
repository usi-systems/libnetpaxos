#!/usr/bin/env python

from twisted.internet.protocol import DatagramProtocol
from twisted.internet import reactor
import datetime

class Latency (DatagramProtocol):

    def __init__(self,):
        self.host = '192.168.1.16'
        self.port = 34952
        self.count = 0
        self.f = open('latency.txt', 'w')
    def startProtocol(self):
        """
        Called after protocol has started listening.
        """
        self.transport.connect(self.host, self.port)
        self.sendTime()

    def sendTime(self):
        if self.count >= 100000:
            self.f.close()
            reactor.stop()
        now = datetime.datetime.now()
        data = now.strftime("%y %m %d %H:%M:%S.%f")
        self.transport.write(data)

    def datagramReceived(self, data, (host, port)):
        end_time = datetime.datetime.now()
        self.count += 1
        # print "received %r from %s:%d" % (data, host, port)
        # self.transport.write(data, (host, port))
        start_time = end_time
        start_time = start_time.strptime(data, ("%y %m %d %H:%M:%S.%f"))
        # print start_time, end_time
        dur =  end_time - start_time
        self.f.write('%2.6f\n' % dur.total_seconds())
        self.sendTime()

    def connectionRefused(self):
        print "No one listening"

reactor.listenUDP(0, Latency())
reactor.run()