#
# Get Philly specific paths: data sets, scratch, share...
#

# Include common functions.
. (Join-Path -Path $PSScriptRoot -ChildPath "..\common\common.ps1")

function Get-PhillyDataSetRoot
{
    return "/hdfs/anlgvc/datasets/ids/v3"
}

function Get-PhillyDataSet
{
    <#
    .SYNOPSIS
    Gets path to data set on Philly share.
    .DESCRIPTION
    Gets the list of jobs from Philly using REST API for specified physical and virtual cluster.
    .EXAMPLE
    Get-PhillyQueryStatusBody -PhysicalCluster rr1 -VirtualCluster anlgvc
    .PARAMETER PhysicalCluster
    Physical cluster on Philly, e.g. gcr, rr1
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $True)]
        [ValidateNotNullOrEmpty()]
        [ValidateSet("BD2", "BD3", "ImageNet", "PascalVOC", "Pascal-Context")]
        [string]$DataSet
        )

    $phillyDataSetPath = Get-PhillyDataSetRoot
    If ($DataSet -eq "BD2")
    {
        $phillyDataSetPath = Join-Path $phillyDataSetPath Capture-70k-original-png
    }
    ElseIf ($DataSet -eq "BD3")
    {
        $phillyDataSetPath = Join-Path $phillyDataSetPath Capture-BD3-original-png
    }
    ElseIf ($DataSet -eq "ImageNet")
    {
        $phillyDataSetPath = Join-Path $phillyDataSetPath ImageNet-ILSVRC-2012-Original-png
    }
    ElseIf ($DataSet -eq "PascalVOC")
    {
        $phillyDataSetPath = Join-Path $phillyDataSetPath Pascal-VOC-2011-Original-png
    }
    ElseIf ($DataSet -eq "Pascal-Context")
    {
        $phillyDataSetPath = Join-Path $phillyDataSetPath Pascal-VOC-2011-Original-png
    }
    Else
    {
        Fail -Message "Unknown data set $DataSet"
    }

    return ConvertToLinux-Path -Path $phillyDataSetPath
}

function Get-PhillyHdfsShare
{
    <#
    .SYNOPSIS
    Gets path to Philly share.
    .DESCRIPTION
    Gets path to Philly HDFS share where data sets and models are stored.
    .EXAMPLE
    Get-PhillyHdfsShare -PhysicalCluster rr1 -VirtualCluster anlgvc
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
        [string]$PhysicalCluster = "rr1",

        [Parameter(Mandatory = $False)]
        [ValidateNotNullOrEmpty()]
        [ValidateSet("anlgvc")]
        [string]$VirtualCluster = "anlgvc"
        )
    return Join-Path \\storage.$PhysicalCluster.philly.selfhost.corp.microsoft.com $VirtualCluster
}

function Get-PhillyScratch
{
    <#
    .SYNOPSIS
    Gets path to Philly scratch.
    .DESCRIPTION
    Gets path to Philly scratch where training configuration files are stored.
    .EXAMPLE
    Get-PhillyScratch -PhysicalCluster rr1 -VirtualCluster anlgvc
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
        [string]$PhysicalCluster = "rr1",

        [Parameter(Mandatory = $False)]
        [ValidateNotNullOrEmpty()]
        [ValidateSet("anlgvc")]
        [string]$VirtualCluster = "anlgvc"
        )
    return Join-Path \\storage.$PhysicalCluster.philly.selfhost.corp.microsoft.com $VirtualCluster"_scratch"
}

function Get-NewPhillyScratchConfigFolder
{
    <#
    .SYNOPSIS
    Gets full path to configuration folder on Philly scratch.
    .DESCRIPTION
    Gets full path to configuration folder in user folder on Philly scratch. Also, gets relative path to the scratch folder that can be used as job submission parameter.
    .EXAMPLE
    Get-NewPhillyScratchConfigFolder -PhysicalCluster rr1 -VirtualCluster anlgvc -JobName FCN8
    Creates configuration folder \\storage.rr1.philly.selfhost.corp.microsoft.com\anlgvc_scratch\<user>\FCN8_<date_tag>,
    e.g. \\storage.rr1.philly.selfhost.corp.microsoft.com\anlgvc_scratch\movasi\FCN8_20161107_144738252
    .PARAMETER PhysicalCluster
    Physical cluster on Philly, e.g. gcr, rr1
    .PARAMETER VirtualCluster
    Virtual cluster on Philly, e.g. anlgvc
    .PARAMETER JobName
    Configuration folder name will start with JobName_ if parameter is provided.
    #>
    [CmdletBinding()]
    param(
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
    $scratch = Get-PhillyScratch `
                -PhysicalCluster $PhysicalCluster `
                -VirtualCluster $VirtualCluster
    $date = Get-Date
    $dateTag = "{0:D4}{1:D2}{2:D2}_{3:D2}{4:D2}{5:D2}{6:D3}" -f `
                    $date.Year,$date.Month,$date.Day,$date.Hour,$date.Minute,$date.Second,$date.Millisecond
    $jobFolder = $dateTag
    If ($PSBoundParameters.ContainsKey("JobName"))
    {
        $jobFolder = "$($JobName)_$($jobFolder)"
    }
    $relativePath = Join-Path configs $env:USERNAME
    $relativePath = Join-Path $relativePath $jobFolder
    $relativePath = Join-Path $relativePath config
    $configFolder = @{
        RelativePath = $relativePath
        AbsolutePath = Join-Path $scratch $relativePath
    }
    return $configFolder
}