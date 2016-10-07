//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include "stdafx.h"

#include "ImageDatasetReader.h"

#include "DataReader.h"
#include "dataset_io.hpp"
#include "dataset_events_sink.hpp"
#include "FramePacker.h"
#include "ImageDatasetConfigHelper.h"
#include "Transformer.h"

#include <assert.h>
#include <memory>
#include <numeric>
#include <vector>

using namespace std;

namespace Microsoft { namespace MSR { namespace CNTK {

// Implementation of interface required by data loader.
class ImageDatasetExample : public IExample<float>
{
public:
    // We expect image like blobs/streams from dataset.
    static const int c_blob_dims = 3;

    ImageDatasetExample(const vector<string>& blobNames)
    {
        // blobNames are list of blob names we are interested in.
        if (blobNames.size() == 0)
        {
            RuntimeError("Empty blob names vector provided.");
        }
        // Save blob names and resize other vectors appropriately.
        m_blobNames = blobNames;
        m_blobs.resize(blobNames.size());
        m_blob_shapes.resize(blobNames.size());
    }

    virtual void ReshapeBlob(int index, int channels, int height, int width) override
    {
        // This method is called by dataset loader before asking for memory to copy blob to.
        static_assert(c_blob_dims == 3, "Invalid blob dims.");
        m_blob_shapes[index][0] = channels;
        m_blob_shapes[index][1] = height;
        m_blob_shapes[index][2] = width;
        m_blobs[index].resize(channels * height * width);
    }

    virtual float* GetBlobMemory(int index) override
    {
        // Called by dataset loader. Blob memory will be copied here.
        return m_blobs[index].data();
    }

    const array<int, c_blob_dims>* GetBlobShape(const string& blobName)
    {
        // Return shape for the blob with the given name.
        int index = BlobIndexFromName(blobName);
        return &m_blob_shapes[index];
    }

    // Swaps contents of the blob with the given name with the given vector.
    const void SwapBlobData(const string& blobName, vector<float>& outer)
    {
        int index = BlobIndexFromName(blobName);
        m_blobs[index].swap(outer);
    }

private:
    // Helper method that returns blob index based on blob name.
    int BlobIndexFromName(const string& blobName)
    {
        int index = -1;
        for (size_t ib = 0; ib < m_blobNames.size(); ib++)
        {
            if (blobName == m_blobNames[ib])
            {
                index = static_cast<int>(ib);
                break;
            }
        }
        if (index == -1)
        {
            RuntimeError("Blob with name %s not found.", blobName.c_str());
        }
        return index;
    }

private:
    // List of the all blob names we are interested in.
    vector<string> m_blobNames;
    // Memory for all blobs.
    vector<vector<float> > m_blobs;
    // Shapes fro all the blobs.
    vector<array<int, c_blob_dims> > m_blob_shapes;
};

// We extend dense sequence base and add memory we own to it.
struct DenseSequenceDataIds : public DenseSequenceData
{
public:
    vector<float> m_ownedData;
};
typedef shared_ptr<DenseSequenceDataIds> DenseSequenceDataIdsPtr;

// We extend sparse sequence base and add nonzero indices and values memory.
struct SparseSequenceDataIds : public SparseSequenceData
{
public:
    vector<IndexType> m_indicesMemory;
    vector<float> m_valuesMemory;
};
typedef shared_ptr<SparseSequenceDataIds> SparseSequenceDataIdsPtr;

///////////////////////////////////////////////////////////////////////////////
//                      DATASET EVENTS IMPLEMENATION
///////////////////////////////////////////////////////////////////////////////
class DatasetEventsSinkImpl : public DatasetEventsSink
{
public:
  virtual void DataReadThreadsCount(int /*count*/) override
  {
    // TODO: add perf markers.
  }

  virtual void DataReadStart(int /*data_read_thread_id*/) override
  {
    // TODO: add perf markers.
  }

  virtual void DataReadEnd(int /*data_read_thread_id*/, size_t /*bytes_read*/) override
  {
    // TODO: add perf markers.
  }

  virtual void ImageProcessingThreadsCount(int /*count*/) override
  {
    // TODO: add perf markers.
  }

  virtual void ImageProcessingStart(int /*im_proc_thread_id*/) override
  {
    // TODO: add perf markers.
  }

  virtual void ImageProcessingEnd(int /*im_proc_thread_id*/) override
  {
    // TODO: add perf markers.
  }
};

///////////////////////////////////////////////////////////////////////////////
//                      DATA SOURCE IMPLEMENATION
///////////////////////////////////////////////////////////////////////////////

// Object that connects packer (which creates final data batch) and dataset
// loader. Packer requires SequenceEnumerator from which it pull data so we extend that
// interface to be able to communicate with packer.
class DataSource : public SequenceEnumerator
{
public:
    DataSource(const ConfigParameters& config)
    {
        m_currEpochSampleCount = 0;

        unique_ptr<DatasetEventsSinkImpl> events_sink = make_unique<DatasetEventsSinkImpl>();

        // Kick off loading the dataset.
        m_dsLoader = CreateLoader<float>(ImageDatasetConfigHelper::GetLoadConfigPath(config), nullptr, move(events_sink));

        // Take names of the blobs inside dataset.
        int blobsCount = m_dsLoader->GetBlobsCount();
        vector<string> blobNames;
        for (int ib = 0; ib < blobsCount; ib++)
        {
            blobNames.emplace_back(m_dsLoader->GetBlobName(ib));
        }

        // Take one example to be used for tensor shape retrieval.
        m_example = make_unique<ImageDatasetExample>(blobNames);
        m_dsLoader->GetExample(m_example.get());

        // Create input and output streams. Stream is identical to required blob outputs from this reader.
        m_streamDescriptors = ImageDatasetConfigHelper::GetStreamDescriptors(config);
        int max_dimension = 0;
        int stream_id_in = 0;
        int stream_id_out = 0;
        for (size_t ib = 0; ib < m_streamDescriptors.size(); ib++)
        {
            const StreamDescriptor& streamDescriptor = m_streamDescriptors[ib];

            // Ensure we have blob with given name in dataset.
            if (find(blobNames.begin(), blobNames.end(), streamDescriptor.datasetName) == blobNames.end())
            {
                RuntimeError("Blob with name %s not found in image dataset.", streamDescriptor.datasetName.c_str());
            }

            // Take blob shape to be able to provide tensor shape.
            array<int, 3> shape = *m_example->GetBlobShape(streamDescriptor.datasetName);
            // Shape provided by image dataset has last dimension last, here we need last dimension first.
            reverse(shape.begin(), shape.end());

            // Create new input stream description and fill in the required fields.
            StreamDescriptionPtr inputStreamDescription = make_shared<StreamDescription>();
            m_inputStreams.push_back(inputStreamDescription);
            inputStreamDescription->m_id = stream_id_in++;
            inputStreamDescription->m_name = msra::strfun::utf16(streamDescriptor.name);
            inputStreamDescription->m_elementType = ElementType::tfloat;
            inputStreamDescription->m_storageType = streamDescriptor.datasetStorageType;
            if (streamDescriptor.datasetStorageType == StorageType::sparse_csc)
            {
                // In case of sparse data we expect one value in last dimension.
                if (shape[2] != 1)
                {
                    RuntimeError("Invalid image dataset shape for sparse data.");
                }
                // Final layout of the sample is dense, its last dimension must be declared in config.
                inputStreamDescription->m_sampleLayout = make_shared<TensorShape>(shape[0], shape[1], streamDescriptor.dimension);
                if (streamDescriptor.dimension > max_dimension)
                {
                    max_dimension = streamDescriptor.dimension;
                }

                // Check if input stream produces ignore label stream as well.
                if (streamDescriptor.ignoreStream != nullptr)
                {
                    StreamDescriptionPtr inputStreamDescriptionIgnore = make_shared<StreamDescription>();
                    m_inputStreams.push_back(inputStreamDescriptionIgnore);
                    inputStreamDescriptionIgnore->m_id = stream_id_in++;
                    inputStreamDescriptionIgnore->m_name = msra::strfun::utf16(streamDescriptor.ignoreStream->ignoreStreamName);
                    inputStreamDescriptionIgnore->m_elementType = ElementType::tfloat;
                    // Ignore stream is always dense.
                    inputStreamDescriptionIgnore->m_storageType = StorageType::dense;
                    inputStreamDescriptionIgnore->m_sampleLayout = make_shared<TensorShape>(shape[0], shape[1], 1);
                }
            }
            else
            {
                // Shape is equal to one pulled from the dataset.
                inputStreamDescription->m_sampleLayout = make_shared<TensorShape>(shape[0], shape[1], shape[2]);
            }

            // Create new output stream description and fill in the required fields. Same as previous one just storage
            // type must be dense.
            StreamDescriptionPtr outputStreamDescription = make_shared<StreamDescription>();
            m_outputStreams.push_back(outputStreamDescription);
            outputStreamDescription->m_id = stream_id_out++;
            outputStreamDescription->m_name = msra::strfun::utf16(streamDescriptor.name);
            outputStreamDescription->m_elementType = ElementType::tfloat;
            outputStreamDescription->m_storageType = StorageType::dense;
            if (streamDescriptor.datasetStorageType == StorageType::sparse_csc)
            {
                outputStreamDescription->m_sampleLayout = make_shared<TensorShape>(shape[0], shape[1], streamDescriptor.dimension);
                if (streamDescriptor.ignoreStream != nullptr)
                {
                    StreamDescriptionPtr outputStreamDescriptionIgnore = make_shared<StreamDescription>();
                    m_outputStreams.push_back(outputStreamDescriptionIgnore);
                    outputStreamDescriptionIgnore->m_id = stream_id_out++;
                    outputStreamDescriptionIgnore->m_name = msra::strfun::utf16(streamDescriptor.ignoreStream->ignoreStreamName);
                    outputStreamDescriptionIgnore->m_elementType = ElementType::tfloat;
                    // Ignore stream is always dense.
                    outputStreamDescriptionIgnore->m_storageType = StorageType::dense;
                    outputStreamDescriptionIgnore->m_sampleLayout = make_shared<TensorShape>(shape[0], shape[1], 1);
                }
            }
            else
            {
                outputStreamDescription->m_sampleLayout = make_shared<TensorShape>(shape[0], shape[1], shape[2]);
            }

        }
    }

    virtual vector<StreamDescriptionPtr> GetStreamDescriptions() const override
    {
        // Delegate call to accessor.
        return GetInputStreamDescriptions();
    }

    vector<StreamDescriptionPtr> GetInputStreamDescriptions() const
    {
        // Return input stream description.
        return m_inputStreams;
    }

    vector<StreamDescriptionPtr> GetOutputStreamDescriptions() const
    {
        // Return output stream description.
        return m_outputStreams;
    }

    virtual void StartEpoch(const EpochConfiguration& config) override
    {
        // Save current minibatch size.
        m_minibatchSize = config.m_minibatchSizeInSamples;
        if (UseAllExamplesFromDatasetForEpoch(config))
        {
            // We take all examples from dataset for one epoch.
            m_epochSize = static_cast<size_t>(m_dsLoader->GetExamplesCount());
        }
        else
        {
            // We take given number of examples for one epoch.
            m_epochSize = config.m_totalEpochSizeInSamples;
        }
        m_currEpochSampleCount = 0;
    }

    virtual Sequences GetNextSequences(size_t sampleCount) override
    {
        // This method needs to return final (output) data in a form of set of sequences.
        Sequences sequences;

        // Check if we are about to read all samples in one epoch and set flag if this is the case.
        if (m_currEpochSampleCount + sampleCount >= m_epochSize)
        {
            sequences.m_endOfEpoch = true;
        }

        sequences.m_data.resize(m_inputStreams.size());

        // For each sequence we provide several streams.
        for (int istr = 0; istr < sequences.m_data.size(); istr++)
        {
            sequences.m_data[istr].resize(sampleCount);
        }

        // Now fill in the sequence data one by one.
        for (size_t ismpl = 0; ismpl < sampleCount; ismpl++)
        {
            // Go over the streams of the sequence.
            size_t istr = 0;
            size_t iStreamDesc = 0;
            while (istr < m_inputStreams.size())
            {
                // Take stream descriptor for the current sequence.
                const StreamDescriptor& streamDescriptor = m_streamDescriptors[iStreamDesc];
                // Increment stream descriptor index for the next cycle.
                iStreamDesc++;

                // We need to store different sequence object based on storage type.
                if (m_inputStreams[istr]->m_storageType == StorageType::dense)
                {
                    if (streamDescriptor.ignoreStream != nullptr)
                    {
                        RuntimeError("Dense input cannot have ignore label.");
                    }

                    // For dense sequence we use DenseSequenceDataIds.
                    DenseSequenceDataIdsPtr newSequenceDataDense = make_shared<DenseSequenceDataIds>();

                    array<int, 3> shape = *m_example->GetBlobShape(streamDescriptor.datasetName);
                    // Shape provided by image dataset has last dimension last, here we need last dimension first.
                    reverse(shape.begin(), shape.end());

                    // Move data from example to sequence. Although we reversed the shape we should not alter data since
                    // expected memory layout is the same (just shape notation is different).
                    m_example->SwapBlobData(streamDescriptor.datasetName, newSequenceDataDense->m_ownedData);
                    // Fill in the base class fields.
                    newSequenceDataDense->m_data = newSequenceDataDense->m_ownedData.data();
                    newSequenceDataDense->m_id = ismpl;
                    newSequenceDataDense->m_numberOfSamples = 1;
                    newSequenceDataDense->m_chunk = nullptr;
                    newSequenceDataDense->m_sampleLayout = make_shared<TensorShape>(shape[0], shape[1], shape[2]);

                    // Save new sequence in the set of sequences.
                    sequences.m_data[istr][ismpl] = newSequenceDataDense;

                    // Move to next stream.
                    istr++;
                }
                else
                {
                    vector<float>* ignoreBuffer = nullptr;
                    if (streamDescriptor.ignoreStream != nullptr)
                    {
                        if (istr + 1 >= m_inputStreams.size())
                        {
                            RuntimeError("Invalid number of input streams (sparse stream is not followed by ignore stream).");
                        }

                        // Dimensions of the output ignore tensor (height x width x 1, where values are 1 or 0 - zero
                        // means ignore classification result at that spatial position).
                        auto ignoreDims = m_inputStreams[istr + 1]->m_sampleLayout->GetDims();

                        DenseSequenceDataIdsPtr newSequenceDataDenseIgnore = make_shared<DenseSequenceDataIds>();

                        // We set all ignore output values to 1. Below we will set values to zero where output should be ignored.
                        newSequenceDataDenseIgnore->m_ownedData.assign(ignoreDims[0] * ignoreDims[1] * ignoreDims[2], 1);
                        newSequenceDataDenseIgnore->m_data = newSequenceDataDenseIgnore->m_ownedData.data();
                        newSequenceDataDenseIgnore->m_id = ismpl;
                        newSequenceDataDenseIgnore->m_numberOfSamples = 1;
                        newSequenceDataDenseIgnore->m_chunk = nullptr;
                        newSequenceDataDenseIgnore->m_sampleLayout = make_shared<TensorShape>(ignoreDims[0], ignoreDims[1], ignoreDims[2]);

                        // Save ignore sequence in the set of sequences.
                        sequences.m_data[istr + 1][ismpl] = newSequenceDataDenseIgnore;

                        // Save ignore buffer to be filled in below.
                        ignoreBuffer = &newSequenceDataDenseIgnore->m_ownedData;
                    }

                    // For sparse sequence we use SparseSequenceDataIds.
                    SparseSequenceDataIdsPtr newSequenceDataSparse = make_shared<SparseSequenceDataIds>();

                    auto dims = m_inputStreams[istr]->m_sampleLayout->GetDims();

                    // Move data from example to sequence.
                    vector<float> data;
                    m_example->SwapBlobData(streamDescriptor.datasetName, data);
                    if (data.size() != dims[0] * dims[1])
                    {
                        RuntimeError("Unexpected sparse data count");
                    }

                    newSequenceDataSparse->m_id = ismpl;
                    newSequenceDataSparse->m_numberOfSamples = 1;
                    newSequenceDataSparse->m_chunk = nullptr;

                    // We have data.size() non-zero values, set necessary counts.
                    newSequenceDataSparse->m_nnzCounts.resize(1);
                    newSequenceDataSparse->m_nnzCounts[0] = static_cast<IndexType>(data.size());
                    newSequenceDataSparse->m_totalNnzCount = static_cast<IndexType>(data.size());
                    // All the non-zero values are equal to 1.
                    newSequenceDataSparse->m_valuesMemory.resize(data.size(), 1.0f);
                    newSequenceDataSparse->m_data = newSequenceDataSparse->m_valuesMemory.data();
                    // Now we need to convert indices contained in data.
                    newSequenceDataSparse->m_indicesMemory.resize(data.size());
                    size_t spatialSize = dims[0] * dims[1];
                    // Out channels dimension is equal to number of outputs (we need distribution per class).
                    size_t outChannels = dims[2];
                    for (size_t inz = 0; inz < data.size(); inz++)
                    {
                        int c = static_cast<int>(data[inz]);

                        if (ignoreBuffer != nullptr && c == streamDescriptor.ignoreStream->ignoreLabel)
                        {
                            (*ignoreBuffer)[inz] = 0;
                            continue;
                        }

                        // Here we need to be within the range.
                        if (c < 0 || c > outChannels - 1)
                        {
                            RuntimeError("Invalid channel value in sparse input stream.");
                        }

                        size_t index = c * spatialSize + inz;
                        newSequenceDataSparse->m_indicesMemory[inz] = static_cast<IndexType>(index);
                    }
                    newSequenceDataSparse->m_indices = newSequenceDataSparse->m_indicesMemory.data();

                    // Save new sequence in the set of sequences.
                    sequences.m_data[istr][ismpl] = newSequenceDataSparse;

                    // Move to the next stream.
                    istr++;
                    if (streamDescriptor.ignoreStream != nullptr)
                    {
                        // We had one additional stream (ignore mask).
                        istr++;
                    }
                }
            }
            // Move to the next example (sequence).
            m_dsLoader->GetExample(m_example.get());
        }

        m_currEpochSampleCount += static_cast<int>(sampleCount);

        return sequences;
    }

    bool UseAllExamplesFromDatasetForEpoch(const EpochConfiguration& config)
    {
        // If epoch size is equal to magic constant than we need to use all examples per epoch.
        return (config.m_totalEpochSizeInSamples == requestDataSize);
    }

private:

    // Input stream description describes the data coming out of dataset loader.
    vector<StreamDescriptionPtr> m_inputStreams;
    // Output stream description describes the data coming out of this object.
    vector<StreamDescriptionPtr> m_outputStreams;
    // Stream descriptions from the config.
    vector<StreamDescriptor> m_streamDescriptors;

    // Performs loading of the dataset.
    unique_ptr<IDsLoader<float> > m_dsLoader;
    // Object used for storing results from dataset loader.
    unique_ptr<ImageDatasetExample> m_example;

    // Size of the current minibatch.
    size_t m_minibatchSize;

    size_t m_epochSize;
    size_t m_currEpochSampleCount;
};

///////////////////////////////////////////////////////////////////////////////
//                      IMAGE DATASET READER IMPLEMENATION
///////////////////////////////////////////////////////////////////////////////

ImageDatasetReader::ImageDatasetReader(MemoryProviderPtr provider,
                                       const ConfigParameters& config)
    : m_provider(provider)
{
    // Create data source and connect it to the packer.
    m_dataSource = make_shared<DataSource>(config);

    m_packer = make_shared<FramePacker>(
        m_provider,
        m_dataSource,
        m_dataSource->GetOutputStreamDescriptions());
}

vector<StreamDescriptionPtr> ImageDatasetReader::GetStreamDescriptions()
{
    // Descriptions are saved in datasource, just forward the call.
    return m_dataSource->GetOutputStreamDescriptions();
}

void ImageDatasetReader::StartEpoch(const EpochConfiguration& config)
{
    if (config.m_totalEpochSizeInSamples == 0)
    {
        RuntimeError("Epoch size cannot be 0.");
    }

    // Inform child objects that new epoch started.
    m_dataSource->StartEpoch(config);
    m_packer->StartEpoch(config);
}

Minibatch ImageDatasetReader::ReadMinibatch()
{
    // Ask packer to provide data.
    return m_packer->ReadMinibatch();
}
} } }
