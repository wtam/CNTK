#
# Example script for getting list of jobs from Philly cluster.
#

# Include functions from shared.ps1
. (Join-Path -Path $PSScriptRoot -ChildPath "shared.ps1")

$PhysicalCluster = "rr1"
$VirtualCluster = "anlgvc"

$JobList = Get-PhillyJobList `
                -PhysicalCluster $PhysicalCluster `
                -VirtualCluster $VirtualCluster

$UserName = $env:USERNAME

# Get finished jobs for specified user.
# To get list of running or queued jobs, use runningJobs or queuedJobs, respectively.
Write-Host -ForegroundColor Green "Finished jobs started by ($UserName): "
$JobList.finishedJobs |
    Where {$_.username -eq $UserName} |
    Sort-Object -Descending -Property finishDateTime |
    Format-Table -Wrap
