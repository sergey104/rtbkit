/** augmentor_1.cc                                 -*- C++ -*-
    Rémi Attab, 14 Feb 2013
    Copyright (c) 2013 Datacratic.  All rights reserved.

    Augmentor example that can be used to do extremely simplistic frequency
    capping.

*/

#include "augmentor_1.h"

#include "rtbkit/core/agent_configuration/agent_configuration_listener.h"
#include "rtbkit/core/agent_configuration/agent_config.h"
#include "rtbkit/plugins/augmentor/augmentor_base.h"
#include "rtbkit/common/bid_request.h"
#include "soa/service/zmq_named_pub_sub.h"
#include "jml/utils/exc_assert.h"

#include <unordered_map>
#include <mutex>

using namespace std;

namespace RTBKIT {

typedef struct {
    size_t counter;
    chrono::system_clock::time_point startPoint;
} CounterData_t;
    
typedef struct {
    CounterData_t perHourCounter;
    CounterData_t perDayCounter;
    size_t commonCounter;
    chrono::system_clock::time_point startInterval;
} Counters_t;

const size_t defaultInterval = 300;
const size_t NO_LIMIT = static_cast<size_t>(-1);

/******************************************************************************/
/* FREQUENCY CAP STORAGE                                                      */
/******************************************************************************/

/** A very primitive storage for frequency cap events.

    Ideally this should be replaced by some kind of low latency persistent
    storage (eg. Redis).

    Note that the locking scheme for this class is unlikely to scale well. A
    better scheme is left as an exercise to the reader.
 */
struct FrequencyCapStorage
{
    bool isKeysExist(const RTBKIT::AccountKey& account, const std::string& uid)
    {
        //std::cerr << "DEBUG: check key: " << account << "/" << uid << std::endl;
        lock_guard<mutex> guard(lock);
        auto it = counters.find(account.toString());
        if(it == counters.end())
            return false;
        else if(it->second.find(uid) == it->second.end())
            return false;
        
        //std::cerr << "DEBUG: check key: " << account << "/" << uid << " found."<< std::endl;
        return true;
    }    
    
    Counters_t getCounters(const RTBKIT::AccountKey& account, const std::string& uid)
    {
        //std::cerr << "DEBUG: get counters: " << account << "/" << uid << std::endl;
        lock_guard<mutex> guard(lock);
        return counters[account.toString()][uid];
    }

    void setCounters(const RTBKIT::AccountKey& account, const std::string& uid, Counters_t& cntrs)
    {
        //std::cerr << "DEBUG: set counters: " << account << "/" << uid << std::endl;
        lock_guard<mutex> guard(lock);
        counters[account.toString()][uid] = cntrs;
    }

private:

    mutex lock;
    unordered_map<std::string, unordered_map<std::string, Counters_t>> counters;
};

/******************************************************************************/
/* FREQUENCY CAP AUGMENTOR                                                    */
/******************************************************************************/

/** Note that the serviceName and augmentorName are distinct because you may
    have multiple instances of the service that provide the same
    augmentation.
*/
FrequencyCapAugmentor::
FrequencyCapAugmentor(
        std::shared_ptr<Datacratic::ServiceProxies> services,
        const string& serviceName,
        const string& augmentorName) :
    SyncAugmentor(augmentorName, serviceName, services),
    storage(new FrequencyCapStorage()),
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
FrequencyCapAugmentor::
init()
{
    SyncAugmentor::init(2 /* numThreads */);

    /* Manages all the communications with the AgentConfigurationService. */
    agentConfig.init(getServices()->config);
    addSource("FrequencyCapAugmentor::agentConfig", agentConfig);

    palEvents.init(getServices()->config);

    /* This lambda will get called when the post auction loop receives a win
       on an auction.
    */
    palEvents.messageHandler = [&] (const vector<zmq::message_t>& msg)
        {
            RTBKIT::AccountKey account(msg[11].toString());
            Json::Value req = Json::parse(msg[4].toString());
			Json::Value device = req["device"];
			
			//std::cerr << "DEBUG: devices: " << device.toString() << std::endl;
			
			Json::Value ifa = device["ifa"];
			
			//std::cerr << "DEBUG: ifa: " << ifa.asString() << std::endl;
			
            std::string uids = ifa.asString();

            //std::cerr << "DEBUG: account/uids: " << account << "/" << uids << std::endl;
            
            Counters_t counters;
            if(storage->isKeysExist(account, uids))
                counters = storage->getCounters(account, uids);
            else {
                counters.perHourCounter.counter = 0;
                counters.perDayCounter.counter = 0;
                counters.commonCounter = 0;
                counters.perHourCounter.startPoint = counters.perDayCounter.startPoint = std::chrono::system_clock::now();
            }
            
            /*
            int idx = 0;
            for(auto it: msg) {
                std::cerr << "DEBUG: " << idx++ << ": " << it.toString() << std::endl;
            }
            */
            
            
            ++counters.perHourCounter.counter;
            ++counters.perDayCounter.counter;
            ++counters.commonCounter;
            counters.startInterval = std::chrono::system_clock::now();
            
            storage->setCounters(account, uids, counters);
            recordHit("Impression");
        };

    palEvents.connectAllServiceProviders("rtbPostAuctionService", "logger", {"MATCHEDIMPRESSION"});
    addSource("FrequencyCapAugmentor::palEvents", palEvents);
}


/** Augments the bid request with our frequency cap information.

    This function has a 5ms window to respond (including network latency).
    Note that the augmentation is in charge of ensuring that the time
    constraints are respected and any late responses will be ignored.
*/
RTBKIT::AugmentationList
FrequencyCapAugmentor::
onRequest(const RTBKIT::AugmentationRequest& request)
{
    recordHit("requests");

    RTBKIT::AugmentationList result;

    const string& uids = request.bidRequest->device->ifa;

    for (const string& agent : request.agents) {

        RTBKIT::AgentConfigEntry config = agentConfig.getAgentEntry(agent);
        bool toSkip = false;

	
        /* When a new agent comes online there's a race condition where the
           router may send us a bid request for that agent before we receive
           its configuration. This check keeps us safe in that scenario.
        */
        if (!config.valid()) {
            recordHit("unknownConfig");
	        //std::cerr << "DEBUG: unknown Config" << std::endl;
            continue;
        }

        const RTBKIT::AccountKey& account = config.config->account;
		
		//std::cerr << "DEBUG: " << "account: " << account << "  uids: " << uids  << std::endl;

        if (storage->isKeysExist(account, uids)) {

            Counters_t counters = storage->getCounters(account, uids);
            //std::cerr << "DEBUG: " << "account: " << account << "  uids: " << uids << " counts: " << std::endl;
            //std::cerr << "DEBUG: \t\t\t    per hour: " << counters.perHourCounter.counter << std::endl;
            //std::cerr << "DEBUG: \t\t\t     per day: " << counters.perDayCounter.counter << std::endl;
            //std::cerr << "DEBUG: \t\t\t      common: " << counters.commonCounter << std::endl;
        
            size_t hourLimit = getHourLimit(request.augmentor, agent, config);
            size_t dayLimit = getDayLimit(request.augmentor, agent, config);
            size_t commonLimit = getCommonLimit(request.augmentor, agent, config);
            //std::cerr << "DEBUG: \t\t\t  hour limit: " << hourLimit << std::endl;
            //std::cerr << "DEBUG: \t\t\t   day limit: " << dayLimit << std::endl;
            //std::cerr << "DEBUG: \t\t\tcommon limit: " << commonLimit << std::endl;
            
            size_t minInterval = getInterval(request.augmentor, agent, config);
            //std::cerr << "DEBUG: interval: " << minInterval << std::endl;
            
            auto curInterval = chrono::system_clock::now() - counters.startInterval;
            
            //std::cerr << "DEBUG: waiting/interval: " << chrono::duration_cast<chrono::seconds>(curInterval).count() << "/" << minInterval << std::endl;
            
            if(curInterval < chrono::seconds(minInterval)) 
                toSkip = true;
            else if(counters.commonCounter >= commonLimit)
                toSkip = true;
            else {
                std::time_t cur_tm = std::time(nullptr);
                std::tm cur_utc_tm = *std::gmtime(&cur_tm);
        
                std::time_t savd_tm = std::chrono::system_clock::to_time_t(counters.perDayCounter.startPoint);
                std::tm savd_utc_tm = *std::gmtime(&savd_tm);
                std::time_t savh_tm = std::chrono::system_clock::to_time_t(counters.perHourCounter.startPoint);
                std::tm savh_utc_tm = *std::gmtime(&savh_tm);
        
                //std::cerr << "DEBUG: day/day: " << cur_utc_tm.tm_yday << "/" << savd_utc_tm.tm_yday << std::endl;
                //std::cerr << "DEBUG: hour/hour: " << cur_utc_tm.tm_hour << "/" << savh_utc_tm.tm_hour << std::endl;

                bool changed = false;
                if(cur_utc_tm.tm_yday != savd_utc_tm.tm_yday) {
                    counters.perDayCounter.counter = 0;
                    counters.perDayCounter.startPoint = std::chrono::system_clock::now();
                    changed = true;
                }
                if(cur_utc_tm.tm_hour != savh_utc_tm.tm_hour) {
                    counters.perHourCounter.counter = 0;
                    counters.perHourCounter.startPoint = std::chrono::system_clock::now();
                    changed = true;
                }
                if(changed)
                    storage->setCounters(account, uids, counters);
                if((counters.perDayCounter.counter >= dayLimit) || (counters.perHourCounter.counter >= hourLimit))
                    toSkip = true;
            }
        }
        if(!toSkip) {
            result[account].tags.insert("pass-frequency-cap-1");
            recordHit("accounts." + account[0] + ".passed");
            //std::cerr << "DEBUG: passed" << std::endl;
        }
        else {
            recordHit("accounts." + account[0] + ".skipped");
            //std::cerr << "DEBUG: skipped" << std::endl;
        }
    }

    return result;
}


/** Returns the frequency cap configured by the agent.

    This function is a bit brittle because it makes no attempt to validate
    the configuration.
*/
size_t
FrequencyCapAugmentor::
getCapLimit( const string& augmentor,
        const string& agent,
        const RTBKIT::AgentConfigEntry& config,
        const char* key) const
{
    size_t limit = NO_LIMIT;
    
    for (const auto& augConfig : config.config->augmentations) {
        if (augConfig.name != augmentor) continue;
        if(augConfig.config.isMember(key)) {
            limit = augConfig.config[key].asInt();
        }
    }

    return limit;
}

size_t
FrequencyCapAugmentor::
getHourLimit( const string& augmentor,
        const string& agent,
        const RTBKIT::AgentConfigEntry& config) const
{
    return getCapLimit(augmentor, agent, config, "maxPerHour");
}

size_t
FrequencyCapAugmentor::
getDayLimit( const string& augmentor,
        const string& agent,
        const RTBKIT::AgentConfigEntry& config) const
{
    return getCapLimit(augmentor, agent, config, "maxPerDay");
}

size_t
FrequencyCapAugmentor::
getCommonLimit( const string& augmentor,
        const string& agent,
        const RTBKIT::AgentConfigEntry& config) const
{
    return getCapLimit(augmentor, agent, config, "maxLimit");
}

/** Returns the interval configured by the agent.

    This function is a bit brittle because it makes no attempt to validate
    the configuration.
*/
size_t
FrequencyCapAugmentor::
getInterval( const string& augmentor,
	     const string& agent,
	     const RTBKIT::AgentConfigEntry& config) const
{
    size_t interval = defaultInterval;
    for (const auto& augConfig : config.config->augmentations) {
        if (augConfig.name != augmentor) continue;
        if(augConfig.config.isMember("minInterval")) {
            interval = augConfig.config["minInterval"].asInt();
        }
    }

    return interval;
}

} // namespace RTBKIT

