/** augmentor_start_stop.h                                 -*- C++ -*-

    Interface of start/stop Augmentor

*/

#pragma once

#include "rtbkit/core/agent_configuration/agent_configuration_listener.h"
#include "rtbkit/plugins/augmentor/augmentor_base.h"
#include "soa/service/zmq_named_pub_sub.h"
#include "soa/service/service_base.h"

#include <string>
#include <memory>

namespace RTBKIT {

struct AgentConfigEntry;

/******************************************************************************/
/* FREQUENCY CAP AUGMENTOR                                                    */
/******************************************************************************/

/** A start/stop augmentor which limits a period an ad can
    be shown. It's multithreaded and connects to the
    following services:

    - The augmentation loop for its bid request stream.
    - The post auction loop for its win notification
    - The agent configuration listener to retrieve agent configuration for the
      augmentor.

 */
struct StartStopAugmentor :
    public RTBKIT::SyncAugmentor
{

    StartStopAugmentor(
            std::shared_ptr<Datacratic::ServiceProxies> services,
            const std::string& serviceName,
            const std::string& augmentorName = "start-stop-time");

    void init();

private:

    virtual RTBKIT::AugmentationList
    onRequest(const RTBKIT::AugmentationRequest& request);

    int getTime( const std::string& augmentor,
		 const std::string& agent,
		 const RTBKIT::AgentConfigEntry& config,
		 const std::string key,
		 std::tm* ptime) const;
		    
    int getStartTime( const std::string& augmentor,
		      const std::string& agent,
		      const RTBKIT::AgentConfigEntry& config,
		      std::tm* pstart_time) const;
			 
    int getStopTime( const std::string& augmentor,
		     const std::string& agent,
		     const RTBKIT::AgentConfigEntry& config,
		     std::tm* pstop_time) const;   
			 
    RTBKIT::AgentConfigurationListener agentConfig;
    Datacratic::ZmqNamedMultipleSubscriber palEvents;
};


} // namespace RTBKIT
