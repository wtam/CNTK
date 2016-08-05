//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once

#include "Reader.h"
#include "MemoryProvider.h"
#include "Config.h"
#include "Transformer.h"
#include "Packer.h"

template <class T>
class IDsLoader;

namespace Microsoft { namespace MSR { namespace CNTK {

class DataSource;

// Reader that reads image dataset format.
class ImageDatasetReader : public Reader
{
public:
    ImageDatasetReader(MemoryProviderPtr provider, const ConfigParameters& parameters);

    // Description of streams that this reader provides.
    std::vector<StreamDescriptionPtr> GetStreamDescriptions() override;

    // Starts a new epoch with the provided configuration.
    void StartEpoch(const EpochConfiguration& config) override;

    // Reads a single minibatch.
    Minibatch ReadMinibatch() override;

private:

    // Object that reads data from ids and pushes them to packer.
    std::shared_ptr<DataSource> m_dataSource;

    // Packer.
    PackerPtr m_packer;

    // Memory provider.
    MemoryProviderPtr m_provider;
};

}}}
