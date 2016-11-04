function Get-PhillyJobList
{
    <#
    .SYNOPSIS
    Gets the list of jobs from Philly.
    .DESCRIPTION
    Gets the list of jobs from Philly using REST API for specified physical and virtual cluster.
    .EXAMPLE
    Get-PhillyQueryStatusBody -PhysicalCluster rr1
    .EXAMPLE
    Get-PhillyQueryStatusBody -PhysicalCluster rr1 -VirtualCluster anlgvc
    .PARAMETER PhysicalCluster
    Physical cluster on Philly, e.g. gcr, rr1
    .PARAMETER VirtualCluster
    Virtual cluster on Philly, e.g. anlgvc
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $False)]
        [ValidateNotNullOrEmpty()]
        [ValidateSet("gcr", "rr1")]
        [string]$PhysicalCluster = "gcr",

        [Parameter(Mandatory = $False)]
        [ValidateNotNullOrEmpty()]
        [ValidateSet("anlgvc")]
        [string]$VirtualCluster = "anlgvc"
        )

        $EndPoint = "https://philly/api/list"

        $body = @{
            clusterId=$PhysicalCluster
            vcId=$VirtualCluster
        }

        Invoke-RestMethod $EndPoint -Body $body -UseDefaultCredentials | Write-Output
}

function Get-PhillyJobStatus
{
    <#
    .SYNOPSIS
    Gets status of specified job from Philly.
    .DESCRIPTION
    Gets status of specified job from Philly using REST API.
    .EXAMPLE
    Get-PhillyQueryStatusBody -JobId application_1474301356012_0130 -PhysicalCluster rr1 -VirtualCluster anlgvc
    .PARAMETER JobId
    Job identifier, e.g. application_1474301356012_0130
    .PARAMETER PhysicalCluster
    Physical cluster on Philly, e.g. gcr, rr1
    .PARAMETER VirtualCluster
    Virtual cluster on Philly, e.g. anlgvc
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $True)]
        [ValidateNotNullOrEmpty()]
        [string]$JobId,

        [Parameter(Mandatory = $False)]
        [ValidateNotNullOrEmpty()]
        [ValidateSet("gcr", "rr1")]
        [string]$PhysicalCluster = "gcr",

        [Parameter(Mandatory = $False)]
        [ValidateNotNullOrEmpty()]
        [ValidateSet("anlgvc")]
        [string]$VirtualCluster = "anlgvc"
        )

        $EndPoint = "https://philly/api/status"
        If ($Verbose)
        {
            $Content = "full"
        }
        Else
        {
            $Content = "partial"
        }

        $body = @{
            clusterId=$PhysicalCluster
            vcId=$VirtualCluster
            jobId=$JobId
            jobType="cntk"
            content=$Content
        }

        Invoke-RestMethod $EndPoint -Body $body -UseDefaultCredentials
}

function Submit-PhillyJob
{
    <#
    .SYNOPSIS
    Submits job to Philly.
    .DESCRIPTION
    Submits job to Philly using REST API.
    .EXAMPLE
    Submit-PhillyJob -Name FCN32 -ConfigFile movasi/FCN32/fcn32.cntk -DataDir /hdfs/anlgvc/datasets/ids/v3/Capture -BuildId 2701 -PhysicalCluster rr1 -VirtualCluster anlgvc
    .EXAMPLE
    Submit-PhillyJob -Name FCN32 -ConfigFile "movasi/FCN32/fcn32.cntk" -DataDir /hdfs/anlgvc/datasets/ids/v3/Capture -BuildId 2701 -ExtraParams "TrainFCN32=[SGD=[minibatchSize=32 learningRatesPerMB=1e-8:1e-7:3.2e-7]]"
    Extra parameters will be used for overriding corresponding fields in ConfigFile
    .PARAMETER Name
    A distinguishing name for the job, e.g. lstmTry4
    .PARAMETER ConfigFile
    Specifies the relative path and filename to starting from after your VC subdirectory to your configuration file.
    For example, if your VC is anlgvc, and in anlgvc_scratch you have your own subdirectory user, and in that user
    directory, you have configuration file, train.config,
    \\storage.rr1.philly.selfhost.corp.microsoft.com\anlgvc_scratch\user\train.config
    then use: user/train.config.
    .PARAMETER DataDir
    Specifies the path to the HDFS directory containing the input data.
    E.g. /hdfs/anlgvc/datasets/ids/v3/Capture
    .PARAMETER BuildId
    Specifies the CNTK build ID from Jenkins that you want Philly to use for this job.
    .PARAMETER ExtraParams
    Extra CNTK parameters to specialize the job. For example, tweaking the learning rate schedule outside the
    configuration file, e.g. TrainFCN32=[SGD=[minibatchSize=32 learningRatesPerMB=1e-8:1e-7:3.2e-7]]
    .PARAMETER PhysicalCluster
    Physical cluster on Philly, e.g. gcr, rr1
    .PARAMETER VirtualCluster
    Virtual cluster on Philly, e.g. anlgvc
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $True)]
        [ValidateNotNullOrEmpty()]
        [string]$Name,

        [Parameter(Mandatory = $True)]
        [ValidateNotNullOrEmpty()]
        [string]$ConfigFile,

        [Parameter(Mandatory = $True)]
        [ValidateNotNullOrEmpty()]
        [string]$DataDir,

        [Parameter(Mandatory = $True)]
        [ValidateNotNullOrEmpty()]
        [string]$BuildId,

        [Parameter(Mandatory = $True)]
        [int]$GpuCount=1,

        [Parameter(Mandatory = $False)]
        [string]$ExtraParams="",

        [Parameter(Mandatory = $False)]
        [string]$PreviousModelPath,

        [Parameter(Mandatory = $False)]
        [ValidateNotNullOrEmpty()]
        [ValidateSet("gcr", "rr1")]
        [string]$PhysicalCluster = "gcr",

        [Parameter(Mandatory = $False)]
        [ValidateNotNullOrEmpty()]
        [ValidateSet("anlgvc")]
        [string]$VirtualCluster = "anlgvc"
        )

        $EndPoint = "https://philly/api/submit"

        $body = @{
            clusterId=$PhysicalCluster
            vcId=$VirtualCluster
            name=$Name
            userName=$env:USERNAME
            inputDir=$DataDir
            minGPUs=$GpuCount
            maxGPUs=$GpuCount
            configFile=$ConfigFile
            buildId=$BuildId
            isdebug=$False
            extraParams=$ExtraParams
            toolType="cntk"
        }

        If ($PSBoundParameters.ContainsKey("PreviousModelPath"))
        {
            $body.Add("prevModelPath", $PreviousModelPath)
        }

        Invoke-RestMethod $EndPoint -Body $body -UseDefaultCredentials
}

function CopyTraingFiles-PhillyJob
{
    <#
    .SYNOPSIS
    Backup training files.
    .DESCRIPTION
    Copy training input and output files to a permanent directory.
    .EXAMPLE
    Copy-Files -OutputBaseDir "//analogfs/private/SemanticLabeling" -ConfigFile ivanst/resnet-idsv3a-norand/ResNet_18_256xAsp-ids-png.cntk -PreviousModelPath sys/jobs/application_1476768521734_0083/models -OutputModelFolder anlgvc\sys\jobs\application_1476768521734_0275 -OutputLogFolder anlgvc_scratch\sys\jobs\application_1476768521734_0275 -PhysicalCluster rr1 -VirtualCluster anlgvc
    .PARAMETER ConfigFile
    Specifies the relative path and filename to starting from after your VC subdirectory to your configuration file.
    For example, if your VC is anlgvc, and in anlgvc_scratch you have your own subdirectory user, and in that user
    directory, you have configuration file, train.config,
    \\storage.rr1.philly.selfhost.corp.microsoft.com\anlgvc_scratch\user\train.config
    then use: user/train.config.
    .PARAMETER PreviousModelPath
    Specifies the previous model path which will be used to continue the training.
    It is optional.
    E.g. sys/jobs/application_1476768521734_0083/models
    .PARAMETER OutputModelFolder
    Specifies the CNTK build output path of model files.
    .PARAMETER OutputLogFolder
    Specifies the CNTK build output path of log related files.
    .PARAMETER PhysicalCluster
    Physical cluster on Philly, e.g. gcr, rr1
    .PARAMETER VirtualCluster
    Virtual cluster on Philly, e.g. anlgvc
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $True)]
        [ValidateNotNullOrEmpty()]
        [string]$OutputBaseDir,

        [Parameter(Mandatory = $True)]
        [ValidateNotNullOrEmpty()]
        [string]$ConfigFile,

        [Parameter(Mandatory = $False)]
        [string]$PreviousModelPath,

        [Parameter(Mandatory = $True)]
        [ValidateNotNullOrEmpty()]
        [string]$OutputModelFolder,

        [Parameter(Mandatory = $True)]
        [ValidateNotNullOrEmpty()]
        [string]$OutputLogFolder,

        [Parameter(Mandatory = $False)]
        [ValidateNotNullOrEmpty()]
        [ValidateSet("gcr", "rr1")]
        [string]$PhysicalCluster = "rr1",

        [Parameter(Mandatory = $False)]
        [ValidateNotNullOrEmpty()]
        [ValidateSet("anlgvc")]
        [string]$VirtualCluster = "anlgvc"
        )

    $DataBaseDir = Join-Path \\storage.$PhysicalCluster.philly.selfhost.corp.microsoft.com $VirtualCluster
    $ConfigBaseDir = Join-Path \\storage.$PhysicalCluster.philly.selfhost.corp.microsoft.com $VirtualCluster"_scratch"

    if (-not(Test-Path $OutputBaseDir))
    {
        New-Item -ItemType Directory $OutputBaseDir -Force
    }
    # copy config files
    $ConfigBasePath = split-path -parent $ConfigFile
    $ConfigFilePath = Join-Path $ConfigBaseDir $ConfigBasePath
    if (-not(Test-Path $ConfigFilePath))
    {
        Write-Error("$($ConfigFilePath.ToString()) does not exist!")
        return
    }
    $OutputConfigPath = Join-Path $OutputBaseDir "config"
    Write-Host("Copying $($ConfigFilePath.ToString()) to $($OutputConfigPath.ToString())")
    Copy-Item $ConfigFilePath $OutputConfigPath -Force -Recurse

    # copy previous model files
    $PreviousModelPath = Join-Path $DataBaseDir $PreviousModelPath
    $OutputPrevModelPath = Join-Path $OutputBaseDir "previousModels"

    if (-not(Test-Path $OutputPrevModelPath))
    {
        New-Item -ItemType Directory -Path $OutputPrevModelPath -Force
    }
    $PreviousModelFiles = Get-ChildItem $PreviousModelPath\*
    foreach ($File in $PreviousModelFiles)
    {
        $FileName = $File.Name
        $tmp = 0
        # only copy the .ckp .Eval file and the final model
        if (-not([int]::TryParse($FileName.substring($FileName.LastIndexOf(".") + 1), [ref]$tmp)) -or $FileName.Contains(".ckp") -or $FileName.Contains(".Eval"))
        {
            Copy-Item $File $OutputPrevModelPath -Force -Recurse
            Write-Host("Copying $($File.ToString()) to $($OutputPrevModelPath.ToString())")
        }
    }

    # copy output model files
    $OutputModelPath = Join-Path $OutputBaseDir "model"
    if (-not(Test-Path $OutputModelPath))
    {
        New-Item -ItemType Directory -Path $OutputModelPath -Force
    }
    $OutputModelFolder= Join-Path $OutputModelFolder "models"
    $OutputModelFiles = Get-ChildItem $OutputModelFolder\*
    foreach ($File in $OutputModelFiles)
    {
        $FileName = $File.Name
        $tmp = 0
        # only copy the .ckp .Eval file and the final model
        if (-not([int]::TryParse($FileName.substring($FileName.LastIndexOf(".") + 1), [ref]$tmp)) -or $FileName.Contains(".ckp") -or $FileName.Contains(".Eval"))
        {
            Copy-Item $File $OutputModelPath -Force -Recurse
            Write-Host("Copying $($File.ToString()) to $($OutputModelPath.ToString())")
        }
    }

    # copy log related files
    if (-not(Test-Path $OutputLogFolder))
    {
        Write-Error("$($OutputLogFolder.ToString()) does not exist!")
        return
    }
    $OutputLogPath = Join-Path $OutputBaseDir "log"
    Write-Host("Copying $($OutputLogFolder.ToString()) to $($OutputLogPath.ToString())")
    Copy-Item $OutputLogFolder $OutputLogPath -Force -Recurse
}