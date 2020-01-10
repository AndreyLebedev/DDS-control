// Copyright 2019 GSI, Inc. All rights reserved.
//
//

#ifndef __ODC__GrpcControlServer__
#define __ODC__GrpcControlServer__

// STD
#include <string>

// DDS
#include "GrpcControlService.h"

namespace odc
{
    namespace grpc
    {
        class GrpcControlServer final
        {
          public:
            void Run(const std::string& _host, const odc::core::ControlService::SConfigParams& _params);
        };
    } // namespace grpc
} // namespace odc

#endif /* defined(__ODC__GrpcControlServer__) */
