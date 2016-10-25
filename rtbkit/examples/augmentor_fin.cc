/** augmentor_fin.cc                                 -*- C++ -*-
    Alexander Poznyak, 20.10.2016

    Financial augmentor.
**/

#include "augmentor_fin.h"

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
    chrono::system_clock::time_point startHourPeriod;
    chrono::system_clock::time_point startDayPeriod;
    Amount amountPerHour;
    Amount amountPerDay;
} SpentMoney_t;

/******************************************************************************/
/* FREQUENCY CAP STORAGE                                                      */
/******************************************************************************/

/** A very primitive storage for financial cap events.

    Ideally this should be replaced by some kind of low latency persistent
    storage (eg. Redis).

    Note that the locking scheme for this class is unlikely to scale well. A
    better scheme is left as an exercise to the reader.
 */
struct FinancialCapStorage
{
    
    bool isKeyExist(const RTBKIT::AccountKey& account)
    {
        lock_guard<mutex> guard(lock);
        //std::cerr << "DEBUG: check key: " << account << std::endl;
        if(dbSpentMoney.find(account) == dbSpentMoney.end()) {
            return false;
        }
        return true;
    }
        
    void setSpentMoney(const RTBKIT::AccountKey& account, const SpentMoney_t& spentMoney)
    {
        lock_guard<mutex> guard(lock);
        //std::cerr << "DEBUG: set key: " << account << std::endl;
        dbSpentMoney[account] = spentMoney;
    }

    SpentMoney_t getSpentMoney(const RTBKIT::AccountKey& account)
    {
        lock_guard<mutex> guard(lock);
        //std::cerr << "DEBUG: get key: " << account << std::endl;
        return dbSpentMoney[account];
    }
    
private:

    mutex lock;
    unordered_map<RTBKIT::AccountKey, SpentMoney_t> dbSpentMoney;
};

/******************************************************************************/
/* FREQUENCY CAP AUGMENTOR                                                    */
/******************************************************************************/

/** Note that the serviceName and augmentorName are distinct because you may
    have multiple instances of the service that provide the same
    augmentation.
*/
FinancialAugmentor::
FinancialAugmentor(std::shared_ptr<Datacratic::ServiceProxies> services,
                   const string& serviceName,
                   const string& augmentorName) :
                   SyncAugmentor(augmentorName, serviceName, services),
                   storage(new FinancialCapStorage()),
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
FinancialAugmentor::
init()
{
    SyncAugmentor::init(2 /* numThreads */);

    /* Manages all the communications with the AgentConfigurationService. */
    agentConfig.init(getServices()->config);
    addSource("FinancialAugmentor::agentConfig", agentConfig);

    palEvents.init(getServices()->config);

    /* This lambda will get called when the post auction loop receives a win
       on an auction.
    */
    palEvents.messageHandler = [&] (const vector<zmq::message_t>& msg)
        {
            RTBKIT::AccountKey account(msg[11].toString());

            /*
            for(auto it: msg) {
                std::cerr << "DEBUG: " << it.toString() << std::endl;
            }
            */
            Json::Value data = Json::parse(msg[6].toString());
            RTBKIT::Amount winPrice = RTBKIT::Amount::fromJson(data["winPrice"]);
            SpentMoney_t spentMoney;
            if(storage->isKeyExist(account)) {
                spentMoney = storage->getSpentMoney(account);
                //std::cerr << "DEBUG: spendMoney: " << std::endl;
                //std::cerr << "DEBUG: \tamountPerHour: " << spentMoney.amountPerHour.toString() << std::endl;
                //std::cerr << "DEBUG: \tamountPerDay: " << spentMoney.amountPerDay.toString() << std::endl;
            }
            else {
                spentMoney.startHourPeriod = std::chrono::system_clock::now();
                spentMoney.startDayPeriod = std::chrono::system_clock::now();
                spentMoney.amountPerHour = RTBKIT::Amount("USD/1M", 0);
                spentMoney.amountPerDay = RTBKIT::Amount("USD/1M", 0);
            }
            spentMoney.amountPerHour += winPrice;
            spentMoney.amountPerDay += winPrice;
            //std::cerr << "DEBUG: updated spendMoney: " << std::endl;
            //std::cerr << "DEBUG: \tamountPerHour: " << spentMoney.amountPerHour.toString() << std::endl;
            //std::cerr << "DEBUG: \tamountPerDay: " << spentMoney.amountPerDay.toString() << std::endl;
            storage->setSpentMoney(account, spentMoney);
	        //std::cerr << "DEBUG: Wins. Request id: " << msg[2].toString() << std::endl;
            //std::cerr << "DEBUG: Wins. Price: " << winPrice.toString() << std::endl;
            recordHit("impression");
        };

    palEvents.connectAllServiceProviders("rtbPostAuctionService", "logger", {"MATCHEDIMPRESSION"});
    addSource("FinancialAugmentor::palEvents", palEvents);
}


/** Augments the bid request with our frequency cap information.

    This function has a 5ms window to respond (including network latency).
    Note that the augmentation is in charge of ensuring that the time
    constraints are respected and any late responses will be ignored.
*/
RTBKIT::AugmentationList
FinancialAugmentor::
onRequest(const RTBKIT::AugmentationRequest& request)
{
    recordHit("requests");

    RTBKIT::AugmentationList result;

    for (const string& agent : request.agents) {

        RTBKIT::AgentConfigEntry config = agentConfig.getAgentEntry(agent);
        bool toSkip = false;
        bool dataExist = false;
        SpentMoney_t spentMoney;

	
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
        //std::cerr << "DEBUG: " << "account: " << account << std::endl;

        dataExist = storage->isKeyExist(account);
        if(dataExist) {
            //std::cerr << "DEBUG: found key: " << account << std::endl;
            spentMoney = storage->getSpentMoney(account);
            
            RTBKIT::Amount maxPerHour = getMaxPerHour(request.augmentor, agent, config);
            RTBKIT::Amount maxPerDay = getMaxPerDay(request.augmentor, agent, config);
            //std::cerr << "DEBUG: maxPerHour: " << maxPerHour.toString() << ", maxPerDay: " << maxPerDay.toString() << std::endl;
        
            std::time_t cur_tm = std::time(nullptr);
            std::tm cur_utc_tm = *std::gmtime(&cur_tm);
        
            std::time_t sav_tmPerHour = std::chrono::system_clock::to_time_t(spentMoney.startHourPeriod);
            std::tm sav_utc_tmPerHour = *std::gmtime(&sav_tmPerHour);
            std::time_t sav_tmPerDay = std::chrono::system_clock::to_time_t(spentMoney.startDayPeriod);
            std::tm sav_utc_tmPerDay = *std::gmtime(&sav_tmPerDay);
            
            if(cur_utc_tm.tm_hour != sav_utc_tmPerHour.tm_hour) {
                spentMoney.startHourPeriod = std::chrono::system_clock::now();
                spentMoney.amountPerHour = Amount("USD/1M", 0);
                //std::cerr << "DEBUG: Reset hour amount" << std::endl;
                storage->setSpentMoney(account, spentMoney);
            }
            else {
                //std::cerr << "DEBUG: amountPerHour: " << spentMoney.amountPerHour.toString() << std::endl;
                if(!maxPerHour.isNegative() && spentMoney.amountPerHour >= maxPerHour)
                    toSkip = true;
            }
            if(cur_utc_tm.tm_mday != sav_utc_tmPerHour.tm_mday) {
                spentMoney.startDayPeriod = std::chrono::system_clock::now();
                spentMoney.amountPerDay = Amount("USD/1M", 0);
                //std::cerr << "DEBUG: Reset day amount" << std::endl;
                storage->setSpentMoney(account, spentMoney);
            }
            else {
                //std::cerr << "DEBUG: amountPerDay: " << spentMoney.amountPerDay.toString() << std::endl;
                if(!maxPerDay.isNegative() && spentMoney.amountPerDay >= maxPerDay)
                    toSkip = true;
            }
        }
        if(!toSkip) {
            result[account].tags.insert("pass-spending-money-control");
            recordHit("accounts." + account[0] + ".passed");
            //std::cerr << "DEBUG: passed" << std::endl;
        }
        else {
            recordHit("accounts." + account[0] + ".toofften");
            //std::cerr << "DEBUG: skipped" << std::endl;
        }
    }

    return result;
}


/** Returns the maximum amount of money that can be spent in one day

*/
RTBKIT::Amount
FinancialAugmentor::
getMaxPerDay( const string& augmentor,
              const string& agent,
              const RTBKIT::AgentConfigEntry& config) const
{
    RTBKIT::Amount amount("USD/1M", -1000);
    for (const auto& augConfig : config.config->augmentations) {
        if (augConfig.name != augmentor) 
            continue;
        if(augConfig.config.isMember("maxPerDay")) {
            amount = Amount::fromJson(augConfig.config["maxPerDay"]);
        }
    }

    return amount;
}

/** Returns the maximum amount of money that can be spent in one hour

*/
RTBKIT::Amount
FinancialAugmentor::
getMaxPerHour( const string& augmentor,
               const string& agent,
               const RTBKIT::AgentConfigEntry& config) const
{
    RTBKIT::Amount amount("USD/1M", -1000);
    for (const auto& augConfig : config.config->augmentations) {
        if (augConfig.name != augmentor) continue;
        if(augConfig.config.isMember("maxPerHour")) {
            amount = Amount::fromJson(augConfig.config["maxPerHour"]);
        }
    }

    return amount;
}

} // namespace RTBKIT

