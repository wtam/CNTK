# Include common functions.
. (Join-Path -Path $PSScriptRoot -ChildPath "..\..\..\..\scripts\common\common.ps1")

function Convert-CntkToCaffe
{
    <#
    .SYNOPSIS
    Converts model from CNTK format to Caffe format.
    .DESCRIPTION
    Converts model from CNTK format to Caffe format. Complete output Caffe model will be stored on <CaffeModel> file. Caffe network architecture will be stored to <CaffeModel>.model.
    .EXAMPLE
    Convert-Model -Converter D:\Test\converter_test\bin\cntk_to_caffe.exe -CntkModel D:\Test\converter_test\bin\cntk_model -CaffeModel D:\Test\converter_test\bin\caffe.prototxt
    .PARAMETER Converter
    Path to binary for converting CNTK model to Caffe.
    .PARAMETER CntkModel
    Path to input CNTK model.
    .PARAMETER CaffeModel
    Path to output Caffe model.
    #>
    [CmdletBinding()]
    Param(
        [Parameter(Mandatory = $True)]
        [ValidateNotNullOrEmpty()]
        [string]$Converter,

        [Parameter(Mandatory = $True)]
        [ValidateNotNullOrEmpty()]
        [string]$CntkModel,

        [Parameter(Mandatory = $True)]
        [ValidateNotNullOrEmpty()]
        [string]$CaffeModel
    )

    Check `
        -Condition (Test-Path $Converter) `
        -Message "Converter binary missing: $Converter"

    Write-Host "Converting CNTK model to Caffe..." -ForegroundColor Yellow
    $converterCommand = "$Converter $CntkModel $CaffeModel"
    Invoke-Expression -Command $converterCommand
    Write-Host "Finished converting CNTK model to Caffe." -ForegroundColor Green
}

function Train-CntkModel
{
    <#
    .SYNOPSIS
    Trains CNTK model.
    .DESCRIPTION
    Trains CNTK model based on given parameters.
    .EXAMPLE
    Train-CntkModel -Converter D:\Test\converter_test\bin\cntk_to_caffe.exe -CntkModel D:\Test\converter_test\bin\cntk_model -CaffeModel D:\Test\converter_test\bin\caffe.prototxt
    .PARAMETER Model
    Path to CNTK model that should be produced by training. If already exists, training will be skipped.
    .PARAMETER Config
    Configuration file for training CNTK model.
    .PARAMETER DataDir
    Folder that contains training data.
    .PARAMETER OutputDir
    Folder that contains training output.
    #>
    [CmdletBinding()]
    Param(
        [Parameter(Mandatory = $True)]
        [ValidateNotNullOrEmpty()]
        [string]$Cntk,

        [Parameter(Mandatory = $True)]
        [ValidateNotNullOrEmpty()]
        [string]$Model,

        [Parameter(Mandatory = $True)]
        [ValidateNotNullOrEmpty()]
        [string]$Config,

        [Parameter(Mandatory = $True)]
        [ValidateNotNullOrEmpty()]
        [string]$DataDir,

        [Parameter(Mandatory = $True)]
        [ValidateNotNullOrEmpty()]
        [string]$OutputDir
    )

    Check `
        -Condition (Test-Path $Cntk) `
        -Message "Cntk binary missing: $Cntk"

    If (!(Test-Path $Model))
    {
        Write-Host "Training CNTK model..." -ForegroundColor Yellow
        $cpuId = -1
        $command = "$Cntk configFile=$Config dataDir=$DataDir outputDir=$OutputDir device=$cpuId"
        Invoke-Expression -Command $command
        Write-Host "Finished training CNTK model" -ForegroundColor Green

        Check `
            -Condition (Test-Path $Model) `
            -Message "CNTK training finished, but $Model not found."
    }
    Else
    {
        Write-Host "CNTK model already exists. Skipping training." -ForegroundColor Green
    }
}

function Test-CaffeModel
{
    <#
    .SYNOPSIS
    Tests Caffe model.
    .DESCRIPTION
    Tests Caffe model based on given parameters and returns final Caffe accuracy.
    .EXAMPLE
    Test-CaffeModel -Caffe D:\Test\converter_test\bin\caffe.exe -ModelArchitecture D:\Test\converter_test\bin\caffe.prototxt.model -ModelWeights D:\Test\converter_test\bin\caffe.prototxt
    .PARAMETER Caffe
    Path to Caffe binary.
    .PARAMETER ModelArchitecture
    Configuration file that describes Caffe model architecture.
    .PARAMETER ModelWeights
    Configuration file that contains Caffe model architecture with trained weights.
    .PARAMETER TestIteration
    Number of test iterations to run.
    #>
    [CmdletBinding()]
    Param(
        [Parameter(Mandatory = $True)]
        [ValidateNotNullOrEmpty()]
        [string]$Caffe,

        [Parameter(Mandatory = $True)]
        [ValidateNotNullOrEmpty()]
        [string]$ModelArchitecture,

        [Parameter(Mandatory = $True)]
        [ValidateNotNullOrEmpty()]
        [string]$ModelWeights,

        [Parameter(Mandatory = $False)]
        [int]$TestIterations = 100
    )

    Check `
        -Condition (Test-Path $Caffe) `
        -Message "Caffe binary missing: $Caffe"

    $caffeCommand = "$Caffe test --model=$ModelArchitecture --weights=$ModelWeights --iterations=$TestIterations 2>&1"
    $caffeOutput = Invoke-Expression -Command $caffeCommand
    $caffeOutput | Select-String -CaseSensitive -AllMatches -Pattern "accuracy = [\d\.]+" |
    % {
        If ($_ -cmatch "accuracy = ([\d\.]+)")
        {
            Write-Host "Batch accuracy = $($Matches[1])" -ForegroundColor Cyan
        }
    }
    $finalAccuracy = 0
    foreach ($text in $caffeOutput)
    {
        If ($text -cmatch "\] accuracy = ([\d\.]+)")
        {
            $finalAccuracy = $Matches[1]
        }
    }
    return $finalAccuracy
}