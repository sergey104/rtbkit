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
	
	// ouput pid to stderr
	std::cerr << "pid:" << getpid() << std::endl;
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
	    
	//std::cerr << "DEBUG: doConfig" << std::endl;
	//std::cerr << "--- <configuration> ---" << std::endl;
	//Json::Value value = config.toJson();
	//std::cerr << value << std::endl << "--- </configuration> ---" << std::endl;
	
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
            bid.bid(availableCreative, USD_CPM(0.05));

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
    // config file name
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
    RTBKIT::FixedPriceBiddingAgent agent(serviceProxies, "fixed-price-agent-1", cfgfile);
    agent.init(bankerArgs.makeApplicationLayer(serviceProxies));

    signal(SIGKILL, sig_handler);
    signal(SIGTERM, sig_handler);
    signal(SIGINT, sig_handler);
    
    agent.start();

    
    while (!to_shutdown) this_thread::sleep_for(chrono::seconds(10));

    // shutdown
    agent.shutdown();

    return 0;
}

