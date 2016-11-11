/** augmentor_clicks.h                                 -*- C++ -*-
    Alexander Poznyak, 09 Oct 2016

    Interface of Augmentor

*/

#pragma once

#include "rtbkit/core/agent_configuration/agent_configuration_listener.h"
#include "rtbkit/plugins/augmentor/augmentor_base.h"
#include "soa/service/zmq_named_pub_sub.h"
#include "soa/service/service_base.h"

#include <string>
#include <memory>

namespace RTBKIT {

struct ClicksCapStorage;

struct AgentConfigEntry;

/******************************************************************************/
/* Clicks CAP AUGMENTOR                                                    */
/******************************************************************************/

struct ClicksCapAugmentor :
    public RTBKIT::SyncAugmentor
{

    ClicksCapAugmentor(
            std::shared_ptr<Datacratic::ServiceProxies> services,
            const std::string& serviceName,
            const std::string& augmentorName = "frequency-clicks-cap");

    void init();

private:

    virtual RTBKIT::AugmentationList
    onRequest(const RTBKIT::AugmentationRequest& request);
    
    size_t getCapLimit(
            const std::string& augmentor,
            const std::string& agent,
            const RTBKIT::AgentConfigEntry& config,
            const char* key) const;
    
    size_t getCommonLimit(
            const std::string& augmentor,
            const std::string& agent,
            const RTBKIT::AgentConfigEntry& config) const;

    size_t getDayLimit(
            const std::string& augmentor,
            const std::string& agent,
            const RTBKIT::AgentConfigEntry& config) const;
            
    size_t getHourLimit(
            const std::string& augmentor,
            const std::string& agent,
            const RTBKIT::AgentConfigEntry& config) const;

    size_t getInterval(
            const std::string& augmentor,
            const std::string& agent,
            const RTBKIT::AgentConfigEntry& config) const;

    std::shared_ptr<ClicksCapStorage> storage;

    RTBKIT::AgentConfigurationListener agentConfig;
    Datacratic::ZmqNamedMultipleSubscriber palEvents;
};


} // namespace RTBKIT
