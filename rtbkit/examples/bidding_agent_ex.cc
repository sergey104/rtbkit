/** bidding_agent_ex.cc                                 -*- C++ -*-
    Rémi Attab, 22 Feb 2013
    Copyright (c) 2013 Datacratic.  All rights reserved.

    Example of a simple fixed-price bidding agent.

*/

#include "rtbkit/common/bids.h"
#include "rtbkit/core/banker/slave_banker.h"
#include "rtbkit/core/agent_configuration/agent_config.h"
#include "rtbkit/plugins/bidding_agent/bidding_agent.h"
#include "soa/service/service_utils.h"

#include <boost/program_options/cmdline.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/positional_options.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>

#include <iostream>
#include <thread>
#include <chrono>
#include <string>     // std::string, std::to_string


using namespace std;
using namespace ML;

namespace RTBKIT {

/******************************************************************************/
/* FIXED PRICE BIDDING AGENT                                                  */
/******************************************************************************/

/** Simple bidding agent whose sole purpose in life is to bid 0.1$ CPM on every
    bid requests it sees. It also has a very simple pacer which ensures that we
    always have at most 1$ to spend every 10 seconds.

 */
struct FixedPriceBiddingAgent :
        public BiddingAgent
{
    FixedPriceBiddingAgent(
            std::shared_ptr<Datacratic::ServiceProxies> services,
            const string& serviceName) :
        BiddingAgent(services, serviceName),
        accountSetup(false)
    {}


    void init(const std::shared_ptr<ApplicationLayer> appLayer)
    {
        // We only want to specify a subset of the callbacks so turn the
        // annoying safety belt off.
        strictMode(false);

        onBidRequest = bind(
                &FixedPriceBiddingAgent::bid, this, _1, _2, _3, _4, _5, _6);

        // This component is used to speak with the master banker and pace the
        // rate at which we spend our budget.
        budgetController.setApplicationLayer(appLayer);
        budgetController.start();

        // Update our pacer every 10 seconds. Note that since this interacts
        // with the budgetController which is only synced up with the router
        // every few seconds, the wait period shouldn't be set too low.
        addPeriodic("FixedPriceBiddingAgent::pace", 10.0,
                [&] (uint64_t) { this->pace(); });

        BiddingAgent::init();

    }

    void start()
    {
        BiddingAgent::start();

        // Build our configuration and tell the world about it.
        setConfig();

    }

    void shutdown()
    {
        BiddingAgent::shutdown();

        budgetController.shutdown();
    }


    /** Sets up an agent configuration for our example. */
    void setConfig()
    {
        config = AgentConfig();
       // config.locationFilter.include = { L"^ES:.*:.*:((08394))" };
        // Accounts are used to control the allocation of spending budgets for
        // an agent. The whole mechanism is fully generic and can be setup in
        // whatever you feel it bests suits you.
        config.account = {"hello", "world"};

        // Specify the properties of the creatives we are trying to show.
        config.creatives.push_back(Creative::sampleLB);
        config.creatives.push_back(Creative::sampleWS);
        config.creatives.push_back(Creative::sampleBB);
        config.creatives.push_back(Creative::sampleAA);
        config.creatives.push_back(Creative::sampleCC);
        config.creatives.push_back(Creative::sampleDD);
        config.creatives.push_back(Creative::sampleRR);

    //    config.providerConfig["appodeal"]["seat"] = 12;
      //  config.providerConfig["appodeal"]["iurl"] = "http://www.gnu.org";
   //     std::string temp = "<script src=\"mraid.js\"></script><a href=\"http://52.208.150.10:17341/click\" target=\"_blank\"><img width=\"300\" height=\"250\" style=\"border-style: none\" src=\"\"/></a><script language=\"javascript\"type=\"text/javascript\">if(mraid.isViewable()){sendImpression();}else{mraid.addEventListener(\"viewableChange\",function(viewable){if(viewable){mraid.removeEventListener(\"viewableChange\",arguments.callee);sendImpression();}});}\nfunction sendImpression(){var urls=new Array();var i=0;urls[i++]=\"http://api.whatzbuzz.net/rtb/view?buzz=gAAAAABW4oe_ZnBIMcAliFv1T5ujGaBN_vCxv6BjFLq31DtNn_WDjuh-iF0QLyOITGXWllUaseOboW5xV3SswbIN5IstgO-vK-vh2S1MUb6hcnnYsaHEXCmZsv-zN8xYVM7jFRRWBLe4dYZYWRC9oi-5xgSWLbGBRt1ooldJ5rL-XK1-i_OT1QcMWJ-yCwpICSYSflv5mMGc&view=pixel.gif\";varhiddenSpan=document.createElement(\"span\");hiddenSpan.style.display=\"none\";var i=0;for(var i=0;i<urls.length;i++){var img=document.createElement(\"img\");img.src=urls[i];hiddenSpan.appendChild(img);}\nvar body=document.getElementsByTagName(\"body\")[0];body.appendChild(hiddenSpan);}</script>";
     std::string temp = "test_adm";

        for(auto & c: config.creatives){
                    c.providerConfig["appodeal"]["adm"] = temp;
                    c.providerConfig["appodeal"]["nurl"] = "http://52.208.150.10:17340?winprice=${AUCTION_PRICE}&auction_id=%{bidrequest.id}&adspot_id=%{creative.id}";
                    c.providerConfig["appodeal"]["iurl"] = "http://amadoad-dev.eu-west-1.elasticbeanstalk.com/_banners/a4/75/a4757c5908c8ed6805d23dd44c8d8098b2f7b28e.png";
                  }

        // Indicate to the router that we want our bid requests to be augmented
        // with our frequency cap augmentor example.
        {
            AugmentationConfig augConfig;

            // Name of the requested augmentor.
            augConfig.name = "frequency-cap-ex";

            // If the augmentor was unable to augment our bid request then it
            // should be filtered before it makes it to our agent.
            augConfig.required = true;

            // Config parameter sent used by the augmentor to determine which
            // tag to set.
            augConfig.config = Json::Value(112);

            // Instruct to router to filter out all bid requests who have not
            // been tagged by our frequency cap augmentor.
            augConfig.filters.include.push_back("pass-frequency-cap-ex");

            config.addAugmentation(augConfig);
        }

        // Configures the agent to only receive 10% of the bid request traffic
        // that matches its filters.
        config.bidProbability = 0.7;

        // Tell the world about our config. We can change the configuration of
        // an agent at any time by calling this function.
        doConfig(config);

    }


    /** Simple fixed price bidding strategy. Note that the router is in charge
        of making sure we stay within budget and don't go bankrupt.
    */
    void bid(
            double timestamp,
            const Id & id,
            std::shared_ptr<RTBKIT::BidRequest> br,
            Bids bids,
            double timeLeftMs,
            const Json::Value & augmentations)
    {



        for (Bid& bid : bids) {

            // In our example, all our creatives are of the different sizes so
            // there should only ever be one biddable creative. Note that that
            // the router won't ask for bids on imp that don't have any
            // biddable creatives.
            ExcAssertEqual(bid.availableCreatives.size(), 1);

            int availableCreative = bid.availableCreatives.front();

            // We don't really need it here but this is how you can get the
            // AdSpot and Creative object from the indexes.
            (void) br->imp[bid.spotIndex];

            (void) config.creatives[availableCreative];

            // Create a 0.0001$ CPM bid with our available creative.
            // Note that by default, the bid price is set to 0 which indicates
            // that we don't wish to bid on the given spot.
            bid.bid(availableCreative, MicroUSD(100));

        }

        // A value that will be passed back to us when we receive the result of
        // our bid.
        Json::Value metadata = 42;

        // Send our bid back to the agent.
        doBid(id, bids, metadata);

    }


    /** Simple pacing scheme which allocates 1$ to spend every period. */
    void pace()
    {
        // We need to register our account once with the banker service.
        if (!accountSetup) {
            accountSetup = true;
            budgetController.addAccountSync(config.account);
        }

        // Make sure we have 1$ to spend for the next period.
        budgetController.topupTransferSync(config.account, USD(1));
    }


    AgentConfig config;

    bool accountSetup;
    SlaveBudgetController budgetController;
};

} // namepsace RTBKIT


/******************************************************************************/
/* MAIN                                                                       */
/******************************************************************************/

int main(int argc, char** argv)
{
    using namespace boost::program_options;

    Datacratic::ServiceProxyArguments args;
    RTBKIT::SlaveBankerArguments bankerArgs;

    options_description options = args.makeProgramOptions();
    options.add_options()
        ("help,h", "Print this message");
    options.add(bankerArgs.makeProgramOptions());

    variables_map vm;
    store(command_line_parser(argc, argv).options(options).run(), vm);
    notify(vm);

    if (vm.count("help")) {
        cerr << options << endl;
        return 1;
    }

    auto serviceProxies = args.makeServiceProxies();
    RTBKIT::FixedPriceBiddingAgent agent(serviceProxies, "fixed-price-agent-ex");
    agent.init(bankerArgs.makeApplicationLayer(serviceProxies));
    agent.start();

    while (true) this_thread::sleep_for(chrono::seconds(10));

    // Won't ever reach this point but this is how you shutdown an agent.
    agent.shutdown();

    return 0;
}

