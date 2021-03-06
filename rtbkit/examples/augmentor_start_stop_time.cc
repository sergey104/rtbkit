/** augmentor_start_stop.cc                                 -*- C++ -*-
    Alexander Poznyak, 25.10.2016
*/

#include "augmentor_start_stop_time.h"

#include "rtbkit/core/agent_configuration/agent_configuration_listener.h"
#include "rtbkit/core/agent_configuration/agent_config.h"
#include "rtbkit/plugins/augmentor/augmentor_base.h"
#include "rtbkit/common/bid_request.h"
#include "soa/service/zmq_named_pub_sub.h"
#include "jml/utils/exc_assert.h"

#include <unordered_map>
#include <mutex>
#include <time.h>

using namespace std;

namespace RTBKIT {

/******************************************************************************/
/* START STOP AUGMENTOR                                                    */
/******************************************************************************/

/** Note that the serviceName and augmentorName are distinct because you may
    have multiple instances of the service that provide the same
    augmentation.
*/
StartStopAugmentor::
StartStopAugmentor(
        std::shared_ptr<Datacratic::ServiceProxies> services,
        const string& serviceName,
        const string& augmentorName) :
    SyncAugmentor(augmentorName, serviceName, services),
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
StartStopAugmentor::
init()
{
    SyncAugmentor::init(2 /* numThreads */);

    /* Manages all the communications with the AgentConfigurationService. */
    agentConfig.init(getServices()->config);
    addSource("StartStopAugmentor::agentConfig", agentConfig);

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
            
            //std::cerr << "DEBUG: Wins. Request id: " << msg[2].toString() << std::endl;
            recordHit("wins");
        };

    palEvents.connectAllServiceProviders("rtbPostAuctionService", "logger", {"MATCHEDWIN"});
    addSource("StartStopAugmentor::palEvents", palEvents);
}


/** Augments the bid request with our frequency cap information.

    This function has a 5ms window to respond (including network latency).
    Note that the augmentation is in charge of ensuring that the time
    constraints are respected and any late responses will be ignored.
*/
RTBKIT::AugmentationList
StartStopAugmentor::
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
            //std::cerr << "DEBUG: unknown Config" << std::endl;
            continue;
        }

        const RTBKIT::AccountKey& account = config.config->account;
        //std::cerr << "DEBUG: " << "account: " << account <<  std::endl;
	
        std::time_t cur_time = std::time(nullptr);
        std::tm cur_tm = *std::localtime(&cur_time);
    
        std::tm start_tm;
        std::tm stop_tm;
	
        if((getStartTime(request.augmentor, agent, config, &start_tm) > 0) && 
        (getStopTime(request.augmentor, agent, config, &stop_tm) >= 0)) {

            //std::cerr << "DEBUG: Start: " << start_tm.tm_mday << "." << start_tm.tm_mon + 1 << "." << start_tm.tm_year + 1900 << " " << start_tm.tm_hour << ":" << start_tm.tm_min << std::endl;
            //std::cerr << "DEBUG: Current: " << cur_tm.tm_mday << "." << cur_tm.tm_mon + 1 << "." << cur_tm.tm_year + 1900 << " " << cur_tm.tm_hour << ":" << cur_tm.tm_min << std::endl;
            //std::cerr << "DEBUG: Stop: " << stop_tm.tm_mday << "." << stop_tm.tm_mon + 1 << "." << stop_tm.tm_year + 1900 << " " << stop_tm.tm_hour << ":" << stop_tm.tm_min << std::endl;

            std::time_t start_time = mktime(&start_tm);
            std::time_t stop_time = mktime(&stop_tm);
            
            if((start_time <= cur_time) && (cur_time <= stop_time)) {
                result[account].tags.insert("pass-start-stop-time");
                recordHit("accounts." + account[0] + ".passed");
                //std::cerr << "DEBUG: passed" << std::endl;
            }
            else {
                recordHit("accounts." + account[0] + ".at-a-wrong-time");
                //std::cerr << "DEBUG: at a wrong time." << std::endl;
                continue;
            }
        }
        else {
            recordHit("badConfig");
            //std::cerr << "DEBUG: bad config or bad data" << std::endl;
            continue;
        }
    }

    return result;
}

/** Returns the start time configured by the agent.

*/
int
StartStopAugmentor::
getTime( const std::string& augmentor,
	 const std::string& agent,
	 const RTBKIT::AgentConfigEntry& config,
	 const std::string key,
	 std::tm* ptime) const
{
    std::string time_str;
    for (const auto& augConfig : config.config->augmentations) {
        if (augConfig.name != augmentor) continue;
        if(augConfig.config.isMember(key.c_str())) {
            time_str = augConfig.config[key.c_str()].asString();
            size_t length = time_str.length();
            if(length) {
                const char *p = time_str.c_str();
                char *result = strptime(p, "%d.%m.%Y %H:%M", ptime);
                if(result && !*result)
                    return length;
            }
        }
    }

    return 0;
}

/** Returns the start time configured by the agent.

*/
int
StartStopAugmentor::
getStartTime( const std::string& augmentor,
	      const std::string& agent,
	      const RTBKIT::AgentConfigEntry& config,
	      std::tm* pstart_time) const
{
    return getTime(augmentor, agent, config, std::string("start"), pstart_time);
}

/** Returns the stop time configured by the agent.

*/
int
StartStopAugmentor::
getStopTime( const std::string& augmentor,
	     const std::string& agent,
	     const RTBKIT::AgentConfigEntry& config,
	     std::tm* pstop_time) const
{
    if(getTime(augmentor, agent, config, std::string("stop"), pstop_time) == 0) {
        std::time_t cur_time = std::time(nullptr);
        *pstop_time = *std::localtime(&cur_time);
        pstop_time->tm_year += 100;
        return 0;
    }
    return -1;
}

} // namespace RTBKIT

