/** augmentor_device_class.cc                                 -*- C++ -*-
    Copyright (c) 2013 Datacratic.  All rights reserved.

    Augmentor example that can be used to do extremely simplistic frequency
    capping.

*/

#include "augmentor_device_class.h"

#include "rtbkit/core/agent_configuration/agent_configuration_listener.h"
#include "rtbkit/core/agent_configuration/agent_config.h"
#include "rtbkit/plugins/augmentor/augmentor_base.h"
#include "rtbkit/common/bid_request.h"
#include "soa/service/zmq_named_pub_sub.h"
#include "jml/utils/exc_assert.h"

#include <unordered_set>
#include <mutex>

using namespace std;

namespace RTBKIT {


/******************************************************************************/
/* FREQUENCY CAP STORAGE                                                      */
/******************************************************************************/

/** A very primitive storage for frequency cap events.

    Ideally this should be replaced by some kind of low latency persistent
    storage (eg. Redis).

    Note that the locking scheme for this class is unlikely to scale well. A
    better scheme is left as an exercise to the reader.
 */
struct DeviceClassStorage
{
    /** Load data from file
     */
    void loadData(const std::string& jsondata)
    {
//	std::cerr << "DEBUG: Load data" << std::endl;
	data.clear();

	if(!jsondata.empty()) {
	    try {
		Json::Value dbf = Json::parse(jsondata);
		if(!dbf.isNull()) {
		    Json::Value db = dbf.get("device_classes", "");
		    if(!db.isNull()) {
			for(auto cl: db) {
			    std::string name;
			    name = cl.get("class_name", "").asCString();
			    if(!name.empty()) {
//				std::cerr << "DEBUG: device class: " << name << std::endl;
				std::unordered_set<std::string> devices;
				Json::Value dnl = cl.get("devices", "");
				for(auto dn: dnl) {
				    if(!dn.isNull()) {
					devices.insert(std::string(dn.asCString()));
//					std::cerr << "DEBUG: \tdevice: " << dn.asCString() << std::endl;
				    }
				}
				data.push_back(std::make_pair(name, devices));
			    }
			}
		    }
		}
	    }
	    catch(const std::exception& ex) {
		std::cerr << ML::format("threw exception: %s", ex.what());
		exit(-2);
	    }
	    catch(...) {
		exit(-3);
	    }
	} else {
	    std::cerr << "empty data" << std::endl;
	    exit(-4);
	}
    }
    
    std::string findClass(const std::string& device) {
	std::string deviceClass("");
	for(auto it: data) {
	    if(it.second.find(device) != it.second.end()) {
		deviceClass = it.first;
		break;
	    }
	}
	return deviceClass;
    }

private:

    deviceClassInfoList data;

};

/******************************************************************************/
/* DEVICE CLASS AUGMENTOR                                                    */
/******************************************************************************/

/** Note that the serviceName and augmentorName are distinct because you may
    have multiple instances of the service that provide the same
    augmentation.
*/
DeviceClassAugmentor::
DeviceClassAugmentor(
        std::shared_ptr<Datacratic::ServiceProxies> services,
        const string& serviceName,
        const string& augmentorName) :
    SyncAugmentor(augmentorName, serviceName, services),
    storage(new DeviceClassStorage()),
    agentConfig(getZmqContext()),
    palEvents(getZmqContext())
{
    recordHit("up");
}


/** Sets up the internal components of the augmentor.

    Note that SyncAugmentorBase is a MessageLoop so we can attach all our
    other service providers to our message loop to cut down on the number of
    polling threads which in turns reduces the number of context switches.
*/
void
DeviceClassAugmentor::
init(const std::string& data)
{
    storage->loadData(data);
    
    SyncAugmentor::init(2 /* numThreads */);

    /* Manages all the communications with the AgentConfigurationService. */
    agentConfig.init(getServices()->config);
    addSource("DeviceClassAugmentor::agentConfig", agentConfig);

    palEvents.init(getServices()->config);

    /* This lambda will get called when the post auction loop receives a win
       on an auction.
    */
    palEvents.messageHandler = [&] (const vector<zmq::message_t>& msg)
        {
            RTBKIT::AccountKey account(msg[19].toString());
            RTBKIT::UserIds uids = RTBKIT::UserIds::createFromString(msg[15].toString());

	    /*
	    for(auto it: msg) {
		std::cerr << "DEBUG: " << it.toString() << std::endl;
	    }
	    */
//	    std::cerr << "DEBUG: Wins. Request id: " << msg[2].toString() << std::endl;
            recordHit("wins");
        };

    palEvents.connectAllServiceProviders("rtbPostAuctionService", "logger", {"MATCHEDWIN"});
    addSource("DeviceClassAugmentor::palEvents", palEvents);
}


/** Augments the bid request with our frequency cap information.

    This function has a 5ms window to respond (including network latency).
    Note that the augmentation is in charge of ensuring that the time
    constraints are respected and any late responses will be ignored.
*/
RTBKIT::AugmentationList
DeviceClassAugmentor::
onRequest(const RTBKIT::AugmentationRequest& request)
{
    recordHit("requests");

    RTBKIT::AugmentationList result;

    for (const string& agent : request.agents) {

        RTBKIT::AgentConfigEntry config = agentConfig.getAgentEntry(agent);
	
        /* When a new agent comes online there's a race condition where the
           router may send us a bid request for that agent before we receive
           its configuration. This check keeps us safe in that scenario.
        */
        if (!config.valid()) {
            recordHit("unknownConfig");
//	    std::cerr << "DEBUG: unknown Config" << std::endl;
            continue;
        }

        const RTBKIT::AccountKey& account = config.config->account;
        std::string device_class = storage->findClass(request.bidRequest->device->model.extractAscii());
        //std::cerr << "DEBUG: model: "<< request.bidRequest->device->model << " / class: " << device_class << std::endl;

        result[account].tags.insert(device_class.c_str());
        recordHit("accounts." + account[0] + ".passed");
    }

    return result;
}

} // namespace RTBKIT

