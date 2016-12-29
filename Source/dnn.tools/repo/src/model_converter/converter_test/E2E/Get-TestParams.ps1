function Get-TestParams
{    <#
    .SYNOPSIS
    Gets parameters for testing CNTK to Caffe converter.
    .DESCRIPTION
    Gets parameters for testing CNTK to Caffe converter: CNTK/Caffe config files, data directory, CNTK/Caffe data sets, CNTK/Caffe models...
    .EXAMPLE
    Get-TestParams -Root . -Test 01_OneHidden
    .EXAMPLE
    Get-TestParams -Root $PSScriptRoot -Test 01_OneHidden
    .PARAMETER Root
    Folder that contains config files and scripts for downloading data.
    .PARAMETER Test
    Test for which test parameters should be returned.
    #>
    [CmdletBinding()]
    Param(
        [Parameter(Mandatory = $True)]
        [ValidateNotNullOrEmpty()]
        [string]$Root,

        [Parameter(Mandatory = $True)]
        [ValidateNotNullOrEmpty()]
        [ValidateSet("01_OneHidden", "02_OneConv", "04_OneConvBN")]
        [string]$Test
    )
    $expectedAccuracy = @{
        "01_OneHidden" = 0.980469
        "02_OneConv" = 0.9850
        "04_OneConvBN" = 0.987656
    }
    $backend = "leveldb"
    $outputDir = Join-Path $Root Output
    $details = @{
        DataDir = Join-Path $Root DataSets/MNIST
        OutputDir = $outputDir
        CntkTrainSet = 'Train-28x28_cntk_text.txt'
        CntkTestSet = 'Test-28x28_cntk_text.txt'
        CntkModel = Join-Path $outputDir Models/$Test
        CntkConfig = Join-Path $Root Config/$Test.cntk
        CaffeTrainSet = "mnist_train_$backend"
        CaffeTestSet = "mnist_test_$backend"
        CaffeBackend = $backend
        CaffeModel = Join-Path $outputDir "$Test.prototxt"
        CaffeArchitecture = Join-Path $outputDir "$Test.prototxt.model"
        CaffeArchitectureTest = Join-Path $Root Baseline/$Test.test.prototxt
        CaffeArchitectureBaseline = Join-Path $Root Baseline/$Test.expected.prototxt
        CaffeExpectedAccuracy = $expectedAccuracy[$Test]
        CaffeAccuracyThreshold = 0.0001
    }

    return $details
}