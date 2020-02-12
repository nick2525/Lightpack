/*
 * LedDevicePaintpack.hpp
 *
 *  Created on: 04.01.2013
 *      Author: Vladimir Smirnov (mindcollapse)
 *     Project: Lightpack
 *
 *  Lightpack is very simple implementation of the backlight for a laptop
 *
 *  Copyright (c) 2010, 2011 Mike Shatohin, mikeshatohin [at] gmail.com
 *
 *  Lightpack is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  Lightpack is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */


#include <QtGui>

#include "ILedDevice.hpp"
#include "TimeEvaluations.hpp"
#include "LightpackMath.hpp"

#include "../../CommonHeaders/USB_ID.h"     /* For device VID, PID, vendor name and product name */
#include "hidapi.h" /* USB HID API */

#include "../../CommonHeaders/COMMANDS.h"   /* CMD defines */

// This defines using in all data transfers to determine indexes in write_buffer[]
// In device COMMAND have index 0, data 1 and so on, report id isn't using
#define WRITE_BUFFER_INDEX_REPORT_ID    0
#define WRITE_BUFFER_INDEX_COMMAND      1
#define WRITE_BUFFER_INDEX_DATA_START   2

#define WRITE_BUFFER_BOOTLOADER 15
#define WRITE_BUFFER_COLORUPDATE 3

class LedDevicePaintpack : public ILedDevice
{
    Q_OBJECT
public:
    LedDevicePaintpack(QObject *parent = 0);
    ~LedDevicePaintpack();

public slots:
    void open();
    void setColors(const QList<QRgb> & colors);
    void switchOffLeds();
    void setRefreshDelay(int value);
    void setColorDepth(int value);
    void setSmoothSlowdown(int value);
    void setColorSequence(QString /*value*/);
    void setGamma(double value);
    void setBrightness(int percent);
    void requestFirmwareVersion();
    void updateDeviceSettings();

private: 
    bool readDataFromDevice();
    bool writeBufferToDevice(int command);
    bool tryToReopenDevice();
    bool readDataFromDeviceWithCheck();
    bool writeBufferToDeviceWithCheck(int command);    
    void resizeColorsBuffer(int buffSize);
    void closeDevice();

private slots:
    void restartPingDevice(bool isSuccess);
    void timerPingDeviceTimeout();

private:
    hid_device *m_hidDevice;

    unsigned char m_readBuffer[33];
    unsigned char m_writeBuffer[33];

    double m_gamma;
    int m_brightness;    

    QList<QRgb> m_colorsSaved;
    QList<StructRgb> m_colorsBuffer;

    QTimer *m_timerPingDevice;

    static const int PingDeviceInterval;
    static const int MaximumLedsCount;
};
