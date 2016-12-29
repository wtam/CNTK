# .SYNOPSIS
# Transforms camera pose information into a format than can be consumed by IDS packing tool.
# .DESCRIPTION
# Transforms camera pose information into a format than can be consumed by IDS packing tool. Original data drops contain one pose file per clip. This cmdlet creates one IDS tensor file per frame.
# .EXAMPLE
# Create-Poses -ClipsDir "\\analogfs\PRIVATE\SemanticLabeling\BeautifulData4" -PartitionsDir "\\analogfs\PRIVATE\SemanticLabeling\BD4_DataSets" -Camera "Barabretto" -OutputPosesDir "\\hohonu1\DataSets\IDS\v3\Capture-70k-original-png\Poses"
# .PARAMETER ClipsDir
# Root folder containing original data for the whole dataset.
# .PARAMETER PartitionsDir
# Root folder containing partition for the whole dataset.
# .PARAMETER Camera
# Name of the camera used to collect input images. Actual drop is expected to be at $ClipsDir\$Camera and $PartitionsDir\$Camera
# .PARAMETER OutputPosesDir
# Output folder that will contain pose files for the whole dataset.
[CmdletBinding()]
Param (
    [Parameter(Mandatory=$True, Position = 1)]
    [ValidateNotNullOrEmpty()]
    [string]$ClipsDir,

    [Parameter(Mandatory=$True, Position = 2)]
    [ValidateNotNullOrEmpty()]
    [string]$PartitionsDir,

    [Parameter(Mandatory=$True, Position = 3)]
    [ValidateNotNullOrEmpty()]
    [string]$Camera,

    [Parameter(Mandatory=$True, Position = 4)]
    [ValidateNotNullOrEmpty()]
    [string]$OutputPosesDir
    )

# Include common functions.
. (Join-Path -Path $PSScriptRoot -ChildPath "..\common\common.ps1")
. (Join-Path -Path $PSScriptRoot -ChildPath "Common.ps1")

# Parameters of IDS tensor that encodes one pose.
$idsTensorTypeDouble = 2
$idsTensorHeight = 3
$idsTensorWidth = 4
$idsTensorChannels = 1

# Parse pose file for the given clip, split it into multiple IDS tensor files
# (each encoding one pose), and put all the resulting files into given dir.
# A pose file is saved for each timestamp associated with the clip.
# Furthermore, it is verified that all timestamps for the given clip are successfully
# matched to a pose file.
function CreatePoseFilesForClip([string]$clipDir, [string]$outDir)
{
    New-Item -ItemType Directory -Force $outDir | Out-Null

    $timestamps = GetTimestampsForClip $clipDir
    $timestampToPoseMap = GetCameraPosesForClip $clipDir

    foreach ($timestamp in $timestamps)
    {
        Check $timestampToPoseMap.ContainsKey($timestamp) "No pose found for timestamp $timestamp in clip $clipDir"
        $timestampPose = $timestampToPoseMap[$timestamp]
        "$idsTensorTypeDouble $idsTensorHeight $idsTensorWidth $idsTensorChannels $timestampPose" |
            Out-File -Encoding ascii (Join-Path $outDir $timestamp$PoseSuffix)
    }
}

# Create a local dir that will hold pose files for the whole dataset.
New-Item -ItemType Directory -Force $OutputPosesDir | Out-Null

(GetAllExistingClips $PartitionsDir $Camera) |
% {
    # Create pose files for current clip.
    CreatePoseFilesForClip `
        (GetClipAbsolutePath $ClipsDir $Camera $_) `
        (Join-Path $OutputPosesDir $_)
}