# Include common functions.
. (Join-Path -Path $PSScriptRoot -ChildPath "..\..\..\..\scripts\common\common.ps1")

function Download-MnistDatasetCNTK
{
    <#
    .SYNOPSIS
    Download MNIST data set in format consumable by CNTK.
    .DESCRIPTION
    Download MNIST data set in format consumable by CNTK to data folder if necessary. Download dir should contain python scripts for downloading data. Python executable should be in the PATH environment variable.
    .EXAMPLE
    Download-MnistDatasetCNTK -DataDir D:\Test\converter_test\E2E\DataSets\MNIST -TrainingSet Train-28x28_cntk_text.txt -TestSet Test-28x28_cntk_text.txt
    .PARAMETER DataDir
    Download dir that contains python scripts for downloading data and will contain MNIST data in format consumable by CNTK.
    .PARAMETER TrainingSet
    File name of training set, e.g. Train-28x28_cntk_text.txt.
    .PARAMETER TestSet
    File name of test set, e.g. Test-28x28_cntk_text.txt.
    #>
    [CmdletBinding()]
    Param(
        [Parameter(Mandatory = $True)]
        [ValidateNotNullOrEmpty()]
        [string]$DataDir,

        [Parameter(Mandatory = $False)]
        [ValidateNotNullOrEmpty()]
        [string]$TrainingSet = "Train-28x28_cntk_text.txt",

        [Parameter(Mandatory = $False)]
        [ValidateNotNullOrEmpty()]
        [string]$TestSet = "Test-28x28_cntk_text.txt"
    )

    $trainSetFullPath = Join-Path $DataDir $TrainingSet
    $testSetFullPath = Join-Path $DataDir $TestSet
    If ((Test-Path $trainSetFullPath) -and (Test-Path $testSetFullPath))
    {
        Write-Host "MNIST dataset exists. Skipping download..." -ForegroundColor Green
        return
    }

    # Check that scripts for downloading MNIST data exist.
    $mainMnistDownloadScript = Join-Path $DataDir install_mnist.py
    $downloadScripts = @($mainMnistDownloadScript, (Join-Path $DataDir mnist_utils.py))
    Foreach ($script in $downloadScripts)
    {
        Check `
            -Condition (Test-Path $script) `
            -Message "Missing download script $script"
    }

    # Download MNIST data.
    Push-Location $DataDir
    Write-Host "Downloading MNIST data..." -ForegroundColor Yellow
    $downloadCommand = "python $mainMnistDownloadScript"
    Invoke-Expression -Command $downloadCommand
    Pop-Location

    # Check if created test sets exist.
    Check `
        -Condition (Test-Path $trainSetFullPath) `
        -Message "Training set $trainSetFullPath missing"
    Check `
        -Condition (Test-Path $testSetFullPath) `
        -Message "Test set $testSetFullPath missing"
    Write-Host "MNIST data downloaded" -ForegroundColor Green
}

function Download-MnistDatasetCaffe
{
    <#
    .SYNOPSIS
    Download MNIST data set in format consumable by Caffe.
    .DESCRIPTION
    Download MNIST data set in format consumable by Caffe to data folder, if necessary.
    .EXAMPLE
    Download-MnistDatasetCaffe -DataDir D:\Test\converter_test\E2E\DataSets\MNIST -Backend leveldb -TrainingSet mnist_train_leveldb -TestSet mnist_test_leveldb
    .PARAMETER DataDir
    Download dir that will contain MNIST data in format consumable by Caffe and that will be used as working folder for the process of converting data.
    .PARAMETER TrainingSet
    File name of training set, e.g. mnist_train_leveldb.
    .PARAMETER TestSet
    File name of test set, e.g. mnist_test_leveldb.
    .PARAMETER $CaffeMnistConverter
    Binary that converts downloaded MNIST data set to format that can be used in Caffe.
    #>
    [CmdletBinding()]
    Param(
        [Parameter(Mandatory = $True)]
        [ValidateNotNullOrEmpty()]
        [string]$DataDir,

        [Parameter(Mandatory = $True)]
        [ValidateNotNullOrEmpty()]
        [ValidateSet("leveldb", "lmdb")]
        [string]$Backend = "leveldb",

        [Parameter(Mandatory = $True)]
        [ValidateNotNullOrEmpty()]
        [string]$TrainingSet,

        [Parameter(Mandatory = $True)]
        [ValidateNotNullOrEmpty()]
        [string]$TestSet,

        [Parameter(Mandatory = $True)]
        [ValidateNotNullOrEmpty()]
        [string]$CaffeMnistConverter
    )
    Write-Host "Creating MNIST dataset for Caffe..." -ForegroundColor Yellow

    $mnistFiles = @()
    $mnistFiles += New-Object psobject @{ArchiveName="train-images-idx3-ubyte"; Name="train-images.idx3-ubyte"}
    $mnistFiles += New-Object psobject @{ArchiveName="train-labels-idx1-ubyte"; Name="train-labels.idx1-ubyte"}
    $mnistFiles += New-Object psobject @{ArchiveName="t10k-images-idx3-ubyte"; Name="t10k-images.idx3-ubyte"}
    $mnistFiles += New-Object psobject @{ArchiveName="t10k-labels-idx1-ubyte"; Name="t10k-labels.idx1-ubyte"}

    $trainFinal = Join-Path $DataDir $TrainingSet
    $testFinal = Join-Path $DataDir $TestSet
    If ((Test-Path $trainFinal) -and (Test-Path $testFinal))
    {
        Write-Host "Caffe MNIST dataset files already exist. Nothing to do." -ForegroundColor Green
        return
    }

    $webclient = New-Object System.Net.WebClient

    ForEach ($file in $mnistFiles)
    {
        # Download and extract MNIST data.
        $extractedFile = Join-Path $DataDir $file.Name
        If (-not (Test-Path $extractedFile))
        {
            $download = Join-Path $DataDir "$($file.ArchiveName).gz"
            If (-not (Test-Path $download))
            {
                Write-Host "Downloading file $($file.ArchiveName)..."
                $url = "http://yann.lecun.com/exdb/mnist/$($file.ArchiveName).gz"
                Write-Host "Downloading from $url to $download"
                $webclient.DownloadFile($url, $download)
                Check `
                    -Condition (Test-Path $download) `
                    -Message "Failed downloading $file file"
                Write-Host "Downloaded file $($file.ArchiveName)." -ForegroundColor DarkGreen
            }
            Write-Host "Extracting file $($file.Name)..."
            Expand-Archive $download $DataDir
            Write-Host "Extracted file $($file.Name)." -ForegroundColor DarkGreen
        }
    }

    # Convert extracted data to format that can be consumed by Caffe.
    # Convert training set.
    If (Test-Path $trainFinal)
    {
        Write-Host "Training data set already exist."
    }
    Else
    {
        Write-Host "Converting training set to $Backend format..."
        $trainData = Join-Path $DataDir $mnistFiles[0].Name
        $trainLabels = Join-Path $DataDir $mnistFiles[1].Name
        $mnistConvertCommand = "$CaffeMnistConverter $trainData $trainLabels $trainFinal --backend=$Backend"
        Invoke-Expression -Command $mnistConvertCommand
        Write-Host "Converted training set to $Backend format." -ForegroundColor DarkGreen
    }
    # Convert test set.
    If (Test-Path $testFinal)
    {
        Write-Host "Test data set already exist."
    }
    Else
    {
        Write-Host "Converting test set to $Backend format..."
        $testData = Join-Path $DataDir $mnistFiles[2].Name
        $testLabels = Join-Path $DataDir $mnistFiles[3].Name
        $mnistConvertCommand = "$CaffeMnistConverter $testData $testLabels $testFinal --backend=$Backend"
        Invoke-Expression -Command $mnistConvertCommand
        Write-Host "Converted test set to $Backend format." -ForegroundColor DarkGreen
    }

    Write-Host "Created MNIST dataset for Caffe" -ForegroundColor Green
}