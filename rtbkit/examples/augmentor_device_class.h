/** augmentor_device_class.h                                 -*- C++ -*-
    Copyright (c) 2013 Datacratic.  All rights reserved.

    Interface of our Augmentor example for an extremely simple frequency cap
    service.

    Note that this header exists mainly so that it can be integrated into the
    rtbkit_integration_test. Most of the documentation for it is in the cc file.

*/

#pragma once

#include "rtbkit/core/agent_configuration/agent_configuration_listener.h"
#include "rtbkit/plugins/augmentor/augmentor_base.h"
#include "soa/service/zmq_named_pub_sub.h"
#include "soa/service/service_base.h"

#include <string>
#include <memory>

namespace RTBKIT {

    
typedef std::pair<std::string, std::unordered_set<std::string>> deviceClassInfo;
typedef std::vector<deviceClassInfo> deviceClassInfoList;
    
struct DeviceClassStorage;

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
struct DeviceClassAugmentor :
    public RTBKIT::SyncAugmentor
{

    DeviceClassAugmentor(
            std::shared_ptr<Datacratic::ServiceProxies> services,
            const std::string& serviceName,
            const std::string& augmentorName = "device-class");

    void init(const std::string& file);

private:

    virtual RTBKIT::AugmentationList
    onRequest(const RTBKIT::AugmentationRequest& request);

    void loadDbFile(const std::string& file);
    
    std::string findClass(const std::string& device);
    std::shared_ptr<DeviceClassStorage> storage;

    RTBKIT::AgentConfigurationListener agentConfig;
    Datacratic::ZmqNamedMultipleSubscriber palEvents;
};


} // namespace RTBKIT
