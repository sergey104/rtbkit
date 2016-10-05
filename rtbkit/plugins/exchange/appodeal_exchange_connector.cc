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
#include "soa/utils/generic_utils.h"
#include <boost/any.hpp>
#include <boost/lexical_cast.hpp>
#include "jml/utils/file_functions.h"
#include "jml/arch/info.h"
#include "jml/utils/rng.h"
#include <sstream>
#include <ctime>
#include <string>

using namespace std;

void find_and_replace(string& source, string const& find, string const& replace)
{
    for(string::size_type i = 0; (i = source.find(find, i)) != string::npos;)
    {
        source.replace(i, find.length(), replace);
        i += replace.length();
    }
}
long int unix_timestamp()
{
    time_t t = std::time(0);
    long int now = static_cast<long int> (t);
    return now;
}
void writeFile (std::string s) {

  std::ofstream ofs;
  ofs.open ("appodeallog.txt", std::ofstream::out | std::ofstream::app);

  ofs << s << endl;

  ofs.close();

}

using namespace Datacratic;
using namespace ML;
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
    : HttpExchangeConnector(name, owner), creativeConfig("appodeal")
{
  this->auctionResource = "/auctions";
  this->auctionVerb = "POST";
  initCreativeConfiguration();

}

AppodealExchangeConnector::
AppodealExchangeConnector(const std::string & name,
                         std::shared_ptr<ServiceProxies> proxies)
    : HttpExchangeConnector(name, proxies), creativeConfig("appodeal")
{
  this->auctionResource = "/auctions";
  this->auctionVerb = "POST";
  initCreativeConfiguration();

}




void AppodealExchangeConnector::initCreativeConfiguration()
{
    creativeConfig.addField(
        "adm",
        [](const Json::Value& value, CreativeInfo& info) {

            Datacratic::jsonDecode(value, info.adm);
            if (info.adm.empty()) {
                throw std::invalid_argument("adm is required");
            }

            return true;
    }).optional().snippet();

    creativeConfig.addField(
        "uniq_id",
        [](const Json::Value& value, CreativeInfo& info) {

            Datacratic::jsonDecode(value, info.uniq_id);
            if (info.uniq_id.empty()) {
                throw std::invalid_argument("uniq_id is required");
            }

            return true;
    }).optional().snippet();

    creativeConfig.addField(
        "group_class",
        [](const Json::Value& value, CreativeInfo& info) {

            Datacratic::jsonDecode(value, info.uniq_id);
            if (info.group_class.empty()) {
                throw std::invalid_argument("group_class is required");
            }

            return true;
    }).optional().snippet();
    // nurl might contain macros
    creativeConfig.addField(
        "nurl",
        [](const Json::Value & value, CreativeInfo & info) {
            Datacratic::jsonDecode(value, info.nurl);
            return true;
        }).optional().snippet();
    // iurl might contain macros
    creativeConfig.addField(
        "iurl",
        [](const Json::Value & value, CreativeInfo & info) {
            Datacratic::jsonDecode(value, info.iurl);
            return true;
        });

}
/*         */
 ExchangeConnector::ExchangeCompatibility
AppodealExchangeConnector::getCampaignCompatibility(
        const AgentConfig& config,
        bool includeReasons) const
{
    ExchangeCompatibility result;
    result.setCompatible();

    std::string exchange = exchangeName();
    const char* name = exchange.c_str();
    if (!config.providerConfig.isMember(exchange)) {
        result.setIncompatible(
                ML::format("providerConfig.%s is null", name), includeReasons);
        return result;
    }
 /*
    const auto& provConf = config.providerConfig[exchange];
    if (!provConf.isMember("seat")) {
        result.setIncompatible(
               ML::format("providerConfig.%s.seat is null", name), includeReasons);
        return result;
    }

   const auto& seat = provConf["seat"];
   if (!seat.isIntegral()) {
        result.setIncompatible(
                ML::format("providerConfig.%s.seat is not merdiumint or unsigned", name),
                includeReasons);
        return result;
    }

    uint64_t value = seat.asUInt();
    if (value > CampaignInfo::MaxSeatValue) {
        result.setIncompatible(
                ML::format("providerConfig.%s.seat > %lld", name, CampaignInfo::MaxSeatValue),
                includeReasons);
        return result;
    }
*/
    auto info = std::make_shared<CampaignInfo>();
 //   info->seat = value;

    result.info = info;
    return result;
}
ExchangeConnector::ExchangeCompatibility
AppodealExchangeConnector::getCreativeCompatibility(
        const Creative& creative,
        bool includeReasons) const
{
    return creativeConfig.handleCreativeCompatibility(creative, includeReasons);
}

std::shared_ptr<BidRequest>
AppodealExchangeConnector::
parseBidRequest(HttpAuctionHandler & connection,
                const HttpHeader & header,
                const std::string & payload)
{

    std::shared_ptr<BidRequest> none;
    //record_request(payload);
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
            return none;
        }
    }
    else {
        connection.sendErrorResponse("MISSING_CONTENT_TYPE_HEADER", "The request is missing the 'Content-Type' header");
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
        return none;
    }
    // Parse the bid request
    std::shared_ptr<BidRequest> result;
    try {
        ML::Parse_Context context("Bid Request", payload.c_str(), payload.size());
        result.reset(OpenRTBBidRequestParser::openRTBBidRequestParserFactory(openRtbVersion)->parseBidRequest(context,
                                                                                              exchangeName(),
                                                                                              exchangeName()));
	if(result->device) {
	    // Save Device.ifa in userIds.exchnageId
	    Id ifa(result->device->ifa);
	    
	    if(result->userIds.find(result->userIds.domainToString(ID_EXCHANGE)) == result->userIds.end()) {
		result->userIds.add(ifa, ID_EXCHANGE);
		//std::cerr << "DEBUG: exchangeId added" << std::endl;
	    }
	    else {
		result->userIds.setStatic(ifa, ID_EXCHANGE);
		//std::cerr << "DEBUG: exchangeId changed" << std::endl;
	    }
	}
	
	if(result->user) {
	    // add gender to segments
	    if(!result->user->gender.empty()) {
		result->segments.add("gender", result->user->gender);
	    }
	    
	    // add age to segments
	    if(result->user->yob.value() != -1) {
		result->segments.add("age", result->user->yob.value());
	    }
	    else {
		Datacratic::UnicodeString keyBirthDay("birthday");
		string birthDay("");
		
		for(auto data: result->user->data) {
		    for(auto segment: data.segment) {
			if(segment.name == keyBirthDay) {
			    birthDay = segment.value.extractAscii();
			    break;
			}
		    }
		    if(!birthDay.empty()) {
			std::string s_year(birthDay, birthDay.find_last_of("/") + 1);
			if (s_year.length() == 4) {
			    int birth_year =  std::stoi(s_year);
			    std::time_t tm = std::time(nullptr);
			    std::tm utc_tm = *std::gmtime(&tm);
			    int cur_year = utc_tm.tm_year + 1900;
			    int age = cur_year - birth_year;
			    result->segments.add("age", age);
			}
			break;
		    }
		}
	    }
	}
	
//	std::cerr <<  std::endl;
//	std::cerr << "DEBUG: Header verb: " << header.verb << std::endl;
//	std::cerr << "DEBUG: Header resource: " << header.resource << std::endl;
//	std::cerr << "DEBUG: Header version: " << header.version << std::endl;
//	std::cerr << "DEBUG: Header content type: " << header.contentType << std::endl;
//	std::cerr << "DEBUG: Header rest data: "  << std::endl;
//	for(auto it : header.headers) {
//	    std::cerr << "DEBUG: key: "  << it.first << " value: " << it.second << std::endl;
//	}

//	std::cerr << "DEBUG: exchangeId: " << result->userIds.exchangeId << std::endl;
//	std::cerr << "DEBUG: Request: " << payload << std::endl;
//	std::cerr << "DEBUG: Request: " << result->toJsonStr() << std::endl;
//	std::cerr << "DEBUG: Request seg: " << result->segments.toJson() << std::endl;
//	std::cerr << "DEBUG: Request device.model: " << result->device->model << std::endl;
//	std::cerr << "DEBUG: Request coordinates (lat/lon): " << result->device->geo->lat.val << "/" << result->device->geo->lon.val << std::endl;
//	std::cerr << "DEBUG: Request keywords: " << result->user->keywords << std::endl;
//	std::cerr << "DEBUG: Request Y.O.B: " << result->user->yob.value() << std::endl;
//	std::cerr << "DEBUG: Request gender: " << result->user->gender << std::endl;
	
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
        return 340.0;

    int tmax = atoi(payload.c_str() + pos + toFind.length());
    return (absoluteTimeMax < tmax) ? absoluteTimeMax : tmax;
}

HttpResponse
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
    std::string rv = stream.str();
 //find_and_replace(rv,"\\","");
cerr << "appodeal connector response 200:" << endl;
//record_response(rv);

    return HttpResponse(200, "application/json", rv);
//return HttpResponse(204, "none", "");
}


Json::Value
AppodealExchangeConnector::
getResponseExt(const HttpAuctionHandler & connection,
               const Auction & auction) const
{

  //auto& id = Id(auction.id, auction.request->imp[0].id);
 // auto& priceval = USD_CPM(resp.price.maxPrice);
 // Json::Value result;
 // result["timestamp"] = unix_timestamp();

  //return result;

  return {};
}

HttpResponse
AppodealExchangeConnector::
getDroppedAuctionResponse(const HttpAuctionHandler & connection,
                          const std::string & reason) const
{
cerr << "appodeal connector response 204: none" << endl;
  return HttpResponse(204, "none", "");
}


HttpResponse
AppodealExchangeConnector::
getErrorResponse(const HttpAuctionHandler & connection,
                 const std::string & message) const
{
    Json::Value response;
    response["error"] = message;
cerr << "appodeal connector response 400:" << message << endl;
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

/*void
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
} */
void
AppodealExchangeConnector::setSeatBid(
        const Auction& auction,
        int spotNum,
        OpenRTB::BidResponse& response) const {

    const Auction::Data *current = auction.getCurrentData();

    auto& resp = current->winningResponse(spotNum);

    const AgentConfig* config
        = std::static_pointer_cast<const AgentConfig>(resp.agentConfig).get();
    std::string name = exchangeName();

    auto campaignInfo = config->getProviderData<CampaignInfo>(name);
    int creativeIndex = resp.agentCreativeIndex;

    auto& creative = config->creatives[creativeIndex];
    auto creativeInfo = creative.getProviderData<CreativeInfo>(name);

    // Find the index in the seats array
    int seatIndex = indexOf(response.seatbid, &OpenRTB::SeatBid::seat, Id(campaignInfo->seat));

    OpenRTB::SeatBid* seatBid;

    // Creative the seat if it does not exist
    if (seatIndex == -1) {
        OpenRTB::SeatBid sbid;
        sbid.seat = Id(campaignInfo->seat);

        response.seatbid.push_back(std::move(sbid));

        seatBid = &response.seatbid.back();
    }
    else {
        seatBid = &response.seatbid[seatIndex];
    }

    ExcAssert(seatBid);
    seatBid->bid.emplace_back();
    auto& bid = seatBid->bid.back();

    AppodealCreativeConfiguration::Context context {
        creative,
        resp,
        *auction.request,
        spotNum
    };
//Json::Value in;
//in["timestamp"] = unix_timestamp();
    bid.cid = Id(resp.agent);
    bid.crid = Id(resp.creativeId);
    bid.impid = auction.request->imp[spotNum].id;
    bid.id = Id(auction.id, auction.request->imp[0].id);
    bid.price.val = USD_CPM(resp.price.maxPrice);
    /* Prices are in Cents CPM */
    bid.price.val *= 100;
//    bid.ext = in;

    //if (!creativeInfo->adomain.empty()) bid.adomain = creativeInfo->adomain;
    if (!creativeInfo->adm.empty()) bid.adm = creativeConfig.expand(creativeInfo->adm, context);
    if (!creativeInfo->nurl.empty()) bid.nurl = creativeConfig.expand(creativeInfo->nurl, context);
    if (!creativeInfo->iurl.empty()) bid.iurl = creativeConfig.expand(creativeInfo->iurl, context);
    if (!creativeInfo->uniq_id.empty()) bid.ext["uniq_id"] = creativeConfig.expand(creativeInfo->uniq_id, context);
    if (!creativeInfo->group_class.empty()) bid.ext["group_class"] = creativeConfig.expand(creativeInfo->group_class, context);

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

