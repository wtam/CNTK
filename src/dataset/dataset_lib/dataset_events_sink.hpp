// Defines dataset events sink interface.
#pragma once

class DatasetEventsSink
{
public:
  virtual void DataReadThreadsCount(int count) = 0;
  virtual void DataReadStart(int data_read_thread_id) = 0;
  virtual void DataReadEnd(int data_read_thread_id, size_t bytes_read) = 0;

  virtual void ImageProcessingThreadsCount(int count) = 0;
  virtual void ImageProcessingStart(int im_proc_thread_id) = 0;
  virtual void ImageProcessingEnd(int im_proc_thread_id) = 0;

protected:
  virtual ~DatasetEventsSink() = default;
};