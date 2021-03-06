/** augmentor_ex.cc                                 -*- C++ -*-
    Rémi Attab, 14 Feb 2013
    Copyright (c) 2013 Datacratic.  All rights reserved.

    Augmentor example that can be used to do extremely simplistic frequency
    capping.

*/

#include "augmentor_ag.h"

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

    const chrono::hours aWaitPeriod(24);
    const chrono::seconds aDelayPeriod(300);
//    const chrono::seconds aWaitPeriod(60);

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
    /** Returns the number of times an ad for the given account has been shown
        to the given user.
     */
    size_t get(const RTBKIT::AccountKey& account, const RTBKIT::UserIds& uids)
    {
        lock_guard<mutex> guard(lock);
        return counters[uids.exchangeId][account[0]];
    }

    /** Increments the number of times an ad for the given account has been
        shown to the given user.
     */
    void inc(const RTBKIT::AccountKey& account, const RTBKIT::UserIds& uids)
    {
        lock_guard<mutex> guard(lock);
        counters[uids.exchangeId][account[0]]++;
    }

    void reset(const RTBKIT::AccountKey& account, const RTBKIT::UserIds& uids)
    {
        lock_guard<mutex> guard(lock);
        counters[uids.exchangeId][account[0]] = 0;
    }
    
    chrono::system_clock::time_point get_time(const RTBKIT::AccountKey& account, const RTBKIT::UserIds& uids)
    {
        lock_guard<mutex> guard(lock);
        return time_points[uids.exchangeId][account[0]];
    }

    /** Increments the number of times an ad for the given account has been
        shown to the given user.
     */
    void set_time(const RTBKIT::AccountKey& account, const RTBKIT::UserIds& uids)
    {
        lock_guard<mutex> guard(lock);
        time_points[uids.exchangeId][account[0]] = chrono::system_clock::now();
    }

    chrono::system_clock::time_point get_delay_point(const RTBKIT::AccountKey& account, const RTBKIT::UserIds& uids)
    {
        lock_guard<mutex> guard(lock);
        return delay_points[uids.exchangeId][account[0]];
    }

    /** Increments the number of times an ad for the given account has been
        shown to the given user.
     */
    void set_delay_point(const RTBKIT::AccountKey& account, const RTBKIT::UserIds& uids)
    {
        lock_guard<mutex> guard(lock);
        delay_points[uids.exchangeId][account[0]] = chrono::system_clock::now();
    }
    
private:

    mutex lock;
    unordered_map<Datacratic::Id, unordered_map<string, size_t> > counters;
    unordered_map<Datacratic::Id, unordered_map<string, chrono::system_clock::time_point> > time_points;
    unordered_map<Datacratic::Id, unordered_map<string, chrono::system_clock::time_point> > delay_points;

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
            RTBKIT::AccountKey account(msg[19].toString());
            RTBKIT::UserIds uids = RTBKIT::UserIds::createFromString(msg[15].toString());

	    //std::cerr << "DEBUG: " << "account: " << account << "/uids: " << uids.toString() << " inc " << std::endl;
            storage->inc(account, uids);
	    storage->set_delay_point(account, uids);
            recordHit("wins");
        };

    palEvents.connectAllServiceProviders("rtbPostAuctionService", "logger", {"MATCHEDWIN"});
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
    bool delay = false;
    Datacratic::Utf8String Minsk("Minsk");
    //Datacratic::Utf8String Gurgaon("Gurgaon");
    //Datacratic::Utf8String Tbilisi("Tbilisi");
    recordHit("requests");

    RTBKIT::AugmentationList result;

    const RTBKIT::UserIds& uids = request.bidRequest->userIds;

    for (const string& agent : request.agents) {

        RTBKIT::AgentConfigEntry config = agentConfig.getAgentEntry(agent);
	
	std::string gender = request.bidRequest->user->gender;
	Datacratic::Utf8String city = request.bidRequest->location.cityName;
	
        //std::cerr << "DEBUG: gender: " << gender << std::endl;
        //std::cerr << "DEBUG: city: " << city << std::endl;
	

        /* When a new agent comes online there's a race condition where the
           router may send us a bid request for that agent before we receive
           its configuration. This check keeps us safe in that scenario.
        */
        if (!config.valid()) {
            recordHit("unknownConfig");
            continue;
        }

        const RTBKIT::AccountKey& account = config.config->account;

        size_t count = storage->get(account, uids);
	//std::cerr << "DEBUG: " << "account: " << account << "/uids: " << uids.toString() << " count: " << count << std::endl;

	if(count == 0) {
	    delay = false;
	} else if(chrono::system_clock::now() - storage->get_delay_point(account, uids) < aDelayPeriod) {
	    delay = true;
	}
        /* The number of times a user has been seen by a given agent can be
           useful to make bid decisions so attach this data to the bid
           request.

           It's also recomended to place your data in an object labeled
           after the augmentor from which it originated.
        */
        result[account[0]].data = count;
	
        /* We tag bid requests that pass the frequency cap filtering because
           if the augmentation doesn't terminate in time or if an error
           occured, then the bid request will not receive any tags and will
           therefor be filtered out for agents that require the frequency
           capping.
        */
	
	if((gender != "F") || (city != Minsk) || delay) {
	    result[account].tags.insert("nopass-frequency-cap-ex");
	} else {
	    size_t cap = getCap(request.augmentor, agent, config);
	    if (count < cap) {
		result[account].tags.insert("pass-frequency-cap-ex");
		recordHit("accounts." + account[0] + ".passed");
		if((cap - count) >= 1) 
		{
		    storage->set_time(account, uids);
		}
	    }
	    else 
	    {
		recordHit("accounts." + account[0] + ".capped");
		if(chrono::system_clock::now() - storage->get_time(account, uids) > aWaitPeriod)
		{
		    storage->reset(account, uids);
		}
	    }
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
getCap( const string& augmentor,
        const string& agent,
        const RTBKIT::AgentConfigEntry& config) const
{
    for (const auto& augConfig : config.config->augmentations) {
        if (augConfig.name != augmentor) continue;
        return augConfig.config.asInt();
    }

    /* There's a race condition here where if an agent removes our augmentor
       from its configuration while there are bid requests being augmented
       for that agent then we may not find its config. A sane default is
       good to have in this scenario.
    */

    return 0;
}

} // namespace RTBKIT

