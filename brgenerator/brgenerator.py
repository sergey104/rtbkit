#!/usr/bin/python

import json
import uuid
import threading
import time
import httplib
import sys
import threading

#-----------------------------------------------------------------------------------------------------------------------
class RequestUpdate:
    def __init__(self, jsonvalue):
        self.data = json.loads(jsonvalue)
#-----------------------------------------------------------------------------------------------------------------------
class RequestData:
    def __init__(self, requestStr):
        self.jsonRequest = json.loads(requestStr)
        self.genderI = 0
        self.locationI = 0
        self.coordsI = 0

    def changeId(self):
        self.jsonRequest["id"] = str(uuid.uuid4())

    def changeGender(self, gender):
        if gender and len(gender):
            self.jsonRequest["user"]["gender"] = gender[self.genderI]
            self.genderI = self.genderI + 1
            if self.genderI >= len(gender):
                self.genderI = 0

    def changeLocation(self, location):
        if location and len(location) and location[self.locationI]:
            city = location[self.locationI]["city"]
            country = location[self.locationI]["country"]
            if city:
                self.jsonRequest["device"]["geo"]["city"] = city
            if country:
                self.jsonRequest["device"]["geo"]["country"] = country
            self.locationI = self.locationI + 1
            if self.locationI >= len(location):
                self.locationI = 0

    def changeCoordinates(self, coords):
        if coords and len(coords) and coords[self.coordsI]:
            lon = coords[self.coordsI]["lon"]
            lat = coords[self.coordsI]["lat"]
            if lon:
                self.jsonRequest["device"]["geo"]["lon"] = lon
            if lat:
                self.jsonRequest["device"]["geo"]["lat"] = lat
            self.coordsI = self.coordsI + 1
            if self.coordsI > len(coords):
                self.coordsI = 0

    def changeYob(self, yob):
        if yob:
            self.jsonRequest["user"]["yob"] = yob

    def changeBirthday(self, birthday):
        if birthday:
            for segment in self.jsonRequest["user"]["data"]["segment"]:
                if segment["id"] == "birthday":
                    segment["value"] = birthday

    def changeIfa(self, ifa):
        if ifa:
            self.jsonRequest["device"]["ifa"] = ifa

    def update(self, data = None):
        self.changeId()

    def getUserAgent(self):
        return self.jsonRequest["device"]["ua"]

    def getJsonData(self):
        return self.jsonRequest

# -----------------------------------------------------------------------------------------------------------------------

class Server:
    def __init__(self):
        self.host = "127.0.0.1"
        self.port = 17339

    def setConfig(self, jsonvalue):
        self.host = jsonvalue["host"]
        self.port = jsonvalue["port"]

    def sendRequest(self, request):
        request.update()
        params = json.dumps(request.getJsonData())
        headers = {
            "Accept": "application/json",
            "Content-type": "application/json",
            "User-Agent": request.getUserAgent(),
            "Connection": "Keep-Alive",
            "Accept-Encoding": "gzip",
            "x-openrtb-version": "2.3"
        }

#	print "Headers: ", headers
        try:
            print "Try to connect: ", self.host, ":", self.port
            conn = httplib.HTTPConnection(host = self.host, port = self.port, timeout = 10)
            conn.set_debuglevel(5)
#           conn = httplib.HTTPConnection('127.0.0.1', 17339, timeout = 10)
            conn.request("POST", "/auctions", params, headers)
            resp = conn.getresponse()
#           print resp.status, ":", resp.reason
            return resp
        except:
            print "Connection error"

class ServerList:
    def __init__(self):
        self.servers = {}
        self.servers["default"] = Server()

    def setServers(self, jsonvalue):
        for svr in jsonvalue:
            name = svr["name"]
            config = svr["config"]
            newserver = Server()
            newserver.setConfig(config)
            self.servers[name] = newserver

    def getServer(self, name):
        return self.servers[name]

#-----------------------------------------------------------------------------------------------------------------------

class RequestThread:
    def __init__(self, jsondata, servers):
        self.file = jsondata["file"]
        server = jsondata["server"]
        self.delay = jsondata["delay"]
        self.rupdate = jsondata["request"]
        self.requests = []
        self.rqNumber = 0
        self.server = Server()
        self.server.host = servers.getServer(server).host
        self.server.port = servers.getServer(server).port

    def load(self):
        rqbase = open(self.file, "r")
        jsonline = rqbase.readline(4096)
        while len(jsonline):
            if jsonline[0] == '{':
                rd = RequestData(jsonline)
                self.requests.append(rd)
	    jsonline = rqbase.readline(4096)
	rqbase.close()
        
    def run(self):
        for rdi in self.requests:
            self.server.sendRequest(rdi)
            self.rqNumber = self.rqNumber + 1
            if self.delay != 0:
		time.sleep(self.delay)
            
    def getRQNumber(self):
	return self.rqNumber

#-----------------------------------------------------------------------------------------------------------------------

class BRGenerator:
    def __init__(self, file):
        self.servers = ServerList()
        self.threads = {}
        self.T0 = 0
        self.T1 = 0

        self.config = json.load(file)

        data = self.config["servers"]
        if data:
            self.servers.setServers(data)

        data = self.config["threads"]
        if data:
            for thr in data:
                name = thr["name"]
                config = thr["config"]
                if name and config:
                    thread = RequestThread(config, self.servers)
                    self.threads[name] = thread

    def load(self):
        for key in self.threads:
	    print("load for: ", key)
            self.threads[key].load()

    def run(self):
        threads = []
        self.T0 = time.time()
        for key in self.threads:
            p = threading.Thread(target = self.threads[key].run())
            p.start()
            threads.append(p)
            
        for thr in threads:
            thr.join()
	self.T1 = time.time()

    def getTime(self):
	return self.T1 - self.T0
    
    def getRQNumber(self):
	number = 0
	for key in self.threads:
	    number = number + self.threads[key].getRQNumber()
	return number

#-----------------------------------------------------------------------------------------------------------------------

def main(filename):
    if filename:
        file = open(filename, "r")
        brg = BRGenerator(file)

        brg.load()
        brg.run()
        print "Time: ", brg.getTime()
        print "Number of requests: ", brg.getRQNumber()

if len(sys.argv) < 2:
    print("No config file")
else:
    main(sys.argv[1])
