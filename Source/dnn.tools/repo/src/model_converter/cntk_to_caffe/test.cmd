@echo off

:: This script should run from Caffe root in order to be able to read data in testing part.

set configuration=Release

:: Set paths to tools
set caffe=D:\Caffe\master\Build\x64\%configuration%\caffe.exe
set converter=D:\dnn_tools\cntk_to_caffe_conv\build\x64\%configuration%\cntk_to_caffe.exe

:: Set paths to models.
set cntk_model=D:\CNTK\e01\code\Examples\Image\MNIST\Output\Models_gpu\Models\03_ConvBatchNorm
set caffe_weights=D:\Temp\Dump\mnist_03_ConvBatchNorm.prototxt
set caffe_model=D:\Temp\Dump\mnist_03_ConvBatchNorm.prototxt.model.ex

del %caffe_weights%
:: Perform conversion.
echo %converter% %cntk_model% %caffe_weights%
%converter% %cntk_model% %caffe_weights%

:: Data layer and accuracy layer in Caffe model should be manually modified in order to run test.

:: Test Caffe.
echo %caffe% test --model=%caffe_model% --weights=%caffe_weights%
%caffe% test --model=%caffe_model% --weights=%caffe_weights%
