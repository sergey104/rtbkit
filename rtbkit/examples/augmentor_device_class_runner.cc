/** augmentor_device_class_runner.cc                                 -*- C++ -*-
    Rémi Attab, 22 Feb 2013
    Copyright (c) 2013 Datacratic.  All rights reserved.

    Runner for our augmentor example.

*/

#include "augmentor_device_class.h"
#include "soa/service/service_utils.h"

#include <boost/program_options/cmdline.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/positional_options.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>
#include <hiredis/hiredis.h>

#include <iostream>
#include <thread>
#include <chrono>


using namespace std;

int static getDeviceClassData(const std::string& server, unsigned port, const std::string& key, std::string& buffer)
{
    int result = 0;
    
    std::cerr << "DEBUG: connect to: " << server << ":" << port << std::endl;
    redisContext *rc = redisConnect(server.c_str(), port);
    if(rc) {
	if(!rc->err) {
	    std::string command = std::string("GET ") + key;
	    std::cerr << "DEBUG: coomand: " << command <<  std::endl;
	    redisReply *reply = static_cast<redisReply*>(redisCommand(rc, command.c_str()));
	    if(reply) {
		if(reply->type != REDIS_REPLY_ERROR) {
		    if(reply->str && *reply->str) {
			buffer = reply->str;
			freeReplyObject(reply);
			std::cerr << "DEBUG: buffer: " << buffer << std::endl;
		    }
		    else {
			std::cerr << "no data" << std::endl;
			result = -5;
		    }
		}
		else {
		    std::cerr << reply->str << std::endl;
		    result = -4;
		}
	    } else {
		std::cerr << "Can't allocate redis reply" << std::endl;
		result = -3;
	    }
	}
	else {
	    std::cerr << "Error: " << rc->errstr << std::endl;
	    result = -2;
	}
	redisFree(rc);
    }
    else {
	std::cerr << "Can't allocate redis context" << std::endl;
	result = -1;
    }
    return result;
}

/******************************************************************************/
/* MAIN                                                                       */
/******************************************************************************/

int main(int argc, char** argv)
{
    using namespace boost::program_options;

    Datacratic::ServiceProxyArguments args;
    
    std::string redis_server("127.0.0.1");
    int redis_port = 6379;
    std::string data_key("DeviceClasses");
    
    std::string jsondata("");

    options_description options = args.makeProgramOptions();
    options.add_options() ("help,h", "Print this message");
    options.add_options() ("server,s", value(&redis_server), "redis server");
    options.add_options() ("port,p", value(&redis_port), "redis port");
    options.add_options() ("key,k", value(&data_key), "redis port");

    variables_map vm;
    store(command_line_parser(argc, argv).options(options).run(), vm);
    notify(vm);

    if (vm.count("help")) {
        cerr << options << endl;
        return 1;
    }
    
    int result = getDeviceClassData(redis_server, redis_port, data_key, jsondata);
    if(result) 
	return result;
    
    auto serviceProxies = args.makeServiceProxies();
    RTBKIT::DeviceClassAugmentor augmentor(serviceProxies, "device-class");
    augmentor.init(jsondata);
    augmentor.start();

    while (true) this_thread::sleep_for(chrono::seconds(10));

    // Won't ever reach this point but this is how you shutdown an augmentor.
    augmentor.shutdown();

    return 0;
}
