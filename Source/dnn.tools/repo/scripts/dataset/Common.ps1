. (Join-Path -Path $PSScriptRoot -ChildPath "..\common\common.ps1")

############### GLOBAL DROP DESCRIPTORS #######################################

# Defines type of camera sensor data is taken from.
[string[]]$CameraType = @(
    "Barabretto",
    "Grigio"
)

# Array of filesystem items expected in the clip.
[string[]]$ClipItems = @(
    "camera_poses.txt",
    "SampleTS_filtered.txt",
    "SLTrainingTestingData\IrisDataset"
)

# PNG suffixes for example channels.
[string[]]$ImageSuffixes = @(
    ".AngleWithGravity.png",
    ".HeightAboveGroundNormalized255.png",
    ".InverseRadialDepth.png",
    ".NormalXWorldSpace.png",
    ".NormalYWorldSpace.png",
    ".NormalZWorldSpace.png",
    ".StretchedIRNormalizedByDepthAndRadial.png",
    ".GTv1.png",
    ".GTv2.png",
    ".GTv3.png"
)

# Suffixes of partition files.
[System.Collections.Hashtable]$PartitionFilesSuffixes = @{
    "train" = "_training.txt";
    "test" = "_testing.txt";
    "validation" = "_validation.txt"
}

# Partition file types.
[string[]]$PartitionFilesTypes = @(
    "train",
    "test",
    "validation"
)

# Pose file suffix.
[string]$PoseSuffix = ".Pose.txt"

############### FILESYSTEM ITEM NAMES ACCESSORS ###############################

# Returns file name for camera poses.
function GetCameraPoseFileName()
{
    return $ClipItems[0]
}

# Returns file name for clip timestamps.
function GetTimestampsFileName()
{
    return $ClipItems[1]
}

# Returns directory name that contains images.
function GetImagesDirName()
{
    return $ClipItems[2]
}

############### PARTITION FILES GETTERS #######################################

function GetPartitionFilePath([string]$partitionsDir, [string]$camera, [string]$partitionFileType)
{
    Check ($CameraType.Contains($camera)) "Unknown camera type $camera"

    $partitionFileSuffix = $PartitionFilesSuffixes[$partitionFileType]
    $cameraPartitionsDir = Join-Path $partitionsDir $camera
    $partitionFileName = $camera + $partitionFileSuffix
    $partitionFilePath = Join-Path $cameraPartitionsDir $partitionFileName

    return $partitionFilePath
}

function GetExistingPartitionFileTypes([string]$partitionsDir, [string]$camera)
{
    Check ($CameraType.Contains($camera)) "Unknown camera type $camera"

    [string[]] $partitionFileTypes = @()

    foreach ($partitionFilesType in $PartitionFilesTypes)
    {
        $partitionFilePath = GetPartitionFilePath $partitionsDir $camera $partitionFilesType
        If (Test-Path $partitionFilePath)
        {
            $partitionFileTypes += ($partitionFilesType)
        }
    }

    Check ($partitionFileTypes.Length -gt 0) "No partition files for camera $camera present in dir $cameraPartitionsDir"
    return $partitionFileTypes
}

############### CLIP GETTERS ##################################################

# Auxiliary method that returns array of clips given partition file.
function GetClips([string]$partitionsDir, [string]$camera, [string]$partitionFileType)
{
    $partitionFilePath = GetPartitionFilePath $partitionsDir $camera $partitionFileType
    Check (Test-Path $partitionFilePath) "Required partition file $partitionFilePath does not exist."
    $clipsRead = Get-Content $partitionFilePath
    [string[]]$clips = @()
    foreach($clip in $clipsRead)
    {
        $clips += $clip.ToString()
    }
    return $clips
}

# Returns all clips.
function GetAllExistingClips([string]$partitionsDir, [string]$camera)
{
    [string[]] $allClips = @()

    GetExistingPartitionFileTypes $partitionsDir $camera | % {
        $allClips += GetClips $partitionsDir $camera $_
    }

    Check ($allClips.Length -gt 0) "Could not find any clips."
    return $allClips
}

# Given the clip name returns its absolute path.
function GetClipAbsolutePath([string]$clipsDir, [string]$camera, [string]$clip)
{
    return (Join-Path (Join-Path $clipsDir $camera) $clip)
}

############### TIMESTAMP GETTERS #############################################

# Given timestamp returns formatted timestamp (padded with leading zeros).
function GetFormattedTimestamp([string]$timestamp)
{
    $timestampFieldLength = 12
    $timestampFormatString = "{0:D$timestampFieldLength}"

    $formattedTimestamp = $timestampFormatString -f [int]$timestamp
    return $formattedTimestamp
}

# Returns array of all timestamps for the clip.
function GetTimestampsForClip([string]$clipDir)
{
    $timestampsFilePath = Join-Path $clipDir (GetTimestampsFileName)
    $unformattedTimestamps = Get-Content $timestampsFilePath
    return $unformattedTimestamps | % {
        GetFormattedTimestamp $_
    }
}

############### POSES GETTERS #################################################

# Returns timestamp to pose map for the given clip.
# Camera pose file expected to encode a single pose in $linesPerPose lines for text:
# <0-based index within the file>
# <timestamp (a single integer)>
# <translation rotation matrix (3 rows, with 4 floating point numbers per row)>
# <blank row>
function GetCameraPosesForClip([string]$clipDir)
{
    $timestampToPoseMap = @{}

    # Parse camera pose file and fill in the map.
    $linesPerPose = 6
    $posesFilePath = Join-Path $clipDir (GetCameraPoseFileName $clipDir)
    $lines = Get-Content $posesFilePath
    for ($i = 0; $i -lt $lines.Length; $i += $linesPerPose)
    {
        [string]$timestamp = GetFormattedTimestamp $lines[$i + 1]
        [string]$poseString = "$($lines[$i + 2]) $($lines[$i + 3]) $($lines[$i + 4])"
        [string[]]$pose = $poseString.split()
        Check ($pose.Length -eq 12) "Pose for timestamp $timestamp has invalid number of coefficients."
        $timestampToPoseMap.Add($timestamp, $pose)
    }

    return $timestampToPoseMap
}