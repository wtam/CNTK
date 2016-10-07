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