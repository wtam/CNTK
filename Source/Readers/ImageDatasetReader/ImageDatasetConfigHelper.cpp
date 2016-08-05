//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include "stdafx.h"
#include "ImageDatasetConfigHelper.h"
#include "StringUtil.h"

using namespace std;

namespace Microsoft { namespace MSR { namespace CNTK {

// Example ids reader declaration:
//    reader = [
//        readerType = "ImageDatasetReader"
//
//        # Path to image dataset load config file.
//        loadConfigFile = "$ConfigDir$/train.config.prototxt"
//        # Streams to import from dataset.
//        streams = [
//            features = [
//                ds_name = input
//                type = dense
//            ]
//            labels = [
//                ds_name = target
//                type = sparse
//                dimension = 21
//                ignoreValue = 255
//                ignoreStreamName = ignoreMask
//            ]
//        ]
//    ]

string ImageDatasetConfigHelper::GetLoadConfigPath(const ConfigParameters& config)
{
    return config(L"loadConfigFile");
}

vector<StreamDescriptor> ImageDatasetConfigHelper::GetStreamDescriptors(const ConfigParameters& config)
{
    // We expect stream descriptions to be array under "streams".
    ConfigParameters streams = config("streams");
    vector<StreamDescriptor> streamDescriptors;
    for (const pair<string, ConfigParameters>& stream : streams)
    {
        StreamDescriptor newStreamDescriptor;
        newStreamDescriptor.name = stream.first;
        if (stream.second.find("ds_name") == stream.second.end())
        {
            RuntimeError("No ds_name declared in stream %s.", stream.first);
        }
        if (stream.second.find("type") == stream.second.end())
        {
            RuntimeError("No type declared in stream %s.", stream.first);
        }
        newStreamDescriptor.datasetName = stream.second.find("ds_name")->second;
        string type = stream.second.find("type")->second;
        if (type == "dense")
        {
            newStreamDescriptor.datasetStorageType = StorageType::dense;
            newStreamDescriptor.dimension = -1;
        }
        else if (type == "sparse")
        {
            newStreamDescriptor.datasetStorageType = StorageType::sparse_csc;
            if (stream.second.find("dimension") == stream.second.end())
            {
                RuntimeError("No dimension declared for sparse stream %s.", stream.first);
            }
            newStreamDescriptor.dimension = stream.second.find("dimension")->second;

            // Check if we have ignore stream.
            if (stream.second.find("ignoreValue") != stream.second.end())
            {
                if (stream.second.find("ignoreStreamName") == stream.second.end())
                {
                    RuntimeError("Ignore value specified without ignore stream name");
                }
                int ignoreValue = stream.second.find("ignoreValue")->second;
                string ignoreStreamName = stream.second.find("ignoreStreamName")->second;
                newStreamDescriptor.ignoreStream = make_shared<IgnoreStreamDescriptor>();
                newStreamDescriptor.ignoreStream->ignoreLabel = ignoreValue;
                newStreamDescriptor.ignoreStream->ignoreStreamName = ignoreStreamName;
            }
        }
        else
        {
            RuntimeError("Invalid stream type %s.", type.c_str());
        }
        streamDescriptors.push_back(newStreamDescriptor);
    }
    if (streamDescriptors.size() == 0)
    {
        RuntimeError("No streams declared in ids config.");
    }
    return streamDescriptors;
}

}}}
