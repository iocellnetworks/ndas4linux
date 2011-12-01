
#ifndef __MIO_CONFIG_H__
#define __MIO_CONFIG_H__

class MioConfig {
public:
    MioConfig();

    static int load();
    static void getOemName();
    static int save();
    static void activateVideoModeSettings();

private:
    static int ourVideoMode;
    static int ourDisplayOffsetX;
    static int ourDisplayOffsetY;
};

#endif // __MIO_CONFIG_H__
