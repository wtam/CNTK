//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once

#include "ReaderBase.h"

namespace Microsoft { namespace MSR { namespace CNTK {

class DataSource;

// Reader that reads image dataset format.
class ImageDatasetReader : public ReaderBase
{
public:
    ImageDatasetReader(MemoryProviderPtr provider, const ConfigParameters& parameters);

    // Description of streams that this reader provides.
    std::vector<StreamDescriptionPtr> GetStreamDescriptions() override;
};

}}}
