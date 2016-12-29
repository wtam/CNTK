# Example script for testing converter.

# Include test function.
. (Join-Path -Path $PSScriptRoot -ChildPath "Test-Converter.ps1")

$Cntk = "D:\Users\movasi\CNTK\analog_cntk_03\x64\Release\CNTK.exe"
$Converter = "D:\Users\movasi\converter\build\x64\Debug\cntk_to_caffe.exe"
$CaffeMnistConverter="D:\Users\movasi\Caffe\src_02\Build\x64\Debug\convert_mnist_data.exe"
$Caffe = "D:\Users\movasi\Caffe\src_02\Build\x64\Debug\caffe.exe"
Test-Converter `
    -Cntk $Cntk `
    -Caffe $Caffe `
    -Converter $Converter `
    -CaffeMnistConverter $CaffeMnistConverter
