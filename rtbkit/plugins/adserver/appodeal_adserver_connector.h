#ifndef APPODEAL_ADSERVER_CONNECTOR_H
#define APPODEAL_ADSERVER_CONNECTOR_H
#include "soa/service/service_utils.h"
#include "soa/service/service_base.h"
#include "soa/service/json_endpoint.h"
#include "soa/service/zmq_named_pub_sub.h"
#include "rtbkit/plugins/adserver/http_adserver_connector.h"
#include "rtbkit/common/auction_events.h"

namespace RTBKIT {

/******************************************************************************/
/* APPODEAL AD SERVER CONNECTOR                                                   */
/******************************************************************************/

/** Basic ad server connector that sits between the stream of wins (received from
    the exchange) and rest of the stack. Note that this assumes that the incoming
    message format is from the mock exchange sample.

 */

struct AppodealAdServerConnector : public HttpAdServerConnector
{
    AppodealAdServerConnector(const std::string& serviceName,
                          std::shared_ptr<Datacratic::ServiceProxies> proxies)
        : HttpAdServerConnector(serviceName, proxies),
          publisher(getServices()->zmqContext) {
    }

    AppodealAdServerConnector(Datacratic::ServiceProxyArguments & args,
                          const std::string& serviceName)
        : HttpAdServerConnector(serviceName, args.makeServiceProxies()),
          publisher(getServices()->zmqContext) {
    }

    AppodealAdServerConnector(std::string const & serviceName, std::shared_ptr<ServiceProxies> const & proxies,
                          Json::Value const & json);

    void init(int winPort, int eventPort);
    void start();
    void shutdown();
    HttpAdServerResponse handleEvent(PostAuctionEvent const & event);

    /// Generic publishing endpoint to forward wins to anyone registered. Currently, there's only the
    /// router that connects to this.
    Datacratic::ZmqNamedPublisher publisher;
};

} // namepsace RTBKIT


#endif // APPODEAL_ADSERVER_CONNECTOR_H
