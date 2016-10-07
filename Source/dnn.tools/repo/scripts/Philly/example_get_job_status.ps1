#
# Example script for getting job status from Philly cluster.
#

# Include functions from shared.ps1
. (Join-Path -Path $PSScriptRoot -ChildPath "shared.ps1")

$PhysicalCluster = "rr1"
$VirtualCluster = "anlgvc"
$app="application_1474301356012_0130"

Get-PhillyJobStatus `
    -JobId $app `
    -PhysicalCluster $PhysicalCluster `
    -VirtualCluster $VirtualCluster
