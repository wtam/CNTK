# .SYNOPSIS
# Copies the part of a given captured data drop that will be packed into IDS format.
# .DESCRIPTION
# Copies the part of a given captured data drop that will be packed into IDS format from data drop location to IDS packing location.
# Folder structure is preserved in the process (see parameter descriptions for structure details).
# Partition files (train/test/val) are also copied from partition directory.
# .EXAMPLE
# Copy-Originals -ClipsSourceDir "\\analogfs\PRIVATE\SemanticLabeling\BeautifulData4" -PartitionsSourceDir "\\analogfs\PRIVATE\SemanticLabeling\BD4_DataSets" -Camera "Barabretto" -DestDir "\\hohonu1\DataSets\Original\Capture-70k"
# .PARAMETER ClipsSourceDir
# Folder containing full data drop, satisfying the following:
#   - All subfolders of this folder are expected to correspond to clips.
#   - Images are expected in folders $ClipsSourceDir\<clip ID>\SLTrainingTestingData\IrisDataset.
#   - Pose files are expected to have the form $ClipsSourceDir\<clip ID>\camera_poses.txt.
#   - Timestamp files are expected to have the form $ClipsSourceDir\<clip ID>\SampleTS_filtered.txt.txt.
# .PARAMETER PartitionsSourceDir
# Directory that contains files that define partition of clips into train/test/val sets.
# .PARAMETER Camera
# Name of the camera used to collect input images. Actual drop is expected to be at $ClipsSourceDir\$Camera and $PartitionsSourceDir\$Camera
# .PARAMETER DestDir
# Output folder that will contain data to be packed into IDS format. An exception is thrown if the folder already exists.
[CmdletBinding()]
Param (
    [Parameter(Mandatory=$True, Position = 1)]
    [ValidateNotNullOrEmpty()]
    [string]$ClipsSourceDir,

    [Parameter(Mandatory=$True, Position = 2)]
    [ValidateNotNullOrEmpty()]
    [string]$PartitionsSourceDir,

    [Parameter(Mandatory=$True, Position = 3)]
    [ValidateSet('Barabretto','Grigio')]
    [string]$Camera,

    [Parameter(Mandatory=$True, Position = 4)]
    [ValidateNotNullOrEmpty()]
    [string]$DestDir
    )

. (Join-Path -Path $PSScriptRoot -ChildPath "Common.ps1")

$robocopy = "robocopy.exe"

function CopyFileWithRoboCopy([string]$filePath, [string]$destDirPath)
{
    & $robocopy `
    (Split-Path $filePath -Parent), `
    $destDirPath, `
    (Split-Path $filePath -Leaf) | Out-Null
}

# Stop script on any error.
$ErrorActionPreference = "Stop"

New-Item -ItemType Directory -Path "$DestDir" | Out-Null

$CameraClipsSourceDir = Join-Path $ClipsSourceDir $Camera
Get-ChildItem -Directory -Path "$CameraClipsSourceDir" |
% {
    $clipDestDir = Join-Path $DestDir $_

    & $robocopy `
    (Join-Path (Join-Path $CameraClipsSourceDir $_) (GetImagesDirName)), `
    (Join-Path $clipDestDir (GetImagesDirName)), `
    "/mt", "/e", "/xf", "*.bin" | Out-Null

    $poseFileAbsolutePath = Join-Path (Join-Path $CameraClipsSourceDir $_) (GetCameraPoseFileName)
    CopyFileWithRoboCopy $poseFileAbsolutePath $clipDestDir

    $timestampsFileAbsolutePath = Join-Path (Join-Path $CameraClipsSourceDir $_) (GetTimestampsFileName)
    CopyFileWithRoboCopy $timestampsFileAbsolutePath $clipDestDir
}

GetExistingPartitionFileTypes $PartitionsSourceDir $Camera |
% {
    CopyFileWithRoboCopy (GetPartitionFilePath $PartitionsSourceDir $Camera $_) $DestDir
}