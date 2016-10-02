/* standard_adserver_connector.cc
   Wolfgang Sourdeau, March 2013
   Copyright (c) 2013 Datacratic.  All rights reserved. */


#include "rtbkit/common/account_key.h"
#include "rtbkit/common/currency.h"
#include "rtbkit/common/json_holder.h"

#include "soa/service/logs.h"

#include "standard_adserver_connector.h"


using namespace std;

std::mutex win_lock;
std::mutex event_lock;

long get_win_id(redisContext *rc) {
    win_lock.lock();
    redisReply *reply;
    reply = (redisReply *)redisCommand(rc,"INCR win_counter");
    long i = reply->integer;
    freeReplyObject(reply);
    win_lock.unlock();
    return i;

}

long get_event_id(redisContext *rc) {
    event_lock.lock();
    redisReply *reply;
    reply = (redisReply *)redisCommand(rc,"INCR event_counter");
    long i = reply->integer;
    freeReplyObject(reply);
    event_lock.unlock();
    return i;

}



void writeFile (std::string s) {

  std::ofstream ofs;
  ofs.open ("win.txt", std::ofstream::out | std::ofstream::app);

  ofs << s << endl;

  ofs.close();

}
using namespace boost::program_options;

using namespace RTBKIT;

Logging::Category adserverTrace("Standard Ad-Server connector");

/* STANDARDADSERVERARGUMENTS */
boost::program_options::options_description
StandardAdServerArguments::
makeProgramOptions()
{
    boost::program_options::options_description stdOptions
        = ServiceProxyArguments::makeProgramOptions();

    verbose = false;

    boost::program_options::options_description
        options("Standard-Ad-Server-Connector");
    options.add_options()
        ("win-port,w", value(&winPort), "listening port for wins")
        ("events-port,e", value(&eventsPort), "listening port for events")
        ("verbose,v", value(&verbose), "verbose mode")
        ("analytics,a", bool_switch(&analyticsOn),
         "Send data to analytics endpoint.")
        ("analytics-connections", value<int>(&analyticsConnections),
         "Number of connections for the analytics publisher.");
    stdOptions.add(options);

    return stdOptions;
}

void
StandardAdServerArguments::
validate()
{
    ExcCheck(winPort > 0, "winPort is not set");
    ExcCheck(eventsPort > 0, "eventsPort is not set");
}

/* STANDARDADSERVERCONNECTOR */

StandardAdServerConnector::
StandardAdServerConnector(std::shared_ptr<ServiceProxies> & proxy,
                           const string & serviceName)
    : HttpAdServerConnector(serviceName, proxy),
      publisher_(proxy->zmqContext)
{
    initEventType(Json::Value());
    rc2 = redisConnect(connection.c_str(), rport);

    if (rc2 == NULL || rc2->err) {
        if (rc2) {
            throw ML::Exception("Error",rc2->errstr);
            // handle error
        } else {
            throw ML::Exception("Can't allocate redis context\n");
        }
    }
    rc3 = redisConnect(connection.c_str(), rport);

    if (rc3 == NULL || rc3->err) {
        if (rc3) {
            throw ML::Exception("Error",rc3->errstr);
            // handle error
        } else {
            throw ML::Exception("Can't allocate redis context\n");
        }
    }
}

void StandardAdServerConnector::record_win(std::string s) const
{
    redisContext* rc7 = redisConnect(connection.c_str(), rport);
    redisReply *reply;
    long i = get_win_id(rc7);
    string z = "win:"+std::to_string(i);
    reply = (redisReply *)redisCommand(rc7,"SET %s %s ", z.c_str(), s.c_str());
    if (reply->type == REDIS_REPLY_ERROR) {
    cerr << reply->str << endl;
    }
    freeReplyObject(reply);
    redisFree(rc7);


}

void StandardAdServerConnector::record_event(std::string s) const
{

    redisContext* rc8 = redisConnect(connection.c_str(), rport);
    redisReply *reply;
    long i = get_event_id(rc8);
    string z = "event:"+std::to_string(i);
    reply = (redisReply *)redisCommand(rc8,"SET %s %s ", z.c_str(), s.c_str());
    if (reply->type == REDIS_REPLY_ERROR) {
    cerr << reply->str << endl;
    }
    freeReplyObject(reply);
    redisFree(rc8);



}
StandardAdServerConnector::
StandardAdServerConnector(std::string const & serviceName, std::shared_ptr<ServiceProxies> const & proxies,
                          Json::Value const & json) :
    HttpAdServerConnector(serviceName, proxies),
    publisher_(getServices()->zmqContext) {
    int winPort = json.get("winPort", 17340).asInt();
    int eventsPort = json.get("eventsPort", 17341).asInt();
    verbose = json.get("verbose", false).asBool();
    initEventType(json);
    init(winPort, eventsPort, verbose);
}

void
StandardAdServerConnector::
initEventType(const Json::Value &json) {
    
    // Default value
    eventType["CLICK"] =  "CLICK";
    eventType["CONVERSION"] =  "CONVERSION";
    eventType["IMPRESSION"] =  "IMPRESSION";
    // User value
    if(json.isMember("eventType")) {
        auto item = json["eventType"];
        auto items = item.getMemberNames();
        
        for(auto i=items.begin(); i!=items.end(); ++i) {
            eventType[*i] = item[*i].asString();
        }
    }
}

void
StandardAdServerConnector::
init(StandardAdServerArguments & ssConfig)
{

  ssConfig.validate();
    init(ssConfig.winPort, ssConfig.eventsPort, ssConfig.verbose,
         ssConfig.analyticsOn, ssConfig.analyticsConnections);
}

void
StandardAdServerConnector::
init(int winsPort, int eventsPort, bool verbose, bool analyticsOn, int analyticsConnections)
{
    if(!verbose) {
        adserverTrace.deactivate();
    }

    shared_ptr<ServiceProxies> services = getServices();

    auto win = &StandardAdServerConnector::handleWinRq;
    registerEndpoint(winsPort, bind(win, this, _1, _2, _3));

    auto delivery = &StandardAdServerConnector::handleDeliveryRq;
    registerEndpoint(eventsPort, bind(delivery, this, _1, _2, _3));

    HttpAdServerConnector::init(services->config);
    publisher_.init(services->config, serviceName_ + "/logger");

    if (analyticsOn) {
        const auto & analyticsUri = services->params["analytics-uri"].asString();
        if (!analyticsUri.empty()) {
            cout << "analyticsURI: " << analyticsUri << endl;
            analytics_.init(analyticsUri, analyticsConnections);
        }
        else cout << "analytics-uri is not in the config" << endl;
    }
}


void
StandardAdServerConnector::
init(int winsPort, int eventsPort)
{
    init(winsPort, eventsPort, false);
}

void
StandardAdServerConnector::
start()
{
    bindTcp();

    publisher_.bindTcp(getServices()->ports->getRange("adServer.logger"));
    publisher_.start();
    analytics_.start();
    HttpAdServerConnector::start();
}

void
StandardAdServerConnector::
shutdown()
{
    redisFree(rc2);
    redisFree(rc3);
    publisher_.shutdown();
    analytics_.shutdown();
    HttpAdServerConnector::shutdown();
}

void
errorResponseHelper(HttpAdServerResponse & resp,
                    const std::string & errorName,
                    const std::string & details) {
    resp.valid = false;
    resp.error = errorName;
    resp.details = details;
}

HttpAdServerResponse
StandardAdServerConnector::
handleWinRq(const HttpHeader & header,
            const Json::Value & json, const std::string & jsonStr)
{
    HttpAdServerResponse response;

    Date timestamp;
    string bidRequestIdStr;
    string impIdStr;
    double winPriceDbl;

    Id bidRequestId;
    Id impId;
    USD_CPM winPrice;

    UserIds userIds;
    string userIdStr;
    string passback;

    /*
     *  Timestamp is an required field.
     *  If null, we return an error response.
     */

    if (json.isMember("timestamp")) {
        timestamp = Date::fromSecondsSinceEpoch(json["timestamp"].asDouble());

        // Check if timestamp is finite when treated as seconds
        if(!timestamp.isADate()) {
            errorResponseHelper(response,
                                "TIMESTAMP_NOT_SECONDS",
                                "The timestamp field is not in seconds.");
            publishError(response);
            return response;
        }
    } else {
        errorResponseHelper(response,
                            "MISSING_TIMESTAMP",
                            "A win notice requires the timestamp field.");
        publishError(response);
        return response;
    }

    /*
     *  bidRequestId is an required field.
     *  If null, we return an error response.
     */
    if (json.isMember("bidRequestId")) {

        bidRequestIdStr = (json["bidRequestId"].asString());
        if(bidRequestIdStr.find(":") == string::npos)
        {
            errorResponseHelper(response,
                                "WRONG_BIDREQUESTID",
                                "A win notice requires correct bidRequestId field.");
            publishError(response);
            return response;
        }
        bidRequestIdStr =  bidRequestIdStr.substr(0,bidRequestIdStr.find_last_of(":"));

        bidRequestId = Id(bidRequestIdStr);

    } else {
        errorResponseHelper(response,
                            "MISSING_BIDREQUESTID",
                            "A win notice requires the bidRequestId field.");
        publishError(response);
        return response;
    }

    /*
     *  impid is an required field.
     *  If null, we return an error response.
     */
    if (json.isMember("impId")) {
        impIdStr = json["impId"].asString();


        impId = Id(impIdStr);

    } else {
        errorResponseHelper(response,
                            "MISSING_IMPID",
                            "A win notice requires the impId field.");

        publishError(response);
        return response;
    }

    /*
     *  price is an required field.
     *  If null, we return an error response.
     */
    std::string winPriceDbls;
    if (json.isMember("winPrice")) {
        winPriceDbl = (json["winPrice"].asDouble())*0.001;
     //   winPriceDbls = json["winPrice"].asString();
      //  winPriceDbl = std::stod(winPriceDbls);
     //   writeFile("winprice: " + std::to_string(winPriceDbl));
        winPrice = USD_CPM(winPriceDbl);

    } else {
        errorResponseHelper(response,
                            "MISSING_WINPRICE",
                            "A win notice requires the price field.");
        publishError(response);
        return response;
    }
    
    /*
     *  UserIds is an optional field.
     *  If null, we just put an empty array.
     */
    if (json.isMember("userIds")) {
        auto item =  json["userIds"];
        if(!item.empty())
            userIds.add(Id(item[0].asString()), ID_PROVIDER);
    }
    else {
        // UserIds is optional
    }

    /*
     *  Passback is an optional field.
     *  If null, we just put an empty string.
     */
    if (json.isMember("passback")) {
        passback =  json["passback"].asString();
    }
    else {
        // Passback is optional
    }
    //record_win(json.toString());
    LOG(adserverTrace) << "{\"timestamp\":\"" << timestamp.print(3) << "\"," <<
        "\"bidRequestId\":\"" << bidRequestId << "\"," <<
        "\"impId\":\"" << impId << "\"," <<
        "\"winPrice\":\"" << winPrice.toString() << "\" }";
  //  string s =  "{\"timestamp\":\"" + timestamp.print(3) + "\","+"\"bidRequestId\":\"" + bidRequestIdStr + "\"," +"\"winPrice\":\"" + json["winPrice"].asString() +"\" }";
  //  writeFile("WIN: "+ s );

    if(response.valid) {

        publishWin(bidRequestId, impId, winPrice, timestamp, Json::Value(), userIds,
                   AccountKey(passback), Date());

        publisher_.publish("WIN", timestamp.print(3), bidRequestIdStr,
                           impIdStr, winPrice.toString());

        analytics_.publish("WIN", timestamp.print(3), bidRequestIdStr,
                           impIdStr, winPrice.toString());
    }

    return response;
}

HttpAdServerResponse
StandardAdServerConnector::
handleDeliveryRq(const HttpHeader & header,
                 const Json::Value & json, const std::string & jsonStr)
{    
    HttpAdServerResponse response;
    string bidRequestIdStr, impIdStr, userIdStr, event;
    Id bidRequestId, impId, userId;
    UserIds userIds;
    Date timestamp;
    
    /*
     *  Timestamp is an required field.
     *  If null, we return an error response.
     */
    if (json.isMember("timestamp")) {
        timestamp = Date::fromSecondsSinceEpoch(json["timestamp"].asDouble());
        
        // Check if timestamp is finite when treated as seconds
        if(!timestamp.isADate()) {
            errorResponseHelper(response,
                                "TIMESTAMP_NOT_SECONDS",
                                "The timestamp field is not in seconds.");
            publishError(response);
            return response;
        }
    } else {
        errorResponseHelper(response,
                            "MISSING_TIMESTAMP",
                            "A win notice requires the timestamp field.");
        publishError(response);
        return response;
    }

    /*
     *  type is an required field.
     *  If null, we return an error response.
     */
    if (json.isMember("type")) {

        event = (json["type"].asString());
        
        if(eventType.find(event) == eventType.end()) {
            errorResponseHelper(response,
                                "UNSUPPORTED_TYPE",
                                "A campaign event requires the type field.");
            publishError(response);
            return response;
        }

    } else {

        errorResponseHelper(response,
                            "MISSING_TYPE",
                            "A campaign event requires the type field.");
        publishError(response);
        return response;
    }
    //event = "CONVERSION";

    /*
     *  impid is an required field.
     *  If null, we return an error response.
     */
    if (json.isMember("impId")) {

    } else {
        errorResponseHelper(response,
                            "MISSING_IMPID",
                            "A campaign event requires the impId field.");
        publishError(response);
        return response;
    }

    /*
     *  bidRequestId is an required field.
     *  If null, we return an error response.
     */
    if (json.isMember("bidRequestId")) {

    } else {
        errorResponseHelper(response,
                            "MISSING_BIDREQUESTID",
                            "A campaign event requires the bidRequestId field.");
        publishError(response);
        return response;
    }
    /*
     *  UserIds is an optional field.
     *  If null, we just put an empty array.
     */
    if (json.isMember("userIds")) {
        auto item =  json["userIds"];

        if(!item.empty())
            userIds.add(Id(item[0].asString()), ID_PROVIDER);
    }
    else {
        // UserIds is optional
    }

    bidRequestIdStr = json["bidRequestId"].asString();
    if(bidRequestIdStr.find(":") == string::npos)
    {
        errorResponseHelper(response,
                            "WRONG_BIDREQUESTID",
                            "Event requires correct bidRequestId field.");
        publishError(response);
        return response;
    }
    bidRequestIdStr =  bidRequestIdStr.substr(0,bidRequestIdStr.find_last_of(":"));
    impIdStr = json["impid"].asString();
    bidRequestId = Id(bidRequestIdStr);
    impId = Id(impIdStr);
    //record_event(json.toString());
    LOG(adserverTrace) << "{\"timestamp\":\"" << timestamp.print(3) << "\"," <<
        "\"bidRequestId\":\"" << bidRequestIdStr << "\"," <<
        "\"impId\":\"" << impIdStr << "\"," <<
        "\"event\":\"" << event << 
        "\"userIds\":" << userIds.toString() << "\"}";

    //string s = "{\"timestamp\":\"" + timestamp.print(3) + "\"," + "\"bidRequestId\":\"" + bidRequestIdStr +"\","+"\"impId\":\"" + impIdStr + "\"}";
   // writeFile("IMPRESSION: "+s);

    if(response.valid) {

        publishCampaignEvent(eventType[event], bidRequestId, impId, timestamp,
                                 Json::Value(), userIds);
        publisher_.publish(eventType[event], timestamp.print(3), bidRequestIdStr,
                                impIdStr, userIds.toString());
        analytics_.publish(eventType[event], timestamp.print(3), bidRequestIdStr,
                                impIdStr, userIds.toString());
    }
    return response;
}

void
StandardAdServerConnector::
publishError(HttpAdServerResponse & resp) {
    analytics_.publish("ADSERVER_ERROR", resp.error, resp.details);
}

namespace {

struct AtInit {
    AtInit()
    {
        PluginInterface<AdServerConnector>::registerPlugin("standard",
					   [](std::string const & serviceName,
					      std::shared_ptr<ServiceProxies> const & proxies,
					      Json::Value const & json) {
            return new StandardAdServerConnector(serviceName, proxies, json);
        });
    }
} atInit;

}

