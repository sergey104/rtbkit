/* Appodeal_exchange_connector.h                                    -*- C++ -*-
   Eric Robert, 7 May 2013
   Copyright (c) 2013 Datacratic Inc.  All rights reserved.

*/
#ifndef APPODEAL_CONNECTOR_H
#define APPODEAL_CONNECTOR_H
#pragma once

#include "rtbkit/plugins/exchange/http_exchange_connector.h"
#include "rtbkit/common/creative_configuration.h"
#include "soa/utils/generic_utils.h"
#include <hiredis/hiredis.h>

namespace RTBKIT {

/*****************************************************************************/
/* Appodeal EXCHANGE CONNECTOR                                                */
/*****************************************************************************/

/** Appodeal exchange connector using the OpenRTB protocol.

    Configuration options are the same as the HttpExchangeConnector on which
    it is based.
*/

struct AppodealExchangeConnector : public HttpExchangeConnector {
	AppodealExchangeConnector(ServiceBase & owner, const std::string & name);
	AppodealExchangeConnector(const std::string & name,
                             std::shared_ptr<ServiceProxies> proxies);

    static std::string exchangeNameString() {
        return "appodeal";
    }

    virtual std::string exchangeName() const {
        return exchangeNameString();
    }

    struct CampaignInfo {
        static constexpr uint64_t MaxSeatValue = 16777215;
        ///< ID of the Casale exchange seat if DSP is used by multiple agencies
        uint64_t seat; // [0, 16777215]
    };

    ExchangeCompatibility
    getCampaignCompatibility(
            const AgentConfig& config,
            bool includeReasons) const;

    ExchangeCompatibility
    getCreativeCompatibility(
            const Creative& creative,
            bool includeReasons) const;


    virtual std::shared_ptr<BidRequest>
    parseBidRequest(HttpAuctionHandler & connection,
                    const HttpHeader & header,
                    const std::string & payload);

    virtual double
    getTimeAvailableMs(HttpAuctionHandler & connection,
                       const HttpHeader & header,
                       const std::string & payload);

    virtual HttpResponse
    getResponse(const HttpAuctionHandler & connection,
                const HttpHeader & requestHeader,
                const Auction & auction) const;

    virtual HttpResponse
    getDroppedAuctionResponse(const HttpAuctionHandler & connection,
                              const std::string & reason) const;

    virtual HttpResponse
    getErrorResponse(const HttpAuctionHandler & connection,
                     const std::string & errorMessage) const;

    virtual std::string getBidSourceConfiguration() const;

    struct CreativeInfo {
        std::string adm = "adm";
        std::string nurl = "nurl";
        std::string iurl = "iurl";
        std::string uniq_id = "uniq_id";
        std::string group_class = "group_class";

    };

    typedef CreativeConfiguration<CreativeInfo> AppodealCreativeConfiguration;


private:

    void initCreativeConfiguration();

    virtual Json::Value
    getResponseExt(const HttpAuctionHandler & connection,
                   const Auction & auction) const;

    AppodealCreativeConfiguration creativeConfig;
    redisContext *rc;
    redisContext *rc1;
    std::string connection = "127.0.0.1";
    int rport = 6379;
    void record_request(std::string s) const;
    void record_response(std::string s) const;

protected:

    virtual void setSeatBid(Auction const & auction,
                            int spotNum,
                            OpenRTB::BidResponse & response) const;
};

} // namespace RTBKIT
#endif
