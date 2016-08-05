//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// Exports.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#define DATAREADER_EXPORTS
#include "CudaMemoryProvider.h"
#include "DataReader.h"
#include "HeapMemoryProvider.h"
#include "ImageDatasetReader.h"
#include "ReaderShim.h"

namespace Microsoft { namespace MSR { namespace CNTK {

auto factory = [](const ConfigParameters& parameters) -> ReaderPtr
{
    return std::make_shared<ImageDatasetReader>(std::make_shared<HeapMemoryProvider>(), parameters);
};

extern "C" DATAREADER_API void GetReaderF(IDataReader** preader)
{
    *preader = new ReaderShim<float>(factory);
}

extern "C" DATAREADER_API void GetReaderD(IDataReader** preader)
{
    *preader = new ReaderShim<double>(factory);
}

}}}
