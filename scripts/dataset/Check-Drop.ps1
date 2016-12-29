# .SYNOPSIS
# Performs the validation of the new drop of images.
# .DESCRIPTION
# Checks if new drop of images is compliant with defined format.
# .EXAMPLE
# Check-Drop -ClipsDir "\\analogfs\PRIVATE\SemanticLabeling\BeautifulData4" -PartitionsDir "\\analogfs\PRIVATE\SemanticLabeling\BD4_DataSets" -Camera "Barabretto"
# .PARAMETER ClipsDir
# Folder containing clips in data drop.
# .PARAMETER PartitionsDir
# Folder containing files that define clips partitions (train/val/test).
# .PARAMETER Camera
# Name of the camera used to collect input images. Actual drop is expected to be at $ClipsDir\$Camera and $PartitionsDir\$Camera
[CmdletBinding()]
Param (
    [Parameter(Mandatory=$True, Position = 1)]
    [ValidateNotNullOrEmpty()]
    [string]$ClipsDir,

    [Parameter(Mandatory=$True, Position = 2)]
    [ValidateNotNullOrEmpty()]
    [string]$PartitionsDir,

    [Parameter(Mandatory=$True, Position = 3)]
    [ValidateSet('Barabretto','Grigio')]
    [string]$Camera
    )

. (Join-Path -Path $PSScriptRoot -ChildPath "..\common\common.ps1")
. (Join-Path -Path $PSScriptRoot -ChildPath "Common.ps1")

############### CHECKERS ######################################################

# Checks if partition files are present.
function Check-PartitionFiles([string]$partitionsDir, [string]$camera)
{
    [int]$filesCount = 0;
    foreach ($partitionFileType in (GetExistingPartitionFileTypes $partitionsDir $camera))
    {
        $partitionFilePath = GetPartitionFilePath $partitionsDir $camera $partitionFileType
        If (Test-Path $partitionFilePath)
        {
            $filesCount++
        }
        Else
        {
            LogWarning "Expected partition file $partitionFilePath does not exist at $partitionsDir."
        }
    }
    Check ($filesCount -gt 0) "All partition files not present."
}

# Checks that all clips from partition files are present.
function Check-ClipFolders([string]$clipsDir, [string]$partitionsDir, [string]$camera)
{
    $allClips = GetAllExistingClips $partitionsDir $camera
    foreach ($clip in $allClips)
    {
        $clipPath = GetClipAbsolutePath $clipsDir $camera $clip
        Check (Test-Path $clipPath) "Clip $clip not present at $clipsDir."
    }
}

# Checks that no duplicates are present in union of all clips.
function Check-NoDuplicateClips([string[]]$allClips)
{
    [string[]]$allUniqueClips = $allClips | Sort-Object | Get-Unique
    Check ($allClips.Length -eq $allUniqueClips.Length) "Duplicate clips found."
}

# Checks that all expected clip related files are present.
function Check-ClipFolderStructure([string]$clipDir)
{
    foreach($item in $ClipItems)
    {
        $itemPath = Join-Path $clipDir $item
        Check (Test-Path $itemPath) "Item $item not present at $clipDir."
    }
}

# Checks that all channels for all timestamps in clip are present.
function Check-ClipTimestampChannels([string]$clipDir)
{
    $imagesDirPath = Join-Path $clipDir (GetImagesDirName)

    # Take clip timestamps.
    $timestamps = GetTimestampsForClip $clipDir
    # Go over all timestamps.
    foreach($timestamp in $timestamps)
    {
        # Go over all channels.
        foreach ($imageSuffix in $ImageSuffixes)
        {
            # Check that channel is present.
            $imagePath = Join-Path $imagesDirPath $timestamp$imageSuffix
            Check (Test-Path $imagePath) "Image $imagePath is missing."
        }
    }
}

# Checks that all timestamps for clip have correct pose present.
function Check-ClipPoses([string]$clipDir)
{
    $timestampToPoseMap = GetCameraPosesForClip $clipDir
    $timestamps = GetTimestampsForClip $clipDir

    # Check that each timestamp has its pose.
    foreach ($timestamp in $timestamps)
    {
        Check $timestampToPoseMap.ContainsKey($timestamp) "Pose not found for timestamp $timestamp in clip $clipDir."
    }
}

############### MAIN SCRIPT ###################################################

# Check that we have required partition files.
Check-PartitionFiles $PartitionsDir $Camera

# Check that drop folder contains all clips from partition files.
Check-ClipFolders $ClipsDir $PartitionsDir $Camera

# Go over clip folders and check their consistency one by one.
$allClips = GetAllExistingClips $PartitionsDir $Camera
# Check we have no duplicate clips.
Check-NoDuplicateClips $allClips
foreach ($clip in $allClips)
{
    # Take clip full path.
    $clipDir = GetClipAbsolutePath $ClipsDir $Camera $clip
    # Check we have expected filesystem items in clip folder.
    Check-ClipFolderStructure $clipDir
    # Now check that we have expected channels for clip timestamps.
    Check-ClipTimestampChannels $clipDir
    # Check that each timestamp has correct pose file.
    Check-ClipPoses $clipDir

    LogMessage "Finished checking clip $clip. Clip is valid."
}

LogMessage "Finished checking drop. Drop is valid." $true