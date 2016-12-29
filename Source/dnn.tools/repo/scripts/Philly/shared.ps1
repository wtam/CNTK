# Include common functions.
. (Join-Path -Path "$PSScriptRoot" -ChildPath "..\common\common.ps1")

# Include Philly paths.
. (Join-Path -Path "$PSScriptRoot" -ChildPath "Get-PhillyPaths.ps1")

# Include function for getting Jenkins build from default log.
. (Join-Path -Path "$PSScriptRoot" -ChildPath "..\jenkins\JenkinsJobs.ps1")

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
    .PARAMETER RackId
    Specifies the name of Philly rack where to execute the job. E.g. s3501s3524.
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

        [Parameter(Mandatory = $False)]
        [string]$RackId="",

        [Parameter(Mandatory = $False)]
        [string]$CustomCntkDockerName,

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
            clusterId = $PhysicalCluster
            vcId = $VirtualCluster
            name = $Name
            userName = $env:USERNAME
            inputDir = (ConvertToLinux-Path -Path $DataDir)
            minGPUs = $GpuCount
            maxGPUs = $GpuCount
            configFile = (ConvertToLinux-Path -Path $ConfigFile)
            buildId = $BuildId
            isdebug = $False
            extraParams = $ExtraParams
            toolType = "cntk"
        }

        If ($PSBoundParameters.ContainsKey("PreviousModelPath") -and $PreviousModelPath)
        {
            $body.Add("prevModelPath", $PreviousModelPath)
        }

        If ($PSBoundParameters.ContainsKey("RackId") -and $RackId)
        {
            $body.Add("rackid", $RackId)
        }

        If ($PSBoundParameters.ContainsKey("CustomCntkDockerName") -and $CustomCntkDockerName)
        {
            $body.Add("tag", $CustomCntkDockerName)
        }

        Invoke-RestMethod $EndPoint -Body $body -UseDefaultCredentials
}

function SubmitFromLocal-PhillyJob
{
    <#
    .SYNOPSIS
    Submits job to Philly.
    .DESCRIPTION
    Submits job to Philly using REST API.
    .EXAMPLE
    Submit-PhillyJob -Name FCN32 -ConfigFile D:\movasi\FCN32\fcn32.cntk -DataSet BD2 -PhysicalCluster rr1 -VirtualCluster anlgvc
    .EXAMPLE
    Submit-PhillyJob -Name FCN32 -ConfigFile D:\movasi\FCN32\fcn32.cntk -DataSet BD2 -BuildId 2701 -ExtraParams "TrainFCN32=[SGD=[minibatchSize=32 learningRatesPerMB=1e-8:1e-7:3.2e-7]]"
    Extra parameters will be used for overriding corresponding fields in ConfigFile
    .PARAMETER Name
    A distinguishing name for the job, e.g. lstmTry4
    .PARAMETER ConfigFile
    CNTK main configuration file. Content of the file's parent folder will be copied to Philly scratch.
    .PARAMETER DataSet
    Data set to be used in training.
    .PARAMETER BuildId
    Specifies the CNTK build ID from Jenkins that you want Philly to use for this job. [TODO] If not specified, latest known good will be used.
    .PARAMETER RackId
    Specifies the name of Philly rack where to execute the job. E.g. s3501s3524.
    .PARAMETER ExtraParams
    Extra CNTK parameters to specialize the job. For example, tweaking the learning rate schedule outside the configuration file, e.g. TrainFCN32=[SGD=[minibatchSize=32 learningRatesPerMB=1e-8:1e-7:3.2e-7]]
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
        [ValidateSet("BD2", "BD3", "ImageNet", "PascalVOC", "Pascal-Context")]
        [string]$DataSet,

        [Parameter(Mandatory = $False)]
        [ValidateNotNullOrEmpty()]
        [string]$BuildId,

        [Parameter(Mandatory = $False)]
        [string]$RackId="",

        [Parameter(Mandatory = $False)]
        [string]$CustomCntkDockerName,

        [Parameter(Mandatory = $False)]
        [int]$GpuCount=1,

        [Parameter(Mandatory = $False)]
        [string]$ExtraParams="",

        [Parameter(Mandatory = $False)]
        [string]$PreviousModelPath,

        [Parameter(Mandatory = $False)]
        [ValidateNotNullOrEmpty()]
        [ValidateSet("gcr", "rr1")]
        [string]$PhysicalCluster = "rr1",

        [Parameter(Mandatory = $False)]
        [ValidateNotNullOrEmpty()]
        [ValidateSet("anlgvc")]
        [string]$VirtualCluster = "anlgvc"
        )

    [int]$JenkinsBuild = $null
    If ($PSBoundParameters.ContainsKey("BuildId"))
    {
        $JenkinsBuild = $BuildId
    }
    Else
    {
        # If build ID is not provided, get build ID of the latest known good daily build.
        $JenkinsBuild = Get-LKGJenkinsBuildId
    }
    Check `
        -Condition (!!$JenkinsBuild) `
        -Message "Jenkins build ID not provided. Latest daily build unsuccessful."

    # Copy config folder to Philly scratch.
    $ConfigFolder = Split-Path $ConfigFile
    $PhillyConfigFile = CopyToPhilly-JobConfigurationFiles `
                            -MainConfigFile $ConfigFile `
                            -PhysicalCluster $PhysicalCluster `
                            -VirtualCluster $VirtualCluster `
                            -JobName $Name
    Write-Host -ForegroundColor DarkGreen "Using configuration file $PhillyConfigFile."

    # Location of data on Philly HDFS.
    $InputDirectory = Get-PhillyDataSet -DataSet $DataSet
    Write-Host -ForegroundColor DarkGreen "Using data folder $InputDirectory."

    # Submit job.
    Submit-PhillyJob `
            -Name $Name `
            -ConfigFile $PhillyConfigFile `
            -DataDir $InputDirectory `
            -BuildId $JenkinsBuild `
            -RackId $RackId `
            -CustomCntkDockerName $CustomCntkDockerName `
            -GpuCount $GpuCount `
            -ExtraParams $ExtraParams `
            -PreviousModelPath $PreviousModelPath `
            -PhysicalCluster $PhysicalCluster `
            -VirtualCluster $VirtualCluster
}

function CopyToPhilly-JobConfigurationFiles
{
    <#
    .SYNOPSIS
    Copies content of configuration folder to Philly scratch.
    .DESCRIPTION
    Creates configuration folder in user folder on Philly scratch and copies local configuration files to it. Returns relative path to config file on Philly scratch.
    .EXAMPLE
    CopyToPhilly-JobConfigurationFiles -MainConfigFile D:\movasi\FCN8_config -PhillyConfigFolder \\storage.rr1.philly.selfhost.corp.microsoft.com\anlgvc_scratch\movasi\FCN8_20161107_144738252
    .PARAMETER MainConfigFile
    Main CNTK configuration file. All other training configuration files should be in the same parent folder (or under sub-folder in parent folder hierarchy).
    .PARAMETER PhysicalCluster
    Physical cluster on Philly, e.g. gcr, rr1
    .PARAMETER VirtualCluster
    Virtual cluster on Philly, e.g. anlgvc
    .PARAMETER JobName
    Name of a job to be trained with specified configuration files.
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $True)]
        [ValidateNotNullOrEmpty()]
        [string]$MainConfigFile,

        [Parameter(Mandatory = $False)]
        [ValidateNotNullOrEmpty()]
        [ValidateSet("gcr", "rr1")]
        [string]$PhysicalCluster = "rr1",

        [Parameter(Mandatory = $False)]
        [ValidateNotNullOrEmpty()]
        [ValidateSet("anlgvc")]
        [string]$VirtualCluster = "anlgvc",

        [Parameter(Mandatory = $False)]
        [string]$JobName
        )

    Check `
        -Condition (Test-Path $MainConfigFile -pathType leaf) `
        -Message "$MainConfigFile doesn't exist or is not a file."

    $ConfigFolder = Split-Path $MainConfigFile
    $ConfigFileName = Split-Path -Leaf $MainConfigFile

    # Get configuration folder on Philly scratch.
    $PhillyConfigFolder = Get-NewPhillyScratchConfigFolder `
                            -PhysicalCluster $PhysicalCluster `
                            -VirtualCluster $VirtualCluster `
                            -JobName $JobName

    Write-Host `
        -ForegroundColor DarkYellow `
        "Copying configuration files from $ConfigFolder to $($PhillyConfigFolder.AbsolutePath)..."

    # Get all configuration files.
    $configFileTypes = @("*.bs", "*.cntk", "*.mel", "*.ndl", "*.prototxt")
    $configFiles = Get-ChildItem `
                    -Path $ConfigFolder `
                    -Include $configFileTypes `
                    -Recurse `
                    -Force

    # Copy configuration files to Philly scratch.
    ForEach ($configFile in $configFiles)
    {
        $sourceFile = $configFile.FullName
        $destinationFile = Join-Path $PhillyConfigFolder.AbsolutePath ($sourceFile.Substring($ConfigFolder.Length))
        $destinationFolder = Split-Path $destinationFile
        EnsureExists-Item -Path $destinationFolder -ItemType Directory
        $sourceFileName = Split-Path -Leaf $sourceFile
        Write-Host `
            "Copying: $sourceFileName" `
            -ForegroundColor DarkGreen
        Copy-Item `
            -Path $sourceFile `
            -Destination $destinationFile `
            -Force
    }

    # Check whether shared.bs file is used in configuration file and ensure it's copied to Philly.
    $SharedBsFileName = "shared.bs"
    $ConfigFileContent = Get-Content -Path $MainConfigFile
    If (!!($ConfigFileContent -cmatch "include .+$SharedBsFileName"))
    {
        # List of possible locations of shared.bs file relative to configuration folder.
        $SharedBsRelativePaths = @($SharedBsFileName, (Join-Path ..\common $SharedBsFileName))
        $IsSharedBsCopied = $False
        ForEach ($SharedBsRelativePath in $SharedBsRelativePaths)
        {
            $SharedBsSource = Join-Path $ConfigFolder $SharedBsRelativePath
            If (Test-Path -Path $SharedBsSource)
            {
                $SharedBsDestination = Join-Path $PhillyConfigFolder.AbsolutePath $SharedBsRelativePath
                If (!(Test-Path -Path $SharedBsDestination))
                {
                    EnsureExists-Item -Path (Split-Path $SharedBsDestination) -ItemType Directory
                    Write-Host `
                        "Copying: $SharedBsFileName" `
                        -ForegroundColor DarkGreen
                    Copy-Item `
                        -Path $SharedBsSource `
                        -Destination $SharedBsDestination `
                        -Force
                }
                # Else: already copied to Philly scratch.
                $IsSharedBsCopied = $True
                Break
            }
        }

        If (!$IsSharedBsCopied)
        {
            Write-Warning -Message "$SharedBsFileName included in configuration file, but it cannot be found!"
        }
    }

    Write-Host `
        -ForegroundColor DarkGreen `
        "Copied configuration files from $ConfigFolder to $($PhillyConfigFolder.AbsolutePath)."

    $ConfigFileRelativePath = Join-Path $PhillyConfigFolder.RelativePath $ConfigFileName
    return ConvertToLinux-Path -Path $ConfigFileRelativePath
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

    $DataBaseDir = Get-PhillyHdfsShare `
                    -PhysicalCluster $PhysicalCluster `
                    -VirtualCluster $VirtualCluster
    $ConfigBaseDir = Get-PhillyScratch `
                        -PhysicalCluster $PhysicalCluster `
                        -VirtualCluster $VirtualCluster

    EnsureExists-Item -Path $OutputBaseDir -ItemType Directory

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

    EnsureExists-Item -Path $OutputPrevModelPath -ItemType Directory
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
    EnsureExists-Item -Path $OutputModelPath -ItemType Directory
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