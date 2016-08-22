#pragma once

class DeviceInfo
{
public:
    const int c_cpu_device = -1;
    static DeviceInfo& GetInstance()
    {
        static DeviceInfo instance;
        return instance;
    }
    int GetId() const { return device_id_; }
    DeviceInfo& operator=(const DeviceInfo&) = delete;
private:
    DeviceInfo() : device_id_(c_cpu_device) {}
    int device_id_;
};