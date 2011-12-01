
#ifndef __MIO_USB_DEVENTRY_H__
#define __MIO_USB_DEVENTRY_H__

namespace mio {
    namespace usb {

    typedef int DevId;

    class DeviceEntry {
    public:
        typedef void (*Handler)(DevId devId);
        DeviceEntry();
        ~DeviceEntry();

        int operator==(const DeviceEntry& o) const;
        int operator==(DevId o) const;

        DevId myDevId;
        Handler myHandler;

    private:
        DeviceEntry(const DeviceEntry&); // disable
        DeviceEntry& operator=(const DeviceEntry&); // disable
    };
};

};

#endif // __MIO_USB_DEVENTRY_H__
