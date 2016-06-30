#ifndef APPODEAL_DEBUG_H
#define APPODEAL_DEBUG_H
#include "soa/service/service_utils.h"
#include "soa/service/service_base.h"
#include "soa/service/json_endpoint.h"
#include "soa/service/zmq_named_pub_sub.h"
#include "rtbkit/plugins/adserver/http_adserver_connector.h"
#include "rtbkit/common/auction_events.h"

namespace appodeal_debud {



const std::string bid_sample_filename("/home/fil/rtbkit/rtbkit/plugins/exchange/testing/appodealtestresponse.json");
const std::string bid_sample_filename1("/home/fil/rtbkit/rtbkit/plugins/exchange/testing/appodealtestresponse2.json");


std::string loadFile(const std::string & filename);


long int unix_timestamp();

}











#endif // APPODEAL_DEBUG_H
