<#
.SYNOPSIS
Creates IDS list file.
.DESCRIPTION
Creates IDS list file given captured data folders, and a list of clips.
.EXAMPLE
Create-ListFile -DataDir ".\Original\70k" -PoseDir ".\IDS\v3\70k\Poses" -ClipListFile ".\IDS\v3\70k\val-clips.txt" -IDSListFile ".\IDS\v3\70k\val-list.txt"
.PARAMETER DataDir
Folder containing original data.
One subfolder per clip is expected ($DataDir\<clip ID>).
.PARAMETER PoseDir
Folder containing per-frame camera poses.
Poses can be generated using dnn.tools:scripts\dataset\Create-Poses.ps1.
One subfolder per clip is expected ($PoseDir\<clip ID>).
.PARAMETER ClipListFile
Text file containing a list of clip IDs (one per line) for which IDS list file should be
generated (e.g. list of validation set clips).
.PARAMETER IDSListFile
Output IDS list file.
#>
[CmdletBinding()]
Param (
    [Parameter(Mandatory=$True)]
    [ValidateNotNullOrEmpty()]
    [string]$DataDir,

    [Parameter(Mandatory=$True)]
    [ValidateNotNullOrEmpty()]
    [string]$PoseDir,

    [Parameter(Mandatory=$True)]
    [ValidateNotNullOrEmpty()]
    [string]$ClipListFile,

    [Parameter(Mandatory=$True)]
    [ValidateNotNullOrEmpty()]
    [string]$IDSListFile
    )

. (Join-Path $PSScriptRoot "..\common\common.ps1")

# File suffixes for relevant data channels.
$poseSuffix = ".Pose.txt"
$imageSuffixes = `
    ".AngleWithGravity.png", `
    ".HeightAboveGroundNormalized255.png", `
    ".InverseRadialDepth.png", `
    ".NormalXWorldSpace.png", `
    ".NormalYWorldSpace.png", `
    ".NormalZWorldSpace.png", `
    ".StretchedIRNormalizedByDepthAndRadial.png", `
    ".ProjectedLabel.png"

# Timestamps are expected to be of this length (with leading zeros) in input file names.
$timestampLength = 12

function GetTimestampsForSuffix([string]$dir, [string]$suffix)
{
    Get-ChildItem $dir -Filter "*$suffix" -Name |
    % { $_.ToString().Substring(0, $timestampLength) } | Sort-Object
}

function GetTimestamps([string]$clipDataDir, [string]$clipPoseDir)
{
    # Get timestamps for the pose suffix.
    $timestamps = GetTimestampsForSuffix $clipPoseDir $poseSuffix

    # Check that all other channels have exactly the same set of timestamps.
    $imageSuffixes |
    % {
        $currentTimestamps = GetTimestampsForSuffix $clipDataDir $_
        Check `
            ((Compare-Object $timestamps $currentTimestamps -Sync 0 -PassThru).Length -eq 0) `
            "Image suffixes $poseSuffix and $_ do not have the same timestamps."
    }
    return $timestamps
}

function AppendToListFile([string[]]$timestamps, [string]$clipDataDir, [string]$clipPoseDir)
{
    foreach ($timestamp in $timestamps)
    {
        foreach ($imageSuffix in $imageSuffixes)
        {
            Join-Path $clipDataDir $timestamp$imageSuffix |
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

Get-Content $ClipListFile |
% {
    $clipDataDir = Join-Path $DataDir $_
    $clipPoseDir = Join-Path $PoseDir $_
    AppendToListFile (GetTimestamps $clipDataDir $clipPoseDir) `
        $clipDataDir $clipPoseDir
}
