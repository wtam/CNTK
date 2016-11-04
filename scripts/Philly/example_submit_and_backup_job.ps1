#
# Script of submitting job to Philly cluster and copying training input & output files into a permanent directory.
#

# Include functions from shared.ps1
. (Join-Path -Path $PSScriptRoot -ChildPath "shared.ps1")

# Our scratch is located at \\storage.rr1.philly.selfhost.corp.microsoft.com\anlgvc_scratch

# All paths are case sensitive. Use Linux-style forward slash.

# Configuration file for CNTK job is:
# \\storage.rr1.philly.selfhost.corp.microsoft.com\anlgvc_scratch\movasi\FCN32_captured_normals_idsv3\ResNet_18_to_deconv.cntk"
$ConfigFile = "ivanst/resnet-idsv3a-norand/ResNet_18_256xAsp-ids-png.cntk"

# Location of data on Philly HDFS.
$InputDirectory = "/hdfs/anlgvc/datasets/ids/v3/ImageNet-ILSVRC-2012-256xAspRatio-png"

# Pre-trained ResNet18 classifier binary can be found at:
# \\storage.rr1.philly.selfhost.corp.microsoft.com\anlgvc\sagalic\models\Imagenet_Class_ResNet_18
# Our VC directory on HDFS is \\storage.rr1.philly.selfhost.corp.microsoft.com\anlgvc, previous model is relative to
# that path.
$PreviousModelPath = "sys/jobs/application_1476768521734_0083/models"

# This will override minibatch size and learning rate specified in configuration file.
$ExtraParameters = "Train=[SGD=[learningRatesPerMB=0.03 maxEpochs=16]]"

# Destination path to copy the file.
$BackupOutputDir = "//analogfs/private/SemanticLabeling"

$PhysicalCluster = "rr1"
$VirtualCluster = "anlgvc"

# Build ID is produced by Jenkins build. Builds get outdated and deleted, so this should be periodically updated.
# TODO: Get latest known good build (LKG) from shared.ps1.
[int]$BuildId = 29586 # This one is outdated.

$Job = Submit-PhillyJob `
            -Name rn18-norandTest2-16gpu-9 `
            -ConfigFile $ConfigFile `
            -DataDir $InputDirectory `
            -BuildId $BuildId `
            -PreviousModelPath $PreviousModelPath `
            -PhysicalCluster $PhysicalCluster `
            -VirtualCluster $VirtualCluster `
            -GpuCount 16 `
            -ExtraParams $ExtraParameters

# TODO: Add error handling either here or in Submit-PhillyJob

# In case of success, $Job will contain response in JSON format with "jobId" field.
# In case of failure, $Job will contain response in JSON format with two fields: "error" and "diag".

$JobId = $Job.jobId

# Query the job status
$JobStatus = Get-PhillyJobStatus `
          -JobId $JobId `
          -PhysicalCluster $PhysicalCluster `
          -VirtualCluster $VirtualCluster

# Query the job status recurrently.

while (($JobStatus.status -match ("Queued")) -or ($JobStatus.status -match ("Running")))
{
    $JobStatus = Get-PhillyJobStatus `
              -JobId $JobId `
              -PhysicalCluster $PhysicalCluster `
              -VirtualCluster $VirtualCluster
    Write-Host("Current status is $($JobStatus.status.ToString())")
    #Sleep for 1 mins
    Start-Sleep -s 60
}

if (($JobStatus.status -match ("Failed")) -or ($JobStatus.status -match ("Killed")))
{
    Write-Error("Job is $($JobStatus.status.ToString())")
    return
}

if ($JobStatus.status -match ("Pass"))
{
    $BackupOutputDir = Join-Path $BackupOutputDir $JobId
    $Job = CopyTraingFiles-PhillyJob `
        -OutputBaseDir $BackupOutputDir `
        -ConfigFile $ConfigFile `
        -PreviousModelPath $PreviousModelPath `
        -OutputModelFolder  $JobStatus.dir.ToString() `
        -OutputLogFolder $JobStatus.scratch.ToString() `
        -PhysicalCluster $PhysicalCluster `
        -VirtualCluster $VirtualCluster
}

# TODO: Add error handling either here or in Submit-PhillyJob

# In case of success, $Job will contain response in JSON format with "jobId" field.
# In case of failure, $Job will contain response in JSON format with two fields: "error" and "diag".