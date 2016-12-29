<#
.SYNOPSIS
Creates IDS list file.
.DESCRIPTION
Creates IDS list file given captured data folders, and a list of clips.
.EXAMPLE
Create-ListFile -ClipsDir ".\Original\70k" -PoseDir ".\IDS\v3\70k\Poses" -PartitionDir ".\IDS\v3\70k" -Camera "Barabretto" -SetType "train" -IDSListFile ".\IDS\v3\70k\val-list.txt"
.PARAMETER ClipsDir
Folder containing clips data.
One subfolder per clip is expected ($ClipsDir\<clip ID>).
.PARAMETER PoseDir
Folder containing per-frame camera poses.
Poses can be generated using dnn.tools:scripts\dataset\Create-Poses.ps1.
One subfolder per clip is expected ($PoseDir\<clip ID>).
.PARAMETER PartitionDir
Directory which contains set partitioning files.
.PARAMETER Camera
Name of the camera used to collect input images. Actual drop is expected to be at $ClipsDir\$Camera and $PartitionsDir\$Camera
.PARAMETER SetType
Indicates type of set list file is generated for.
.PARAMETER IDSListFile
Output IDS list file.
#>
[CmdletBinding()]
Param (
    [Parameter(Mandatory=$True)]
    [ValidateNotNullOrEmpty()]
    [string]$ClipsDir,

    [Parameter(Mandatory=$True)]
    [ValidateNotNullOrEmpty()]
    [string]$PoseDir,

    [Parameter(Mandatory=$True)]
    [ValidateNotNullOrEmpty()]
    [string]$PartitionDir,

    [Parameter(Mandatory=$True)]
    [ValidateNotNullOrEmpty()]
    [string]$Camera,

    [Parameter(Mandatory=$True)]
    [ValidateSet('train','validation','test')]
    [string]$SetType,

    [Parameter(Mandatory=$True)]
    [ValidateNotNullOrEmpty()]
    [string]$IDSListFile
    )

. (Join-Path $PSScriptRoot "..\common\common.ps1")
. (Join-Path $PSScriptRoot "Common.ps1")

function AppendClipToListFile([string]$clipDataDir, [string]$clipPoseDir)
{
    [string[]]$timestamps = GetTimestampsForClip $clipDataDir
    foreach ($timestamp in $timestamps)
    {
        foreach ($imageSuffix in $imageSuffixes)
        {
            Join-Path (Join-Path $clipDataDir (GetImagesDirName)) $timestamp$imageSuffix |
                Out-File -Encoding ascii -Append $IDSListFile
        }
        Join-Path $clipPoseDir $timestamp$poseSuffix |
            Out-File -Encoding ascii -Append $IDSListFile
    }
}

# Stop script on any error.
$ErrorActionPreference = "Stop"

# Create IDS list file (results in an error if the file exists).
New-Item -ItemType File $IDSListFile | Out-Null

GetClips $PartitionDir $Camera $SetType |
% {
    $clipDataDir = GetClipAbsolutePath $ClipsDir $Camera $_
    $clipPoseDir = Join-Path $PoseDir $_
    AppendClipToListFile $clipDataDir $clipPoseDir
}
