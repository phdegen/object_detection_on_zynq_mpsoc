#!/usr/bin/env python
"USAGE: echoclient.py <server> <word> <port>"
from socket import *    # import *, but we'll avoid name conflict
import sys
import numpy as np
import cv2

IMG_SIZE = 374*1232*3


def handleClient(sock):

    while True:
    	while True:
    	    data = sock.recv(4)
    	    print data
            if data=="STOP":
                break
            if data=="STRT":
                break

        if data=="STOP":
            break

        data = sock.recv(1024)
        img = np.fromstring(data,dtype=np.uint8)
        for a in range(0,1348):
            data = sock.recv(1024)
            img = np.concatenate((img,np.fromstring(data,dtype=np.uint8)))
    

        data = sock.recv(928)
        img = np.concatenate((img,np.fromstring(data,dtype=np.uint8)))
        print(img.size)
        print (img)

        sock.sendall("GOTTEN")

        if img.size==IMG_SIZE :
            img = img.reshape(374,1232,3)
            cv2.imshow('image',img)
            cv2.waitKey(1000);

        print 'recieved image'
    cv2.destroyAllWindows()
    sock.close()





sock = socket(AF_INET, SOCK_STREAM)
sock.bind(('',int('1346')))
sock.listen(5)
while 1:    # Run until cancelled
    newsock, client_addr = sock.accept()
    print "Client connected:", client_addr
    handleClient(newsock)
