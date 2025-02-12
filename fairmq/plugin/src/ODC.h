/********************************************************************************
 * Copyright (C) 2017-2021 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#ifndef __ODC__fairmq_odc
#define __ODC__fairmq_odc

#include "CustomCommands.h"

#include <fairmq/Plugin.h>
#include <fairmq/StateQueue.h>
#include <fairmq/Version.h>

#include <dds/dds.h>

#include <boost/asio/executor.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>

#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility> // pair
#include <vector>

namespace odc::plugins
{

    struct DDSConfig
    {
        // container of sub channel addresses
        unsigned int fNumSubChannels;
        // dds values for the channel
        std::map<uint64_t, std::string> fDDSValues;
    };

    struct DDSSubscription
    {
        DDSSubscription()
            : fDDSCustomCmd(fService)
            , fDDSKeyValue(fService)
        {
            LOG(debug) << "$DDS_TASK_PATH: " << dds::env_prop<dds::task_path>();
            LOG(debug) << "$DDS_GROUP_NAME: " << dds::env_prop<dds::group_name>();
            LOG(debug) << "$DDS_COLLECTION_NAME: " << dds::env_prop<dds::collection_name>();
            LOG(debug) << "$DDS_TASK_NAME: " << dds::env_prop<dds::task_name>();
            LOG(debug) << "$DDS_TASK_INDEX: " << dds::env_prop<dds::task_index>();
            LOG(debug) << "$DDS_COLLECTION_INDEX: " << dds::env_prop<dds::collection_index>();
            LOG(debug) << "$DDS_TASK_ID: " << dds::env_prop<dds::task_id>();
            LOG(debug) << "$DDS_LOCATION: " << dds::env_prop<dds::dds_location>();
            std::string dds_session_id(dds::env_prop<dds::dds_session_id>());
            LOG(debug) << "$DDS_SESSION_ID: " << dds_session_id;

            // subscribe for DDS service errors.
            fService.subscribeOnError(
                [](const dds::intercom_api::EErrorCode errorCode, const std::string& errorMsg)
                { LOG(debug) << "DDS Error received: error code: " << errorCode << ", error message: " << errorMsg; });

            // fDDSCustomCmd.subscribe([](const std::string& cmd, const std::string& cond, uint64_t senderId) {
            // LOG(debug) << "cmd: " << cmd << ", cond: " << cond << ", senderId: " << senderId;
            // });
            assert(!dds_session_id.empty());
        }

        auto Start() -> void
        {
            fService.start(dds::env_prop<dds::dds_session_id>());
        }

        ~DDSSubscription()
        {
            fDDSKeyValue.unsubscribe();
            fDDSCustomCmd.unsubscribe();
        }

        template <typename... Args>
        auto SubscribeCustomCmd(Args&&... args) -> void
        {
            fDDSCustomCmd.subscribe(std::forward<Args>(args)...);
        }

        template <typename... Args>
        auto SubscribeKeyValue(Args&&... args) -> void
        {
            fDDSKeyValue.subscribe(std::forward<Args>(args)...);
        }

        template <typename... Args>
        auto Send(Args&&... args) -> void
        {
            fDDSCustomCmd.send(std::forward<Args>(args)...);
        }

        template <typename... Args>
        auto PutValue(Args&&... args) -> void
        {
            fDDSKeyValue.putValue(std::forward<Args>(args)...);
        }

      private:
        dds::intercom_api::CIntercomService fService;
        dds::intercom_api::CCustomCmd fDDSCustomCmd;
        dds::intercom_api::CKeyValue fDDSKeyValue;
    };

    struct IofN
    {
        IofN(int i, int n)
            : fI(i)
            , fN(n)
        {
        }

        unsigned int fI;
        unsigned int fN;
        std::vector<std::string> fEntries;
    };

    class ODC : public fair::mq::Plugin
    {
      public:
        ODC(const std::string& name,
            const fair::mq::Plugin::Version version,
            const std::string& maintainer,
            const std::string& homepage,
            fair::mq::PluginServices* pluginServices);

        ~ODC();

      private:
        auto WaitForExitingAck() -> void;
        auto StartWorkerThread() -> void;

        auto FillChannelContainers() -> void;
        auto EmptyChannelContainers() -> void;

        auto SubscribeForConnectingChannels() -> void;
        auto PublishBoundChannels() -> void;
        auto SubscribeForCustomCommands() -> void;
        auto HandleCmd(const std::string& id, cc::Cmd& cmd, const std::string& cond, uint64_t senderId) -> void;

        DDSSubscription fDDS;
        size_t fDDSTaskId;

        std::unordered_map<std::string, std::vector<std::string>> fBindingChans;
        std::unordered_map<std::string, DDSConfig> fConnectingChans;

        std::unordered_map<std::string, int> fI;
        std::unordered_map<std::string, IofN> fIofN;

        std::thread fControllerThread;
        DeviceState fCurrentState, fLastState;

        std::atomic<bool> fDeviceTerminationRequested;

        std::unordered_map<uint64_t, std::pair<std::chrono::steady_clock::time_point, int64_t>> fStateChangeSubscribers;
        uint64_t fLastExternalController;
        bool fExitingAckedByLastExternalController;
        std::condition_variable fExitingAcked;
        std::mutex fStateChangeSubscriberMutex;

        bool fUpdatesAllowed;
        std::mutex fUpdateMutex;
        std::condition_variable fUpdateCondition;

        std::thread fWorkerThread;
        boost::asio::io_context fWorkerQueue;
        boost::asio::executor_work_guard<boost::asio::executor> fWorkGuard;
    };

    inline fair::mq::Plugin::ProgOptions ODCPluginProgramOptions()
    {
        boost::program_options::options_description options{ "ODC Plugin" };
        options.add_options()(
            "dds-i",
            boost::program_options::value<std::vector<std::string>>()->multitoken()->composing(),
            "Task index for chosing connection target (single channel n to m). When all values come via same update.")(
            "dds-i-n",
            boost::program_options::value<std::vector<std::string>>()->multitoken()->composing(),
            "Task index for chosing connection target (one out of n values to take). When values come as independent "
            "updates.")("wait-for-exiting-ack-timeout",
                        boost::program_options::value<unsigned int>()->default_value(1000),
                        "Wait timeout for EXITING state-change acknowledgement by external controller in "
                        "milliseconds.");

        return options;
    }

    REGISTER_FAIRMQ_PLUGIN(ODC,                                    // Class name
                           odc,                                    // Plugin name (string, lower case chars only)
                           (fair::mq::Plugin::Version{ 1, 0, 0 }), // Version
                           "FairRootGroup <fairroot@gsi.de>",      // Maintainer
                           "https://github.com/FairRootGroup/ODC", // Homepage
                           ODCPluginProgramOptions                 // custom program options for the plugin
    )

} // namespace odc::plugins

#endif /* __ODC__plugins__fairmq_dds */
