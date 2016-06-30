
#include "appodeal_event_source.h"
#include "rtbkit/common/auction_events.h"

using namespace RTBKIT;

AppodealEventSource::
AppodealEventSource(NetworkAddress address) :
    EventSource(std::move(address)) {
}


AppodealEventSource::
AppodealEventSource(Json::Value const & json) :
    EventSource(json) {
}


void
AppodealEventSource::
sendImpression(const BidRequest& bidRequest, const Bid& bid)
{
    PostAuctionEvent event;
    event.type = PAE_CAMPAIGN_EVENT;
    event.label = "IMPRESSION";
    event.auctionId = bidRequest.auctionId;
    event.adSpotId = bid.adSpotId;
    event.timestamp = Date::now();
    event.uids = bidRequest.userIds;

    sendEvent(event);
}


void
AppodealEventSource::
sendClick(const BidRequest& bidRequest, const Bid& bid)
{
    PostAuctionEvent event;
    event.type = PAE_CAMPAIGN_EVENT;
    event.label = "CLICK";
    event.auctionId = bidRequest.auctionId;
    event.adSpotId = bid.adSpotId;
    event.timestamp = Date::now();
    event.uids = bidRequest.userIds;

    sendEvent(event);
}


void
AppodealEventSource::
sendEvent(const PostAuctionEvent& event)
{
    std::string str = event.toJson().toString();
    std::string httpRequest = ML::format(
            "POST /win HTTP/1.1\r\n"
            "Content-Length: %zd\r\n"
            "Content-Type: application/json\r\n"
            "Connection: Keep-Alive\r\n"
            "\r\n"
            "%s",
            str.size(),
            str.c_str());

    write(httpRequest);

    std::string result = read();
    std::string status = "HTTP/1.1 200 OK";

    if(result.compare(0, status.length(), status)) {
        std::cerr << result << std::endl;
    }
}

namespace {

struct AtInit {
    AtInit()
    {
      PluginInterface<EventSource>::registerPlugin("appodeal",
                                                   [](Json::Value const & json) {
            return new AppodealEventSource(json);
        });
    }
} atInit;

}

