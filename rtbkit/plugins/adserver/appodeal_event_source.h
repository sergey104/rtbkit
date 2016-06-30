#pragma once

#include "rtbkit/common/testing/exchange_source.h"

namespace RTBKIT {

struct AppodealEventSource : public EventSource {
    AppodealEventSource(NetworkAddress address);
    AppodealEventSource(Json::Value const & json);

    void sendImpression(const BidRequest& br, const Bid& bid);
    void sendClick(const BidRequest& br, const Bid& bid);

private:
    void sendEvent(const PostAuctionEvent& ev);
};

} // namespace RTBKIT
