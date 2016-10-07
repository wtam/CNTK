# .SYNOPSIS
# Transforms camera pose information into a format than can be consumed by IDS packing tool.
# .DESCRIPTION
# Transforms camera pose information into a format than can be consumed by IDS packing tool. Original data drops contain one pose file per clip. This cmdlet creates one IDS tensor file per frame.
# .EXAMPLE
# Create-Poses -OriginalDataDir "\\hohonu1\DataSets\Original\70k" -OutputPosesDir "\\hohonu1\DataSets\IDS\v3\Capture-70k-original-png\Poses"
# .PARAMETER OriginalDataDir
# Root folder containing original data for the whole dataset.
# .PARAMETER OutputPosesDir
# Output folder that will contain pose files for the whole dataset.
[CmdletBinding()]
Param (
    [Parameter(Mandatory=$True, Position = 1)]
    [ValidateNotNullOrEmpty()]
    [string]$OriginalDataDir,

    [Parameter(Mandatory=$True, Position = 2)]
    [ValidateNotNullOrEmpty()]
    [string]$OutputPosesDir
    )

# Include common functions.
. (Join-Path -Path $PSScriptRoot -ChildPath "..\common\common.ps1")

# Text file containing poses for one clip is expected to be $OriginalDataDir\<clip ID>\$poseFile.
# It is expected to encode a single pose in $linesPerPose lines for text:
# <0-based index within the file>
# <timestamp (a single integer)>
# <translation rotation matrix (3 rows, with 4 floating point numbers per row)>
# <blank row>
$poseFile = "camera_poses.txt"
$linesPerPose = 6

# Output pose files are $OutputPosesDir\<clip ID>\<timestamp>$poseSuffix.
$poseSuffix = ".Pose.txt"

# Parameters of IDS tensor that encodes one pose.
$idsTensorTypeDouble = 2
$idsTensorHeight = 3
$idsTensorWidth = 4
$idsTensorChannels = 1

# Input pose file typically contains poses for many timestamps. We only want to save
# those for which all other data channels exist. For this reason we need to look at
# other data files, whose expected paths are $OriginalDataDir\<clip ID>\<timestamp><suffix>.
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
# They will have the same format in output file names. However, inside individual pose
# files they don't need to have a fixed field length, and they don't need to be zero-padded.
$timestampFieldLength = 12

# Get timestamps for given clip directory and given channel-specific suffix.
function GetTimestampsForSuffix([string]$dir, [string]$suffix)
{
    Get-ChildItem $dir -Filter "*$suffix" -Name |
    % { $_.ToString().Substring(0, $timestampFieldLength) }
}

# Get timestamps for given clip directory. Exception is thrown if all data channels
# (except poses) do not exist for the same set of timestamps.
function GetTimestamps([string]$dir)
{
    # Get timestamps for first suffix.
    $timestamps = GetTimestampsForSuffix $dir $imageSuffixes[0]

    # Check that these timestamps exist for all other suffixes.
    foreach ($suffix in ($imageSuffixes | Select -Skip 1))
    {
        Check `
            ((GetTimestampsForSuffix $dir $suffix | Where-Object { -not ($timestamps -contains $_) }).Length -eq 0) `
            "Data channels with suffixes $suffix and $($imageSuffixes[0]) have different timestamps."
    }
    return $timestamps
}

# Parse given pose file (for one clip), split it into multiple IDS tensor files
# (each encoding one pose), and put all the resulting files into given dir.
# A pose file is saved only if its timestamp appears in given timestamp array.
# Furthremore, it is verified that all timestamps in the array are successfully
# matched to a pose file.
function CreatePoseFilesForClip([string]$inFile, [string[]]$timestamps, [string]$outDir)
{
    $timestampFormatString = "{0:D$timestampFieldLength}"

    New-Item -ItemType Directory -Force $outDir | Out-Null
    $lines = Get-Content $inFile
    $timestampsWithPose = @()
    for ($i = 0; $i -lt $lines.Length; $i += $linesPerPose)
    {
        $timestamp = ($timestampFormatString -f [int]$lines[$i + 1])
        $rotation = "$($lines[$i + 2]) $($lines[$i + 3]) $($lines[$i + 4])"
        if ($timestamps -contains $timestamp)
        {
            # This timestamps has all other channels, we can save its pose.
            "$idsTensorTypeDouble $idsTensorHeight $idsTensorWidth $idsTensorChannels $rotation" |
            Out-File -Encoding ascii (Join-Path $outDir $timestamp$poseSuffix)
            $timestampsWithPose += $timestamp
        }
    }

    # Check that all input timestamps are matched with a pose.
    Check `
        (($timestamps | Where-Object { -not ($timestampsWithPose -contains $_) }).Length -eq 0) `
        "$inFile does not contain all required timestamps."
}

# Create a local dir that will hold pose files for the whole dataset.
New-Item -ItemType Directory -Force $OutputPosesDir | Out-Null

Get-ChildItem $OriginalDataDir -Name | % { $_.ToString() } |
% {
    # Create pose files for current clip.
    CreatePoseFilesForClip `
        (Join-Path (Join-Path $OriginalDataDir $_) $poseFile) `
        (GetTimestamps (Join-Path $OriginalDataDir $_)) `
        (Join-Path $OutputPosesDir $_)
}