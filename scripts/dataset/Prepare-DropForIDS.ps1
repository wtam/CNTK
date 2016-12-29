<#
.SYNOPSIS
Creates IDS config files for new drop.
.DESCRIPTION
Master script that calls all other scripts to generate IDS generation config files for the new drop of images.
.EXAMPLE
Prepare-DropForIDS -ClipsOriginalDir "\\analogfs\PRIVATE\SemanticLabeling\BeautifulData5" -PartitionsOriginalDir "\\analogfs\PRIVATE\SemanticLabeling\BD5_DataSets" -DropName "BD5" -OutputDir "\\hohonu1\DataSets\IDS\v3"
.EXAMPLE
Prepare-DropForIDS -ClipsOriginalDir "\\analogfs\PRIVATE\SemanticLabeling\BeautifulData5" -PartitionsOriginalDir "\\analogfs\PRIVATE\SemanticLabeling\BD5_DataSets" -DropName "BD5" -OutputDir "\\hohonu1\DataSets\IDS\v3" -Cameras "Barabretto" -CopyDir "\\hohonu1\DataSets\Original"
.PARAMETER ClipsOriginalDir
Path to directory where images data is stored.
.PARAMETER PartitionsOriginalDir
Path to directory partition files (test/train/val distribution) is stored.
.PARAMETER DropName
Name of the drop ti use.
.PARAMETER OutputDir
Path to output directory where configuration files are to be generated.
.PARAMETER Cameras
Optional parameter which indicates for which camera(s) IDS files will be generated.
.PARAMETER CopyDir
Optional parameter which indicates where to copy clips and partition files.
#>
[CmdletBinding()]
Param (
    [Parameter(Mandatory=$True, Position = 1)]
    [ValidateNotNullOrEmpty()]
    [string]$ClipsOriginalDir,

    [Parameter(Mandatory=$True, Position = 2)]
    [ValidateNotNullOrEmpty()]
    [string]$PartitionsOriginalDir,

    [Parameter(Mandatory=$True, Position = 3)]
    [ValidateNotNullOrEmpty()]
    [string]$DropName,

    [Parameter(Mandatory=$True, Position = 4)]
    [ValidateNotNullOrEmpty()]
    [string]$OutputDir,

    [Parameter(Mandatory=$False, Position = 5)]
    [ValidateSet('Barabretto','Grigio')]
    [string[]]$Cameras = @("Barabretto", "Grigio"),

    [Parameter(Mandatory=$False, Position = 6)]
    [ValidateNotNullOrEmpty()]
    [string]$CopyDir
    )

. (Join-Path $PSScriptRoot "..\common\common.ps1")
. (Join-Path $PSScriptRoot "Common.ps1")

$posesDirName = "Poses"
$listFilesSuffix  = "-ids-list.txt"

foreach ($camera in $Cameras)
{
    LogMessage "Creating dataset $DropName for camera $camera." $True

    # First copy drop files if necessary.
    $clipsDir = $ClipsOriginalDir
    $partitionsDir = $PartitionsOriginalDir
    If (($CopyDir -ne $Null) -and ($CopyDir -ne ""))
    {
        $cameraCopyDir = Join-Path $CopyDir (Join-Path $DropName $camera)
        LogMessage "Copying drop to $cameraCopyDir..."
        & (Join-Path $PSScriptRoot Copy-Originals.ps1) $ClipsOriginalDir $PartitionsOriginalDir $camera $cameraCopyDir
        $clipsDir = Join-Path $CopyDir $DropName
        $partitionsDir = Join-Path $CopyDir $DropName
        LogMessage "Drop copy finished."
    }

    # Check if the drop is valid
    LogMessage "Checking drop validity..."
    & (Join-Path $PSScriptRoot Check-Drop.ps1) $clipsDir $partitionsDir $camera
    LogMessage "Drop validity check finished."

    # Create poses for the drop
    $posesDirPath = Join-Path (Join-Path (Join-Path $OutputDir $DropName) $camera) $posesDirName
    LogMessage "Creating poses for the drop..."
    & (Join-Path $PSScriptRoot Create-Poses.ps1) $clipsDir $partitionsDir $camera $posesDirPath
    LogMessage "Poses creation finished."

    # Create list files.
    LogMessage "Creating ids list files..."
    GetExistingPartitionFileTypes $partitionsDir $camera | % {
        $listFilePath = Join-Path (Join-Path (Join-Path $OutputDir $DropName) $camera) ($_ + $listFilesSuffix)
        & (Join-Path $PSScriptRoot Create-ListFile.ps1) $clipsDir $posesDirPath $partitionsDir $camera $_ $listFilePath
    }
    LogMessage "Ids list files creation finished."

    LogMessage "Finished creating dataset $DropName for camera $camera." $True
}