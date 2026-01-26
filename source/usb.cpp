#include <cstdio>
#include <memory>

#include "services.hpp"
#include "usb.hpp"
#include "usbhsfs.h"
#include "windows.hpp"

namespace USB {
    static UEvent *status_change_event = nullptr, exit_event = {0};
    static u32 usb_device_count = 0;
    static std::unique_ptr<UsbHsFsDevice[]> usb_devices;
    static Thread thread = {0};
    static u32 listed_device_count = 0;
    static bool thread_created = false;
    
    // Pointer to device registry - set at init, used by thread
    static DeviceRegistry *s_device_registry = nullptr;

    // This function is heavily based off the example provided by DarkMatterCore
    // https://github.com/DarkMatterCore/libusbhsfs/blob/main/example/source/main.c
    static void usbMscThreadFunc(void *arg) {
        DeviceRegistry *dev_reg = static_cast<DeviceRegistry*>(arg);
        
        Result ret = 0;
        int idx = 0;
        
        /* Generate waiters for our user events. */
        Waiter status_change_event_waiter = waiterForUEvent(status_change_event);
        Waiter exit_event_waiter = waiterForUEvent(&exit_event);
        
        while(true) {
            /* Wait until an event is triggered. */
            if (R_FAILED(ret = waitMulti(&idx, -1, status_change_event_waiter, exit_event_waiter)))
                continue;
            
            /* Exit event triggered. */
            if (idx == 1)
                break;

            /* Free USB Mass Storage device data. */
            USB::Unmount();

            {
                std::scoped_lock lock(dev_reg->mutex);

                /* Get mounted device count. */
                usb_device_count = usbHsFsGetMountedDeviceCount();
                if (!usb_device_count)
                    continue;

                /* Allocate mounted devices buffer. */
                usb_devices = std::make_unique<UsbHsFsDevice[]>(usb_device_count);
                if (!usb_devices)
                    continue;

                /* List mounted devices. */
                if (!(listed_device_count = usbHsFsListMountedDevices(usb_devices.get(), usb_device_count))) {
                    usb_devices.reset();
                    continue;
                }

                /* Print info from mounted devices. */
                for(u32 i = 0; i < listed_device_count; i++) {
                    UsbHsFsDevice *device = std::addressof(usb_devices[i]);
                    dev_reg->devices.push_back(device->name);
                }
            }
        }
        
        /* Exit thread. */
        return;
    }

    Result Init(DeviceRegistry &dev_reg) {
        s_device_registry = &dev_reg;
        std::scoped_lock lock(dev_reg.mutex);

        Result ret = usbHsFsInitialize(0);
        if (R_SUCCEEDED(ret)) {
            /* Get USB Mass Storage status change event. */
            status_change_event = usbHsFsGetStatusChangeUserEvent();

            /* Create usermode thread exit event. */
            ueventCreate(&exit_event, true);

            /* Create thread - pass device registry as argument */
            if (R_SUCCEEDED(ret = threadCreate(&thread, usbMscThreadFunc, s_device_registry, nullptr, 0x10000, 0x2C, -2))) {
                if (R_SUCCEEDED(ret = threadStart(&thread)))
                    thread_created = true;
            }
        }

        return ret;
    }

    void Exit(void) {
        if (!s_device_registry)
            return;
        std::scoped_lock lock(s_device_registry->mutex);

        if (thread_created) {
            /* Signal background thread. */
            ueventSignal(&exit_event);

            /* Wait for the background thread to exit on its own. */
            threadWaitForExit(&thread);
            threadClose(&thread);

            thread_created = false;
        }

        /* Clean up and exit. */
        USB::Unmount();
        usbHsFsExit();
    }
    
    bool Connected(void) {
        if (!s_device_registry)
            return false;
        std::scoped_lock lock(s_device_registry->mutex);
        return (listed_device_count > 0);
    }

    void Unmount(void) {
        if (!s_device_registry)
            return;
        std::scoped_lock lock(s_device_registry->mutex);

        /* Unmount devices. */
        if (usb_devices) {
            for(u32 i = 0; i < listed_device_count; i++) {
                UsbHsFsDevice *device = std::addressof(usb_devices[i]);
                s_device_registry->devices.pop_back();
                usbHsFsUnmountDevice(device, false);
            }

            usb_devices.reset();
        }

        listed_device_count = 0;
    }
}
