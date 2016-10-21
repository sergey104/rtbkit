/** augmentor_fin.h                                 -*- C++ -*-
    Alexander POznyak, 20.10.2016

    Interface of financial augmentor
*/

#pragma once

#include "rtbkit/core/agent_configuration/agent_configuration_listener.h"
#include "rtbkit/plugins/augmentor/augmentor_base.h"
#include "soa/service/zmq_named_pub_sub.h"
#include "soa/service/service_base.h"

#include <string>
#include <memory>

namespace RTBKIT {

struct FinancialCapStorage;

struct AgentConfigEntry;

/******************************************************************************/
/* FREQUENCY CAP AUGMENTOR                                                    */
/******************************************************************************/

/** A Simple frequency cap augmentor which limits the number of times an ad can
    be shown to a specific user. It's multithreaded and connects to the
    following services:

    - The augmentation loop for its bid request stream.
    - The post auction loop for its win notification
    - The agent configuration listener to retrieve agent configuration for the
      augmentor.
    - FrequencyCapStorage for its simplistic data repository.

 */
struct FinancialAugmentor :
    public RTBKIT::SyncAugmentor
{

    FinancialAugmentor(
            std::shared_ptr<Datacratic::ServiceProxies> services,
            const std::string& serviceName,
            const std::string& augmentorName = "spending-money-control");

    void init();

private:

    virtual RTBKIT::AugmentationList
    onRequest(const RTBKIT::AugmentationRequest& request);

    RTBKIT::Amount 
    getMaxPerDay(
            const std::string& augmentor,
            const std::string& agent,
            const RTBKIT::AgentConfigEntry& config) const;

    RTBKIT::Amount 
    getMaxPerHour(
            const std::string& augmentor,
            const std::string& agent,
            const RTBKIT::AgentConfigEntry& config) const;

    std::shared_ptr<FinancialCapStorage> storage;

    RTBKIT::AgentConfigurationListener agentConfig;
    Datacratic::ZmqNamedMultipleSubscriber palEvents;
};


} // namespace RTBKIT
