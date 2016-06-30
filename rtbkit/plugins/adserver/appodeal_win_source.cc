
#include "appodeal_win_source.h"
#include "rtbkit/common/auction_events.h"

using namespace RTBKIT;

AppodealWinSource::
AppodealWinSource(NetworkAddress address) :
    WinSource(std::move(address)) {
}


AppodealWinSource::
AppodealWinSource(Json::Value const & json) :
    WinSource(json) {
}

void
AppodealWinSource::
sendWin(const BidRequest& bidRequest, const Bid& bid, const Amount& winPrice)
{
    PostAuctionEvent event;
    event.type = PAE_WIN;
    event.auctionId = bidRequest.auctionId;
    event.adSpotId = bid.adSpotId;
    event.timestamp = Date::now();
    event.winPrice = winPrice;
    event.uids = bidRequest.userIds;
    event.account = bid.account;
    event.bidTimestamp = bid.bidTimestamp;

    sendEvent(event);
}



void
AppodealWinSource::
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
        PluginInterface<WinSource>::registerPlugin("appodeal",
                                                   [](Json::Value const & json) {
            return new AppodealWinSource(json);
        });
    }
} atInit;

}

