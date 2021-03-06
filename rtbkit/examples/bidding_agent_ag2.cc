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
            const string& serviceName, const string& cfg) :
        BiddingAgent(services, serviceName), 
        accountSetup(false), cfgfile(cfg)
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
	
	if(!cfgfile.empty()) {
	    try {
		std::ifstream cf(cfgfile);
		std::string cfg((std::istreambuf_iterator<char>(cf)),  std::istreambuf_iterator<char>());	
		config.parse(cfg);
	    }
	    catch(const std::exception& ex) {
		std::cerr << ML::format("threw exception: %s", ex.what());
        exit(-1);
        }
	    catch(...) {
		exit(-2);
	    }
	}


	
        // Accounts are used to control the allocation of spending budgets for
        // an agent. The whole mechanism is fully generic and can be setup in
        // whatever you feel it bests suits you.
        config.account = {"main", "three"};

        // Specify the properties of the creatives we are trying to show.
        config.creatives.push_back(Creative::sample3S);
        config.creatives.push_back(Creative::sample4S);
      /*  config.creatives.push_back(Creative::sampleBB);
        config.creatives.push_back(Creative::sampleAA);
        config.creatives.push_back(Creative::sampleCC);
        config.creatives.push_back(Creative::sampleDD);
        config.creatives.push_back(Creative::sampleRR);
        config.creatives.push_back(Creative::sample1);
        config.creatives.push_back(Creative::sample2);
        config.creatives.push_back(Creative::sample3);
        config.creatives.push_back(Creative::sample4); */

        config.providerConfig["appodeal"]["seat"] = 12;

	// form _ex 
	
        std::string mr = R"(<script src='mraid.js'></script>)";

        std::string s1 = R"(<script> var impressionTrackers = ["http://nurl.5kszypekn4.eu-west-1.elasticbeanstalk.com/?action=event&user=default&type=IMPRESSION&auctionId=${AUCTION_ID}&bidRequestId=${AUCTION_ID}&impId=${AUCTION_IMP_ID}&winPrice=${AUCTION_PRICE}"]; var clickTrackers = ["http://nurl.5kszypekn4.eu-west-1.elasticbeanstalk.com/?action=event&user=default&type=CLICK&auctionId=${AUCTION_ID}&bidRequestId=${AUCTION_ID}&impId=${AUCTION_IMP_ID}&winPrice=${AUCTION_PRICE}"]; var targetLink = "http://178.124.156.242:17343?auctionId=${AUCTION_ID}&bidRequestId=${AUCTION_BID_ID}&impId=${AUCTION_IMP_ID}&winPrice=${AUCTION_PRICE}"; var trackClick = function() {  sendClicks(); mraid.open(targetLink); }; )";

        std::string s2 = R"(var showAd = function(){if (mraid.isViewable()) { sendImpression(); } else { mraid.addEventListener('viewableChange', function (viewable) {  if (viewable) {  mraid.removeEventListener('viewableChange', showAd); sendImpression(); }  }); } }; )";

        std::string s3 = R"(var sendClicks = function() { var hiddenSpan = document.createElement('span'); hiddenSpan.style.display ='none'; clickTrackers.forEach(function(tracker) { var img = document.createElement('img'); img.src = tracker;  hiddenSpan.appendChild(img);  document.body.appendChild(hiddenSpan);  });    }; )";

        std::string s4 = R"( var sendImpression = function() { var hiddenSpan = document.createElement('span'); hiddenSpan.style.display = 'none'; impressionTrackers.forEach(function(tracker) { var img = document.createElement('img');  img.src = tracker; hiddenSpan.appendChild(img); document.body.appendChild(hiddenSpan);  });  }; )";

        std::string s5 = R"(if (mraid.getState() === 'loading') { mraid.addEventListener('ready', showAd); } else { showAd();  }</script>)";

        std::string img1 = R"(<img style='height: 100%; width: auto;' src='http://ec2-54-194-239-30.eu-west-1.compute.amazonaws.com/banners/Banner1-1024x768.jpg' onclick='trackClick()'> )";
        std::string img2 = R"(<img style='height: 100%; width: auto;' src='http://ec2-54-194-239-30.eu-west-1.compute.amazonaws.com/banners/Banner1-768x1024.jpgg' onclick='trackClick()'> )";


        for(auto & c: config.creatives){

                    std::string s = c.name;
                    if(s == "Extra3") {
                    c.providerConfig["appodeal"]["adm"] = mr + s1 + s2 + s3 + s4 + s5 +img1;
                    c.providerConfig["appodeal"]["iurl"] = "<a href='http://ef-fi.by'><img src=http://ec2-54-194-239-30.eu-west-1.compute.amazonaws.com/banners/Banner1-1024x768.jpg'</a>";

                    }
                    else {
                    c.providerConfig["appodeal"]["adm"] = mr + s1 + s2 + s3 + s4 + s5 +img2;
                    c.providerConfig["appodeal"]["iurl"] = "<a href='http://ef-fi.by'><img src=http://ec2-54-194-239-30.eu-west-1.compute.amazonaws.com/banners/Banner1-768x1024.jpg'</a>";

                    }




                    c.providerConfig["appodeal"]["uniq_id"] = "rec1:" + s;

                    c.providerConfig["appodeal"]["nurl"] = "http://nurl.5kszypekn4.eu-west-1.elasticbeanstalk.com/?action=nurl&user=default&auctionId=${AUCTION_ID}&bidRequestId=${AUCTION_ID}&impId=${AUCTION_IMP_ID}&winPrice=${AUCTION_PRICE}";

                    c.providerConfig["appodeal"]["group_class"] = "group_class";

                }
	 
	
	// from _ag
	/*
        std::string mr = R"(<script src='mraid.js'></script>)";

        std::string s1 = R"(<script> var impressionTrackers = ["http://amadoad-dev.eu-west-1.elasticbeanstalk.com/api/v1/event?type=IMPRESSION&auctionId=${AUCTION_ID}&bidRequestId=${AUCTION_BID_ID}&impId=${AUCTION_IMP_ID}&winPrice=${AUCTION_PRICE}"]; var clickTrackers = ["http://amadoad-dev.eu-west-1.elasticbeanstalk.com/api/v1/event?type=CLICK&auctionId=${AUCTION_ID}&bidRequestId=${AUCTION_BID_ID}&impId=${AUCTION_IMP_ID}&winPrice=${AUCTION_PRICE}"]; var targetLink = "http://178.124.156.242:17343?auctionId=${AUCTION_ID}&bidRequestId=${AUCTION_BID_ID}&impId=${AUCTION_IMP_ID}&winPrice=${AUCTION_PRICE}"; var trackClick = function() {  sendClicks(); mraid.open(targetLink); }; )";

        std::string s2 = R"(var showAd = function(){if (mraid.isViewable()) { sendImpression(); } else { mraid.addEventListener('viewableChange', function (viewable) {  if (viewable) {  mraid.removeEventListener('viewableChange', showAd); sendImpression(); }  }); } }; )";

        std::string s3 = R"(var sendClicks = function() { var hiddenSpan = document.createElement('span'); hiddenSpan.style.display ='none'; clickTrackers.forEach(function(tracker) { var img = document.createElement('img'); img.src = tracker;  hiddenSpan.appendChild(img);  document.body.appendChild(hiddenSpan);  });    }; )";

        std::string s4 = R"( var sendImpression = function() { var hiddenSpan = document.createElement('span'); hiddenSpan.style.display = 'none'; impressionTrackers.forEach(function(tracker) { var img = document.createElement('img');  img.src = tracker; hiddenSpan.appendChild(img); document.body.appendChild(hiddenSpan);  });  }; )";

        std::string s5 = R"(if (mraid.getState() === 'loading') { mraid.addEventListener('ready', showAd); } else { showAd();  }</script>)";

        std::string img = R"(<img style='height: 100%; width: auto;' src='http://amadoad-dev.eu-west-1.elasticbeanstalk.com/_banners/a4/75/a4757c5908c8ed6805d23dd44c8d8098b2f7b28e.png' onclick='trackClick()'> )";

                for(auto & c: config.creatives){
                    c.providerConfig["appodeal"]["adm"] = mr + s1 + s2 + s3 + s4 + s5 +img;

                    std::string s = c.name;

                    int jj = c.id;

                    std::string z = to_string(jj);

                    c.providerConfig["appodeal"]["uniq_id"] = "world:" + s +":"+z;

                    c.providerConfig["appodeal"]["nurl"] = "http://amadoad-dev.eu-west-1.elasticbeanstalk.com/api/v1/nurl?auctionId=${AUCTION_ID}&bidRequestId=${AUCTION_BID_ID}&impId=${AUCTION_IMP_ID}&winPrice=${AUCTION_PRICE}";

                    c.providerConfig["appodeal"]["iurl"] = "http://amadoad-dev.eu-west-1.elasticbeanstalk.com/_banners/a4/75/a4757c5908c8ed6805d23dd44c8d8098b2f7b28e.png";

                  }
        */
	
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
            augConfig.config = Json::Value(5);

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
	/*
	{
	    // logging agent configuration
	    std::cerr << "--- <configuration> ---" << std::endl;
	    Json::Value value = config.toJson();
	    std::cerr << value << std::endl << "--- </configuration> ---" << std::endl;
	}
        */
	    
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
            ExcAssertGreaterEqual(bid.availableCreatives.size(), 1);
	    /*
            cerr << "-----" << endl;
            cerr << "size: " << bid.availableCreatives.size() << endl;


            for (auto& z : bid.availableCreatives) {


            cerr << "index:" << z << ";  spotindex: " << bid.spotIndex << endl;
            }
            cerr << "-----" << endl;
	    */
            std::random_shuffle (  bid.availableCreatives.begin(),  bid.availableCreatives.end() );
            int availableCreative = bid.availableCreatives.front();

            // We don't really need it here but this is how you can get the
            // AdSpot and Creative object from the indexes.
            (void) br->imp[bid.spotIndex];

            (void) config.creatives[availableCreative];

            // Create a 0.0001$ CPM bid with our available creative.
            // Note that by default, the bid price is set to 0 which indicates
            // that we don't wish to bid on the given spot.
            bid.bid(availableCreative, USD_CPM(0.02));

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
    //
    std::string cfgfile;
};

} // namepsace RTBKIT

volatile bool to_shutdown = false;

// signal handler
static void sig_handler(int signal)
{
    switch(signal)
    {
	case SIGTERM:
	case SIGKILL:
	case SIGINT:
	    to_shutdown = true;
	    break;
	default:
	    break;
    }
}

/******************************************************************************/
/* MAIN                                                                       */
/******************************************************************************/

int main(int argc, char** argv)
{
    using namespace boost::program_options;
    string cfgfile;

    Datacratic::ServiceProxyArguments args;
    RTBKIT::SlaveBankerArguments bankerArgs;

    options_description options = args.makeProgramOptions();
    options.add_options()
        ("help,h", "Print this message");
	
    // add option "-f" description
    options.add_options()
	(",f", value(&cfgfile), "Configuration file");
	
    options.add(bankerArgs.makeProgramOptions());
    


    variables_map vm;
    store(command_line_parser(argc, argv).options(options).run(), vm);
    notify(vm);

    if (vm.count("help")) {
        cerr << options << endl;
        return 1;
    }

    auto serviceProxies = args.makeServiceProxies();
    RTBKIT::FixedPriceBiddingAgent agent(serviceProxies, "fixed-price-agent-ex", cfgfile);
    agent.init(bankerArgs.makeApplicationLayer(serviceProxies));

    signal(SIGKILL, sig_handler);
    signal(SIGTERM, sig_handler);
    signal(SIGINT, sig_handler);
    
    agent.start();

    // ouput pid to stderr
    std::cerr << "pid:" << getpid();
    
    while (!to_shutdown) this_thread::sleep_for(chrono::seconds(10));

    // shutdown
    agent.shutdown();

    return 0;
}

