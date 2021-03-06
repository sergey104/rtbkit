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
        std::cout << "pid:" << getpid() << std::endl;
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
cerr << "bidding" << endl;
        // Accounts are used to control the allocation of spending budgets for
        // an agent. The whole mechanism is fully generic and can be setup in
        // whatever you feel it bests suits you.
        config.account = {"hello", "world"};

        // Specify the properties of the creatives we are trying to show.
        config.creatives.push_back(Creative::sample1S);
        config.creatives.push_back(Creative::sample2S);
        config.creatives.push_back(Creative::sample3S);
        config.creatives.push_back(Creative::sample4S);
        config.creatives.push_back(Creative::sample5S);
        config.creatives.push_back(Creative::sample6S);
        config.creatives.push_back(Creative::sample7S);
        config.creatives.push_back(Creative::sample8S);
        config.creatives.push_back(Creative::sample9S);
        config.creatives.push_back(Creative::sample10S);
        config.creatives.push_back(Creative::sample11S);
        config.creatives.push_back(Creative::sample1A);
        //config.creatives.push_back(Creative::sample2A);


      //  config.providerConfig["smaato"]["iurl"] = "http://www.gnu.org";
        std::string mr = R"(<script src='mraid.js'></script>)";

        std::string z1 = R"(<script> var impressionTrackers = ["http://54.229.80.225/?action=event&user=fil&type=IMPRESSION&auctionId=${AUCTION_ID}&bidRequestId=${AUCTION_ID}&impId=${AUCTION_IMP_ID}&winPrice=${AUCTION_PRICE}"]; var clickTrackers = ["http://54.229.80.225/?action=event&user=fil&type=CLICK&auctionId=${AUCTION_ID}&bidRequestId=${AUCTION_BID_ID}&impId=${AUCTION_IMP_ID}&winPrice=${AUCTION_PRICE}"]; var targetLink = "http://178.124.156.242:17343?auctionId=${AUCTION_ID}&bidRequestId=${AUCTION_BID_ID}&impId=${AUCTION_IMP_ID}&winPrice=${AUCTION_PRICE}"; var trackClick = function() {  sendClicks(); mraid.open(targetLink); }; )";

        std::string z2 = R"(var showAd = function(){if (mraid.isViewable()) { sendImpression(); } else { mraid.addEventListener('viewableChange', function (viewable) {  if (viewable) {  mraid.removeEventListener('viewableChange', showAd); sendImpression(); }  }); } }; )";

        std::string z3 = R"(var sendClicks = function() { var hiddenSpan = document.createElement('span'); hiddenSpan.style.display ='none'; clickTrackers.forEach(function(tracker) { var img = document.createElement('img'); img.src = tracker;  hiddenSpan.appendChild(img);  document.body.appendChild(hiddenSpan);  });    }; )";

        std::string z4 = R"( var sendImpression = function() { var hiddenSpan = document.createElement('span'); hiddenSpan.style.display = 'none'; impressionTrackers.forEach(function(tracker) { var img = document.createElement('img');  img.src = tracker; hiddenSpan.appendChild(img); document.body.appendChild(hiddenSpan);  });  }; )";

        std::string z5 = R"(if (mraid.getState() === 'loading') { mraid.addEventListener('ready', showAd); } else { showAd();  }</script>)";

        std::string img = R"(<img style='height: 100%; width: auto;' src='https://devdash.amadoad.com/_banners/d6/f8/d6f8c637a1dc7933dc602dd6ce1c51e7cd69a316.gif' onclick='trackClick()'> )";

        std::string s1 = R"(<ad xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:noNamespaceSchemaLocation="smaato_ad_v0.9.xsd" modelVersion="0.9"><imageAd>)";
        std::string s2 = R"(<clickUrl>http://reports.ubimo.com/fb?b=JdZQFdbCARgKMURHWGhvUVl0bSMBJeAhAA&amp;c=Mzo6&amp;)";
        std::string s3 = R"(t=https%3A%2F%2Fad.doubleclick.net%2Fddm%2Fclk%2F292804678%3B119963336%3Bw%3Fhttp%3A%2F%2Fwww.academy.com%2)";
        std::string s4 = R"(Fwebapp%2Fwcs%2Fstores%2Fservlet%2FContainer_10151_10051_-1_%3Fname%3DOfficial_Rules%26uv%3Dvanity%3Aofficialrules</clickUrl>)";
        std::string s5 = R"(<imgUrl>http://static.ubimo.com/io/603/ecd01dce</imgUrl>)";
        std::string s6 = R"(<height>50</height>)";
        std::string s7 = R"(<width>300</width>)";
        std::string s8 = R"(<beacons>)";
        std::string s9 = R"(<beacon>http://reports.ubimo.com/fb?b=JdZQFdbCARgKMURHWGhvUVl0bSMBJeAhAA&amp;c=Mjo6</beacon>)";
        std::string s10 = R"(<beacon>https://ad.doubleclick.net/ddm/ad/N5865.276855.MOBILEFUSE/B8852634.119963336;sz=1x1;ord=1436319256367</beacon>)";
        std::string s11 = R"(</beacons>)";
        std::string s12 = R"(</imageAd>)";
        std::string s13 = R"(</ad>)";
                for(auto & c: config.creatives){
                  //  c.providerConfig["smaato"]["adm"] = mr + s1 + s2 + s3 + s4 + s5 +img;
                    switch (c.id) {
                    case 0:
                         s6 = R"(<height>50</height>)";
                         s7 = R"(<width>300</width>)";
                        break;
                    case 1:
                         s6 = R"(<height>50</height>)";
                         s7 = R"(<width>300</width>)";
                        break;
                    case 2:
                        s6 = R"(<height>36</height>)";
                         s7 = R"(<width>216</width>)";
                        break;
                    case 3:
                         s6 = R"(<height>36</height>)";
                         s7 = R"(<width>216</width>)";
                        break;
                    case 4:
                         s6 = R"(<height>20</height>)";
                         s7 = R"(<width>120</width>)";
                        break;
                    case 5:
                         s6 = R"(<height>20</height>)";
                         s7 = R"(<width>120</width>)";
                        break;
                    case 6:
                         s6 = R"(<height>28</height>)";
                         s7 = R"(<width>168</width>)";
                        break;
                    case 7:
                         s6 = R"(<height>28</height>)";
                         s7 = R"(<width>168</width>)";
                        break;
                    case 8:
                         s6 = R"(<height>50</height>)";
                         s7 = R"(<width>320</width>)";
                        break;
                    case 9:
                         s6 = R"(<height>50</height>)";
                         s7 = R"(<width>320</width>)";
                        break;
                    case 10:
                         s6 = R"(<height>50</height>)";
                         s7 = R"(<width>320</width>)";
                        break;
                    case 11:
                         s6 = R"(<height>50</height>)";
                         s7 = R"(<width>320</width>)";
                        break;
                    case 12:
                         s6 = R"(<height>90</height>)";
                         s7 = R"(<width>728</width>)";
                        break;
                    default:
                         s6 = R"(<height>50</height>)";
                        s7 = R"(<width>320</width>)";
                        break;

                    }

                  if(c.id <= 10)  {
                      config.providerConfig["smaato"]["seat"] = "12";
                    c.providerConfig["smaato"]["adm"] = s1 + s2 + s3 + s4 + s5 +s6 + s7 + s8 + s9 + s10 +s11 + s12 + s13;


                    std::string s = c.name;


                    c.providerConfig["smaato"]["uniq_id"] = "world:" + s;

                    c.providerConfig["smaato"]["nurl"] = "http://54.229.80.225?action=nurl&user=fil&auctionId=${AUCTION_ID}&bidRequestId=${AUCTION_ID}&impId=${AUCTION_IMP_ID}&winPrice=${AUCTION_PRICE}";


                    c.providerConfig["smaato"]["iurl"] = "http://amadoad-dev.eu-west-1.elasticbeanstalk.com/_banners/a4/75/a4757c5908c8ed6805d23dd44c8d8098b2f7b28e.png";

                    c.providerConfig["smaato"]["group_class"] = "group_class";
                    c.providerConfig["smaato"]["adid"] = "123476";
                    c.providerConfig["smaato"]["crid"] = "123476";
                    c.providerConfig["smaato"]["adomain"][0] = "adm.com";

                    c.providerConfig["smaato"]["mimeTypes"][0] = "image/jpeg";
                     c.providerConfig["smaato"]["mimeTypes"][0] = "image/png";
                      c.providerConfig["smaato"]["mimeTypes"][0] = "image/gif";
                  }
                  else
                  {
                     config.providerConfig["appodeal"]["seat"] = "12";
                      c.providerConfig["appodeal"]["adm"] = mr + z1 + z2 + z3 + z4 + z5 +img;
                     c.providerConfig["appodeal"]["uniq_id"] = "world:" + c.name;

                     c.providerConfig["appodeal"]["nurl"] = "http://54.229.80.225?action=nurl&user=fil&auctionId=${AUCTION_ID}&bidRequestId=${AUCTION_ID}&impId=${AUCTION_IMP_ID}&winPrice=${AUCTION_PRICE}";


                     c.providerConfig["appodeal"]["iurl"] = "https://devdash.amadoad.com/_banners/d6/f8/d6f8c637a1dc7933dc602dd6ce1c51e7cd69a316.gif";

                     c.providerConfig["appodeal"]["group_class"] = "group_class";
                     c.providerConfig["appodeal"]["adid"] = "123476";
                     c.providerConfig["appodeal"]["crid"] = "123476";
                     c.providerConfig["appodeal"]["adomain"][0] = "adm.com";



                  }

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
            augConfig.config = Json::Value(1880);

            // Instruct to router to filter out all bid requests who have not
            // been tagged by our frequency cap augmentor.
            augConfig.filters.include.push_back("pass-frequency-cap-ex");

        //    config.addAugmentation(augConfig);
        }

        // Configures the agent to only receive 10% of the bid request traffic
        // that matches its filters.
        config.bidProbability = 1.0;
	
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
            ExcAssertGreaterEqual(bid.availableCreatives.size(), 1);
        /*    cerr << "-----" << endl;
            cerr << "size: " << bid.availableCreatives.size() << endl;


            for (auto& z : bid.availableCreatives) {


            cerr << "index:" << z << ";  spotindex: " << bid.spotIndex << endl;
            }
            cerr << "-----" << endl; */
            std::random_shuffle (  bid.availableCreatives.begin(),  bid.availableCreatives.end() );
            int availableCreative = bid.availableCreatives.front();

            // We don't really need it here but this is how you can get the
            // AdSpot and Creative object from the indexes.
            (void) br->imp[bid.spotIndex];

            (void) config.creatives[availableCreative];

            // Create a 0.0001$ CPM bid with our available creative.
            // Note that by default, the bid price is set to 0 which indicates
            // that we don't wish to bid on the given spot.
            bid.bid(availableCreative, USD_CPM(0.88));

        }

        // A value that will be passed back to us when we receive the result of
        // our bid.
        Json::Value metadata = 9142;

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

