#
# Example script for submitting job to Philly cluster.
#

# Include functions from shared.ps1
. (Join-Path -Path $PSScriptRoot -ChildPath "shared.ps1")

# Our scratch is located at \\storage.rr1.philly.selfhost.corp.microsoft.com\anlgvc_scratch

# All paths are case sensitive. Use Linux-style forward slash.

# Configuration file for CNTK job is:
# \\storage.rr1.philly.selfhost.corp.microsoft.com\anlgvc_scratch\movasi\FCN32_captured_normals_idsv3\ResNet_18_to_deconv.cntk"
$ConfigFile = "movasi/FCN32_captured_normals_idsv3/ResNet_18_to_deconv.cntk"

# Location of data on Philly HDFS.
$InputDirectory = "/hdfs/anlgvc/datasets/ids/v2/Capture-70k-original-png"

# Pre-trained ResNet18 classifier binary can be found at:
# \\storage.rr1.philly.selfhost.corp.microsoft.com\anlgvc\sagalic\models\Imagenet_Class_ResNet_18
# Our VC directory on HDFS is \\storage.rr1.philly.selfhost.corp.microsoft.com\anlgvc, previous model is relative to
# that path.
$PreviousModelPath = "sagalic/models"

# This will override minibatch size and learning rate specified in configuration file.
$ExtraParameters = "TrainFCN32=[SGD=[minibatchSize=32 learningRatesPerMB=1e-8:1e-7:3.2e-7]]"

# Build ID is produced by Jenkins build. Builds get outdated and deleted, so this should be periodically updated.
# TODO: Get latest known good build (LKG) from shared.ps1.
[int]$BuildId = 20000 # This one is outdated.

$Job = Submit-PhillyJob `
            -Name FCN_lr_search `
            -ConfigFile $ConfigFile `
            -DataDir $InputDirectory `
            -BuildId $BuildId `
            -PreviousModelPath $PreviousModelPath `
            -PhysicalCluster rr1 `
            -VirtualCluster anlgvc `
            -GpuCount 2 `
            -ExtraParams $ExtraParameters

# TODO: Add error handling either here or in Submit-PhillyJob

# In case of success, $Job will contain response in JSON format with "jobId" field.
# In case of failure, $Job will contain response in JSON format with two fields: "error" and "diag".