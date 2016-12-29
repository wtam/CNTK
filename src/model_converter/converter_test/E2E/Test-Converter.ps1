# Include common functions.
. (Join-Path -Path $PSScriptRoot -ChildPath "..\..\..\..\scripts\common\common.ps1")

function Test-Converter
{
    <#
    .SYNOPSIS
    Tests CNTK to Caffe model converter.
    .DESCRIPTION
    Downloads MNIST data set in format consumable by CNTK/Caffe to data folder, trains CNTK model, converts it to Caffe, verifies converted Caffe model.
    .PARAMETER Cntk
    Path to CNTK binary.
    .PARAMETER Caffe
    Path to Caffe binary.
    .PARAMETER Converter
    Path to CNTK to Caffe model converter binary.
    .PARAMETER CaffeMnistConverter
    Path to binary that converts MNIST data set to Caffe consumable format.
    #>
    [CmdletBinding()]
    Param(
        [Parameter(Mandatory = $True)]
        [ValidateNotNullOrEmpty()]
        [string]$Cntk,

        [Parameter(Mandatory = $True)]
        [ValidateNotNullOrEmpty()]
        [string]$Caffe,

        [Parameter(Mandatory = $True)]
        [ValidateNotNullOrEmpty()]
        [string]$Converter,

        [Parameter(Mandatory = $True)]
        [ValidateNotNullOrEmpty()]
        [string]$CaffeMnistConverter
        )


    Check `
        -Condition (Test-Path $Cntk) `
        -Message "CNTK binary missing: $Cntk"
    Check `
        -Condition (Test-Path $Converter) `
        -Message "Converter binary missing: $Converter"
    Check `
        -Condition (Test-Path $CaffeMnistConverter) `
        -Message "Caffe MNIST converter binary missing: $CaffeMnistConverter"
    Check `
        -Condition (Test-Path $Caffe) `
        -Message "Caffe binary missing: $Caffe"

    $failure = "FAILURE"
    $success = "SUCCESS"

    # Include test parameters.
    . (Join-Path -Path $PSScriptRoot -ChildPath "Get-TestParams.ps1")

    # Include functions for downloading data.
    . (Join-Path -Path $PSScriptRoot -ChildPath "Prepare-Data.ps1")

    # Include functions for training, testing and converting model from CNTK to
    # Caffe format.
    . (Join-Path -Path $PSScriptRoot -ChildPath "Manage-Models.ps1")

    # For now, test only feed forward network with one layer.
    $tests = @('01_OneHidden', '02_OneConv', '04_OneConvBN') # TODO: load list of tests from config files.
    # Test results will contain three columns: test name, test status and detais.
    $testResults = @()

    ForEach ($test in $tests)
    {
        $testParams = Get-TestParams -Root $PSScriptRoot -Test $test

        # Prepare data for CNTK.
        Download-MnistDatasetCNTK `
            -DataDir $testParams.DataDir `
            -TrainingSet $testParams.CntkTrainSet `
            -TestSet $testParams.CntkTestSet

        # TODO: Convert MNIST to IDS format and use it both for CNTK and Caffe.
        #       This way we ensure that exactly same data enter into CNTK and Caffe.
        #       Also, maintaining download scripts will be much simpler.

        # Prepare data for Caffe.
        $backend = $testParams.CaffeBackend
        Download-MnistDatasetCaffe `
            -DataDir $testParams.DataDir `
            -TrainingSet $testParams.CaffeTrainSet `
            -TestSet $testParams.CaffeTestSet `
            -Backend $testParams.CaffeBackend `
            -CaffeMnistConverter $CaffeMnistConverter

        # TODO: Consider getting model from external storage once we have multiple tests.

        # Train CNTK model.
        Train-CntkModel `
            -Cntk $Cntk `
            -Model $testParams.CntkModel `
            -Config $testParams.CntkConfig `
            -DataDir $testParams.DataDir `
            -OutputDir $testParams.OutputDir

        # Convert CNTK model to Caffe.
        Convert-CntkToCaffe `
            -Converter $Converter `
            -CntkModel $testParams.CntkModel `
            -CaffeModel $testParams.CaffeModel

        # Compare architecture with expected result.
        $baselineArchitecture = Get-Content $testParams.CaffeArchitectureBaseline
        $resultArchitecture = Get-Content $testParams.CaffeArchitecture
        $comparison = Compare-Object $baselineArchitecture $resultArchitecture
        If ($comparison)
        {
            # Files are different.
            $testResults += New-Object psobject @{
                Test = $test
                Status = $failure
                Details = "Unexpected Caffe model architecture after conversion."}
            Continue
        }

        # Test converted Caffe model.
        [float]$finalAccuracy = Test-CaffeModel `
                                    -Caffe $Caffe `
                                    -ModelArchitecture $testParams.CaffeArchitectureTest `
                                    -ModelWeights $testParams.CaffeModel `
                                    -TestIterations 100

        # Compare to expected results.
        Write-Host "Accuracy in Caffe = $finalAccuracy" -ForegroundColor Magenta
        [float]$threshold = $testParams.CaffeAccuracyThreshold
        [float]$expectedAccuracy = $testParams.CaffeExpectedAccuracy
        [float]$accDifference = `
            [System.Math]::Abs($finalAccuracy - $expectedAccuracy)
        If ($accDifference -gt $threshold)
        {
            $Details = "Expected accuracy {0:f4}, predicted accuracy {1:f4}." `
                            -f $expectedAccuracy,$finalAccuracy
            # Result not close enough to expected result.
            $testResults += New-Object psobject @{
                Test=$test
                Status=$failure
                Details=$Details}
        }
        Else
        {
            $testResults += New-Object psobject @{
                Test=$test
                Status=$success
                Details="Test succeeded."}
        }
    }
    $testResults
}