#!/usr/bin/python

import json
import uuid
import threading
import time
import httplib
from httplib import HTTPConnection
from httplib import HTTPResponse
import sys
import threading
import copy

# ----------------------------------------------------------------------------------------------------------------------


class RequestUpdate:
    def __init__(self, jsonvalue):
        self.data = json.loads(jsonvalue)
# ----------------------------------------------------------------------------------------------------------------------

class RequestData:
	def __init__(self, requestStr):
		self.requests = []
		self.jsonRequest = json.loads(requestStr)
		self.genderI = 0
		self.locationI = 0
		self.coordsI = 0
		self.yobI = 0
		self.bdI = 0
		self.ifaI = 0

	def makeData(self, maxreq, updata):
		self.requests = []
		while len(self.requests) < maxreq:
			self.update(updata)
			self.requests.append(copy.deepcopy(self.jsonRequest))
		
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
			if self.coordsI >= len(coords):
				self.coordsI = 0

	def changeYob(self, yob):
		if yob and len(yob):
			self.jsonRequest["user"]["yob"] = yob[self.yobI]
			self.yobI = self.yobI + 1
			if self.yobI >= len(yob):
				self.yobI = 0

	def changeBirthday(self, birthday):
		if birthday and len(birthday):
			for segment in self.jsonRequest["user"]["data"]["segment"]:
				if segment["id"] == "birthday":
					segment["value"] = birthday[self.bdI]
					self.bdI = self.bdI + 1
					if self.bdI >= len(birthday):
						self.bdI = 0

	def changeIfa(self, ifa):
		if ifa and len(ifa):
			self.jsonRequest["device"]["ifa"] = ifa[self.ifaI]
			self.ifaI = self.ifaI + 1
			if self.ifaI >= len(ifa):
				self.ifaI = 0

	def update(self, data):
		if data:
			for key in data:
				if key == "gender":
					self.changeGender(data["gender"])
				elif key == "birthday":
					self.changeBirthday(data["birthday"])
				elif key == "yob":
					self.changeYob(data["yob"])
				elif key == "location":
					self.changeLocation(data["location"])
				elif key == "ifa":
					self.changeIfa(data["ifa"])
		self.changeId()

	def getUserAgent(self):
		return self.jsonRequest["device"]["ua"]

	def getJsonData(self, idx):
		return self.requests[idx % len(self.requests)]

# -----------------------------------------------------------------------------------------------------------------------

class Server:
	def __init__(self):
		self.host = "127.0.0.1"
		self.port = 17339
		self.winport = 17340
		self.impport = 17341
		self.reqNumber = 0
		self.winNumber = 0
		self.impNumber = 0
		self.clickNumber = 0
		self.maxRequests = 1
		self.maxWins = 1
		self.maxImpressions = 1
		self.maxClicks = 1

	def getReqNumber(self):
		return self.reqNumber
    
	def getWinNumber(self):
		return self.winNumber
    
	def getImpNumber(self):
		return self.impNumber
	
	def getMaxRequests(self):
		return self.maxRequests

	def setConfig(self, jsonvalue):
		for key in jsonvalue:
			if key == "host":
				self.host = jsonvalue["host"]
			elif key == "port":
				self.port = jsonvalue["port"]
			elif key == "winport":
				self.winport = jsonvalue["winport"]
			elif key == "impport":
				self.impport = jsonvalue["impport"]
			elif key == "maxRequests":
				self.maxRequests = jsonvalue["maxRequests"]
			elif key == "maxWins":
				self.maxWins = jsonvalue["maxWins"]
			elif key == "maxImpressions":
				self.maxImpressions = jsonvalue["maxImpressions"]
			elif key == "maxClicks":
				self.maxClicks = jsonvalue["maxClicks"]
        
	def sendClickResponse(self, data, price):
		jsonResponse = json.loads('{"timestamp":0, "bidTimestamp":0, "auctionId":"", "impId": "1", "bidRequestId":"", "winPrice":0.0, "type":"CLICK"}')
		jsonResponse["timestamp"] = time.time()
		aucId = data["id"]
		jsonResponse["auctionId"] = aucId + ":1"
		jsonResponse["bidRequestId"] = aucId + ":1"
		jsonResponse["impId"] = data["seatbid"][0]["bid"][0]["impid"]
		jsonResponse["winPrice"] = price
		
		print "send click: ", jsonResponse
		
		params = json.dumps(jsonResponse)
		
		headers = {
			"Accept": "*/*",
			"Content-type": "application/json"
		}
		
		print "Try to connect: ", self.host, ":", self.impport
		try:
			conn = httplib.HTTPConnection(str(self.host), self.impport)
			conn.request("POST", "/events", params, headers)
			self.impNumber = self.impNumber + 1
			resp = conn.getresponse()
			print resp.status, ":", resp.reason
			clickdata = resp.read()
			if clickdata:
				print "data: ", clickdata
			conn.close()
		except:
			print "Connection error"
		return
        
	def sendImpressionResponse(self, data, price):
		jsonResponse = json.loads('{"timestamp":0, "bidTimestamp":0, "auctionId":"", "impId": "1", "bidRequestId":"", "winPrice":0.0, "type":"IMPRESSION"}')
		jsonResponse["timestamp"] = time.time()
		aucId = data["id"]
		jsonResponse["auctionId"] = aucId + ":1"
		jsonResponse["bidRequestId"] = aucId + ":1"
		jsonResponse["impId"] = data["seatbid"][0]["bid"][0]["impid"]
		jsonResponse["winPrice"] = price
		
		print "send impression: ", jsonResponse
		
		params = json.dumps(jsonResponse)
		
		headers = {
			"Accept": "*/*",
			"Content-type": "application/json"
		}
		
		print "Try to connect: ", self.host, ":", self.impport
		try:
			conn = httplib.HTTPConnection(str(self.host), self.impport)
			conn.request("POST", "/events", params, headers)
			self.impNumber = self.impNumber + 1
			resp = conn.getresponse()
			print resp.status, ":", resp.reason
			impdata = resp.read()
			if impdata:
				print "data: ", impdata
			conn.close()
		except:
			print "Connection error"
		return
		
	def sendWinResponse(self, data):
		windata = None
		jsonResponse = json.loads('{"timestamp":0, "bidTimestamp":0, "auctionId":"", "impId": "1", "bidRequestId":"", "winPrice":0.0, "type": null, "event":"win"}')
		jsonResponse["timestamp"] = time.time()
		aucId = data["id"]
		jsonResponse["auctionId"] = aucId + ":1"
		jsonResponse["bidRequestId"] = aucId + ":1"
		jsonResponse["impId"] = data["seatbid"][0]["bid"][0]["impid"]
		price = data["seatbid"][0]["bid"][0]["price"]
		delta = price * 5 / 100
		if delta < 0.001:
			delta = 0.001
		price = price - delta
		jsonResponse["winPrice"] = price
		
		print "send win: ", jsonResponse
		
		params = json.dumps(jsonResponse)
		
		headers = {
			"Accept": "*/*",
			"Content-type": "application/json"
		}
		
		print "Try to connect: ", self.host, ":", 17340
		try:
			conn = httplib.HTTPConnection(str(self.host), 17340)
			conn.request("POST", "/", params, headers)
			self.winNumber = self.winNumber + 1
			resp = conn.getresponse()
		except:
			print "Connection error"
			return
            
		print resp.status, ":", resp.reason
		windata = resp.read()
		conn.close()
		if windata:
			print "windata: ", windata
		if resp.status == 200 and self.impNumber < self.maxImpressions:
			self.sendImpressionResponse(data, price)
		return
        
	def sendRequest(self, request):
		data = None
		params = json.dumps(request.getJsonData(self.reqNumber))
		headers = {
			"Accept": "application/json",
			"Content-type": "application/json",
			"User-Agent": request.getUserAgent(),
			"Connection": "Keep-Alive",
			"Accept-Encoding": "gzip",
			"x-openrtb-version": "2.3"
		}
		print "Try to connect: ", self.host, ":", self.port
		try:
			conn = httplib.HTTPConnection(str(self.host), self.port)
			conn.request("POST", "/auctions", params, headers)
			self.reqNumber = self.reqNumber + 1
			print "Sent: ", params, "\n\n"
			resp = conn.getresponse()
		except:
			print "Connection error"
			return self.maxRequests - self.reqNumber
		
		print resp.status, ":", resp.reason
		if resp.status == 200:
			data = json.loads(resp.read())
			print "data: ", data, "\n\n"
		conn.close()
		if data and self.winNumber < self.maxWins:
			self.sendWinResponse(data)
		return self.maxRequests - self.reqNumber

class ServerList:
	def __init__(self):
		self.servers = {}
		self.servers["localhost"] = Server()

	def __getitem__(self, key):
		return self.servers[key]
	
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
		server = None
		self.rd = None
		self.rupdate = None 
		for key in jsondata:
			if key == "file":
				self.file = jsondata["file"]
			elif key == "server":
				server = jsondata["server"]
			elif key == "delay":
				self.delay = jsondata["delay"]
			elif key == "request":
				self.rupdate = jsondata["request"]
				
		self.server = copy.deepcopy(servers[server])

	def load(self):
		rqbase = open(self.file, "r")
		jsonline = rqbase.readline(4096)
		while len(jsonline):
			if jsonline[0] == '{':
				self.rd = RequestData(jsonline)
				self.rd.makeData(self.server.getMaxRequests(), self.rupdate)
				break
		rqbase.close()
	
	def run(self):
		while self.server.sendRequest(self.rd) > 0:
			if self.delay != 0:
				time.sleep(self.delay)

	def getReqNumber(self):
		return self.server.getReqNumber()

	def getWinNumber(self):
		return self.server.getWinNumber()

	def getImpNumber(self):
		return self.server.getImpNumber()
    
#-----------------------------------------------------------------------------------------------------------------------

class BRGenerator:
	def __init__(self, file):
		self.servers = ServerList()
		self.threads = {}
		self.T0 = 0
		self.T1 = 0

		self.config = json.load(file)

		for key in self.config:
			if key == "servers":
				data = self.config["servers"]
				if data:
					self.servers.setServers(data)
					
		for key in self.config:
			if key == "threads":
				data = self.config["threads"]
				if data:
					for thr in data:
						name = None
						config = None
						for key in thr:
							if key == "name":
								name = thr["name"]
							elif key == "config":
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

	def getReqNumber(self):
		number = 0
		for key in self.threads:
			number = number + self.threads[key].getReqNumber()
		return number

	def getWinNumber(self):
		number = 0
		for key in self.threads:
			number = number + self.threads[key].getWinNumber()
		return number

	def getImpNumber(self):
		number = 0
		for key in self.threads:
			number = number + self.threads[key].getImpNumber()
		return number
    
#-----------------------------------------------------------------------------------------------------------------------

def main(filename):
	if filename:
		file = open(filename, "r")
		brg = BRGenerator(file)

		T0 = time.time()
		brg.load()
		brg.run()
			
		T1 = time.time()
		print "Time: ", T1 - T0
		print "Number of requests   : ", brg.getReqNumber()
		print "Number of wins       : ", brg.getWinNumber()
		print "Number of impressions: ", brg.getImpNumber()

if len(sys.argv) < 2:
	print("No config file")
else:
	main(sys.argv[1])
