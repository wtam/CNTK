# .SYNOPSIS
# Copies the part of a given captured data drop that will be packed into IDS format.
# .DESCRIPTION
# Copies the part of a given captured data drop that will be packed into IDS format from data drop location to IDS packing location.
# Folder structure is slightly rearranged in the process (see parameter descriptions for details).
# .EXAMPLE
# Copy-Originals -SourceDir "\\analogfs\PRIVATE\SemanticLabeling\BeautifulData2" -DestDir "\\hohonu1\DataSets\Original\Capture-70k"
# .PARAMETER SourceDir
# Folder containing full data drop, satisfying the following:
#   - All subfolders of this folder are expected to correspond to clips.
#   - Images are expected in folders $SourceDir\<clip ID>\SLTrainingTestingData\IrisDataset.
#   - Pose files are expected to have the form $SourceDir\<clip ID>\camera_poses.txt.
# .PARAMETER DestDir
# Output folder that will contain data to be packed into IDS format. An exception is thrown if the folder already exists.
# Both images are pose files will be copied under $DestDir\<clip ID>.
[CmdletBinding()]
Param (
    [Parameter(Mandatory=$True, Position = 1)]
    [ValidateNotNullOrEmpty()]
    [string]$SourceDir,

    [Parameter(Mandatory=$True, Position = 2)]
    [ValidateNotNullOrEmpty()]
    [string]$DestDir
    )

$imageSubdirRelativePath = Join-Path "SLTrainingTestingData" "IrisDataset"
$poseFileRelativePath = "camera_poses.txt"

New-Item -ItemType Directory -Path "$DestDir" | Out-Null
Get-ChildItem -Directory -Path "$SourceDir" |
% {
    $clipDestDir = Join-Path $DestDir $_
    Copy-Item (Join-Path (Join-Path $SourceDir $_) $imageSubdirRelativePath) $clipDestDir -Recurse
    Copy-Item (Join-Path (Join-Path $SourceDir $_) $poseFileRelativePath) $clipDestDir
}