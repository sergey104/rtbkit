#ifndef APPODEAL_WIN_SOURCE_H
#define APPODEAL_WIN_SOURCE_H
#pragma once

#include "rtbkit/common/testing/exchange_source.h"

namespace RTBKIT {

struct AppodealWinSource : public WinSource {
    AppodealWinSource(NetworkAddress address);
    AppodealWinSource(Json::Value const & json);

    void sendWin(const BidRequest& br,
                 const Bid& bid,
                 const Amount& winPrice);

private:
    void sendEvent(const PostAuctionEvent& ev);
};

} // namespace RTBKIT






#endif // APPODEAL_WIN_SOURCE_H
