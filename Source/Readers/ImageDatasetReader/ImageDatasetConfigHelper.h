//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once

#include <string>
#include <vector>
#include <memory>
#include "Config.h"
#include "Reader.h"

namespace Microsoft { namespace MSR { namespace CNTK {

struct IgnoreStreamDescriptor
{
    // Stores ignore label value.
    int ignoreLabel;
    // Stores the name of ignore mask stream.
    std::string ignoreStreamName;
};

// Encompasses stream info from the config file.
struct StreamDescriptor
{
    // Name of the stream in network description.
    std::string name;
    // Name of the stream in dataset.
    std::string datasetName;
    // Type of the stream (dense or sparse).
    StorageType datasetStorageType;
    // In case of sparse stream stores dimension of dense equivalent. Expected sparse data have shape height x width x 1.
    // Last dimension (of size 1) is index of non-zero value in dense equivalent with shape height x width x dimension.
    int dimension;
    // Stores info about ignore stream.
    std::shared_ptr<IgnoreStreamDescriptor> ignoreStream;
};

// Helper methods for parsing config.
class ImageDatasetConfigHelper
{
public:
    // Returns path to the configuration file for dataset loader.
    static std::string GetLoadConfigPath(const ConfigParameters& config);

    // Returns set of stream descriptors from the config.
    static std::vector<StreamDescriptor> GetStreamDescriptors(const ConfigParameters& config);

    // Returns if dataset directory is specified.
    static bool HasDatasetDir(const ConfigParameters& config);

    // Returns specified dataset directory.
    static std::string GetDatasetDir(const ConfigParameters& config);

    // Returns if workers rank is specified.
    static bool HasWorkerRank(const ConfigParameters& config);

    // Returns specified worker rank.
    static size_t GetWorkerRank(const ConfigParameters& config);

    // Returns if number of workers is specified.
    static bool HasWorkersCount(const ConfigParameters& config);

    // Returns specified number of workers.
    static size_t GetWorkersCount(const ConfigParameters& config);
};
} } }
