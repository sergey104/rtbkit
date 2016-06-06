 /* Appodeal_exchange_connector.cc
   Eric Robert, 7 May 2013

   Implementation of the OpenRTB exchange connector.
*/

#include "appodeal_exchange_connector.h"
#include "rtbkit/common/testing/exchange_source.h"
#include "rtbkit/plugins/bid_request/openrtb_bid_source.h"
#include "rtbkit/plugins/bid_request/openrtb_bid_request_parser.h"
#include "rtbkit/plugins/exchange/http_auction_handler.h"
#include "rtbkit/core/agent_configuration/agent_config.h"
#include "rtbkit/openrtb/openrtb_parsing.h"
#include "soa/types/json_printing.h"
#include <boost/any.hpp>
#include <boost/lexical_cast.hpp>
#include "jml/utils/file_functions.h"
#include "jml/arch/info.h"
#include "jml/utils/rng.h"
#include <sstream>


const std::string bid_sample_filename("/home/ubuntu/rtbkit/rtbkit/plugins/exchange/testing/appodealtestresponse.json");


std::string loadFile(const std::string & filename)
{
    ML::filter_istream stream(filename);

    std::string result;

    while (stream) {
        std::string line;
        getline(stream, line);
        result += line + "\n";
    }

    return result;
}

long int unix_timestamp()
{
    time_t t = std::time(0);
    long int now = static_cast<long int> (t);
    return now;
}
using namespace Datacratic;

namespace Datacratic {

template<typename T, int I, typename S>
Json::Value jsonEncode(const ML::compact_vector<T, I, S> & vec)
{
    Json::Value result(Json::arrayValue);
    for (unsigned i = 0;  i < vec.size();  ++i)
        result[i] = jsonEncode(vec[i]);
    return result;
}

template<typename T, int I, typename S>
ML::compact_vector<T, I, S>
jsonDecode(const Json::Value & val, ML::compact_vector<T, I, S> *)
{
    ExcAssert(val.isArray());
    ML::compact_vector<T, I, S> res;
    res.reserve(val.size());
    for (unsigned i = 0;  i < val.size();  ++i)
        res.push_back(jsonDecode(val[i], (T*)0));
    return res;
}

} // namespace Datacratic

namespace OpenRTB {

template<typename T>
Json::Value jsonEncode(const OpenRTB::List<T> & vec)
{
    using Datacratic::jsonEncode;
    Json::Value result(Json::arrayValue);
    for (unsigned i = 0;  i < vec.size();  ++i)
        result[i] = jsonEncode(vec[i]);
    return result;
}

template<typename T>
OpenRTB::List<T>
jsonDecode(const Json::Value & val,OpenRTB::List<T> *)
{
    using Datacratic::jsonDecode;
    ExcAssert(val.isArray());
    OpenRTB::List<T> res;
    res.reserve(val.size());
    for (unsigned i = 0;  i < val.size();  ++i)
        res.push_back(jsonDecode(val[i], (T*)0));
    return res;
}

} // namespace Appodeal

namespace RTBKIT {

BOOST_STATIC_ASSERT(hasFromJson<Datacratic::Id>::value == true);
BOOST_STATIC_ASSERT(hasFromJson<int>::value == false);

/*****************************************************************************/
/* Appodeal EXCHANGE CONNECTOR                                                */
/*****************************************************************************/

AppodealExchangeConnector::
AppodealExchangeConnector(ServiceBase & owner, const std::string & name)
    : HttpExchangeConnector(name, owner)
{
}

AppodealExchangeConnector::
AppodealExchangeConnector(const std::string & name,
                         std::shared_ptr<ServiceProxies> proxies)
    : HttpExchangeConnector(name, proxies)
{
}

std::shared_ptr<BidRequest>
AppodealExchangeConnector::
parseBidRequest(HttpAuctionHandler & connection,
                const HttpHeader & header,
                const std::string & payload)
{

  ofstream myfile;
  myfile.open ("/home/ubuntu/appodeal.txt", std::ofstream::app);
  myfile << "Writing this to a file inside appodeal parser.\n";
  myfile << header;
  myfile <<"\n";
  myfile << payload;
  myfile <<"\n";
    std::shared_ptr<BidRequest> none;

    // Check for JSON content-type
    if (!header.contentType.empty()) {
        static const std::string delimiter = ";";

        std::string::size_type posDelim = header.contentType.find(delimiter);
        std::string content;

        if(posDelim == std::string::npos)
            content = header.contentType;
        else {
            content = header.contentType.substr(0, posDelim);
            #if 0
            std::string charset = header.contentType.substr(posDelim, header.contentType.length());
            #endif
        }

        if(content != "application/json") {
            connection.sendErrorResponse("UNSUPPORTED_CONTENT_TYPE", "The request is required to use the 'Content-Type: application/json' header");
            myfile << "UNSUPPORTED_CONTENT_TYPE\n";
            return none;
        }
    }
    else {
        connection.sendErrorResponse("MISSING_CONTENT_TYPE_HEADER", "The request is missing the 'Content-Type' header");
        myfile << "MISSING_CONTENT_TYPE_HEADER\n";
        return none;
    }

    // Check for the x-openrtb-version header
    auto it = header.headers.find("x-openrtb-version");
  //  if (it == header.headers.end()) {
  //      connection.sendErrorResponse("MISSING_OPENRTB_HEADER", "The request is missing the 'x-openrtb-version' header");
  //      myfile << "MISSING_OPENRTB_HEADER\n";
  //      return none;
  //  }

    // Check that it's version 2.1
  //  std::string openRtbVersion = "it->second";
    std::string openRtbVersion = "2.2";
  //  if (openRtbVersion != "2.1" && openRtbVersion != "2.2") {
   //     connection.sendErrorResponse("UNSUPPORTED_OPENRTB_VERSION", "The request is required to be using version 2.1 or 2.2 of the OpenRTB protocol but requested " + openRtbVersion);
  //      myfile << "UNSUPPORTED_OPENRTB_VERSION\n";
  //      return none;
  //  }

    if(payload.empty()) {
        this->recordHit("error.emptyBidRequest");
        connection.sendErrorResponse("EMPTY_BID_REQUEST", "The request is empty");
        myfile << "EMPTY_BID_REQUEST\n";
        return none;
    }
    myfile << "Start parsing appodeal parser.\n";

    // Parse the bid request
    std::shared_ptr<BidRequest> result;
    try {
        ML::Parse_Context context("Bid Request", payload.c_str(), payload.size());
        result.reset(OpenRTBBidRequestParser::openRTBBidRequestParserFactory(openRtbVersion)->parseBidRequest(context,
                                                                                              exchangeName(),
                                                                                              exchangeName()));
    }
    catch(ML::Exception const & e) {
        this->recordHit("error.parsingBidRequest");
        throw;
    }
    catch(...) {
        throw;
    }

    // Check if we want some reporting
    auto verbose = header.headers.find("x-openrtb-verbose");
    if(header.headers.end() != verbose) {
        if(verbose->second == "1") {
            if(!result->auctionId.notNull()) {
                connection.sendErrorResponse("MISSING_ID", "The bid request requires the 'id' field");
                return none;
            }
        }
    }
    ofstream idfile;
    idfile.open ("/home/ubuntu/id.txt");
    idfile << result->auctionId;
    idfile.close();
myfile << "AUC_ID:" << result->auctionId << "\n";
myfile.close();

    return result;
}

double
AppodealExchangeConnector::
getTimeAvailableMs(HttpAuctionHandler & connection,
                   const HttpHeader & header,
                   const std::string & payload)
{
    // Scan the payload quickly for the tmax parameter.
    static const std::string toFind = "\"tmax\":";
    std::string::size_type pos = payload.find(toFind);
    if (pos == std::string::npos)
        return 30.0;

    int tmax = atoi(payload.c_str() + pos + toFind.length());
    return (absoluteTimeMax < tmax) ? absoluteTimeMax : tmax;
}

/* HttpResponse
AppodealExchangeConnector::
getResponse(const HttpAuctionHandler & connection,
            const HttpHeader & requestHeader,
            const Auction & auction) const
{
  const Auction::Data * current = auction.getCurrentData();

    if (current->hasError())
        return getErrorResponse(connection,
                                current->error + ": " + current->details);

    OpenRTB::BidResponse response;
    response.id = auction.id;

    response.ext = getResponseExt(connection, auction);

    // Create a spot for each of the bid responses
    for (unsigned spotNum = 0; spotNum < current->responses.size(); ++spotNum) {
        if (!current->hasValidResponse(spotNum))
            continue;

        setSeatBid(auction, spotNum, response);
    }

    if (response.seatbid.empty())
        return HttpResponse(204, "none", "");

    static Datacratic::DefaultDescription<OpenRTB::BidResponse> desc;
    std::ostringstream stream;

    StreamJsonPrintingContext context(stream);
    desc.printJsonTyped(&response, context);
    ofstream myfile;
    myfile.open ("/home/fil/appodealresponse.txt", std::ofstream::app);
    myfile << stream.str() << "\n";
    myfile.close();
    return HttpResponse(200, "application/json", stream.str());

}
*/
HttpResponse
AppodealExchangeConnector::
getResponse(const HttpAuctionHandler & connection,
            const HttpHeader & requestHeader,
            const Auction & auction) const
{

  std::string strJson = loadFile(bid_sample_filename);
  std::string ids = loadFile("/home/fil/id.txt");

 // strJson.replace(54,49,ids.substr(0,ids.length()-2));
  long int now = unix_timestamp();
  std::stringstream ss;
  ss << now;  // or any other type
  std::string result=ss.str();
  strJson.replace(398,10,result);
  ofstream myfile;
  myfile.open ("/home/ubuntu/appodealresponse.txt", std::ofstream::app);
  myfile << "Writing this to a file inside appodeal parser and before sending response.\n";
  myfile << strJson;
  myfile <<"\n";
  myfile << ids;
  myfile <<"\n";
  myfile.close();
  return HttpResponse(200, "application/json", strJson);
}
Json::Value
AppodealExchangeConnector::
getResponseExt(const HttpAuctionHandler & connection,
               const Auction & auction) const
{
    return {};
}

HttpResponse
AppodealExchangeConnector::
getDroppedAuctionResponse(const HttpAuctionHandler & connection,
                          const std::string & reason) const
{
  ofstream myfile;
  myfile.open ("/home/ubuntu/appodealresponse.txt", std::ofstream::app);
  myfile << "----NONE 204." ;
  myfile << "    reason: ";
  myfile << reason;
  myfile << "\n" ;

  myfile.close();
  return HttpResponse(204, "none", "");
}

HttpResponse
AppodealExchangeConnector::
getErrorResponse(const HttpAuctionHandler & connection,
                 const std::string & message) const
{
    Json::Value response;
    response["error"] = message;
    ofstream myfile;
    myfile.open ("/home/ubuntu/appodealresponse.txt", std::ofstream::app);
    myfile << "----400.\n";
    myfile << "message:  " << message << "\n";
    myfile.close();
    return HttpResponse(400, response);
}

std::string
AppodealExchangeConnector::
getBidSourceConfiguration() const
{
    auto suffix = std::to_string(port());
    return ML::format("{\"type\":\"openrtb\",\"url\":\"%s\"}",
                      ML::fqdn_hostname(suffix) + ":" + suffix);
}

void
AppodealExchangeConnector::
setSeatBid(Auction const & auction,
           int spotNum,
           OpenRTB::BidResponse & response) const
{
    const Auction::Data * data = auction.getCurrentData();

    // Get the winning bid
    auto & resp = data->winningResponse(spotNum);

    int seatIndex = 0;
    if(response.seatbid.empty()) {
        response.seatbid.emplace_back();
    }

    OpenRTB::SeatBid & seatBid = response.seatbid.at(seatIndex);

    // Add a new bid to the array
    seatBid.bid.emplace_back();

    // Put in the variable parts
    auto & b = seatBid.bid.back();
    b.cid = Id(resp.agentConfig->externalId);
    b.crid = Id(resp.creativeId);
    b.id = Id(auction.id, auction.request->imp[0].id);
    b.impid = auction.request->imp[spotNum].id;
    b.price.val = getAmountIn<CPM>(resp.price.maxPrice);
}

} // namespace RTBKIT

namespace {
    using namespace RTBKIT;

    struct AtInit {
        AtInit() {
            ExchangeConnector::registerFactory<AppodealExchangeConnector>();
        }
    } atInit;
}

