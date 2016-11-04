#
# example_submit_job.ps1
#

# Include shared Jenkins functions.
. (Join-Path -Path $PSScriptRoot -ChildPath "JenkinsJobs.ps1")

# API token can be found at http://jenkins.ccp.philly.selfhost.corp.microsoft.com/user/<username>@<domain>/configure
$ApiToken = "5xq8tvv2b7i46whykuyjl9voopebz5rt" # Invalid token, provided for illustration.

[int]$TimeOut = 8 * 60 # 8 hours

$Builds = Submit-JenkinsJob `
    -ApiToken $ApiToken `
    -Repository "Private" `
    -Commit "ascience/fcn" `
    -OS "Linux" `
    -BuildConfiguration "Release" `
    -TestMode "None" `
    -TargetConfiguration "GPU" `
    -TimeOut $TimeOut

$SuccessStatus = "SUCCESS"

# Display successful builds in green.
$Builds | Where-Object {$_.Status -eq $SuccessStatus} |
%{
    Write-Host -ForegroundColor Green $_.Name $_.ID
}

# Display unsuccessful builds in red.
$Builds | Where-Object {$_.Status -ne $SuccessStatus} |
%{
    Write-Host -ForegroundColor Red $_.Name $_.ID
}