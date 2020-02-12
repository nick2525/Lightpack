/*
 * LightpackDevice.cpp
 *
 *  Created on: 26.07.2010
 *      Author: Mike Shatohin (brunql)
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

#include "LedDevicePaintpack.hpp"
#include "LightpackApplication.hpp"

#include <unistd.h>

#include <QtDebug>
#include "debug.h"
#include "Settings.hpp"

using namespace SettingsScope;

const int LedDevicePaintpack::PingDeviceInterval = 1000;
const int LedDevicePaintpack::MaximumLedsCount = MaximumNumberOfLeds::Paintpack;

LedDevicePaintpack::LedDevicePaintpack(QObject *parent) : ILedDevice(parent)
{
    DEBUG_LOW_LEVEL << Q_FUNC_INFO;
    DEBUG_LOW_LEVEL << Q_FUNC_INFO << "thread id: " << this->thread()->currentThreadId();

    m_hidDevice = NULL;

    memset(m_writeBuffer, 0, sizeof(m_writeBuffer));
    memset(m_readBuffer, 0, sizeof(m_readBuffer));

    m_timerPingDevice = new QTimer(this);

    m_gamma = Settings::getDeviceGamma();
    m_brightness = Settings::getDeviceBrightness();

    connect(m_timerPingDevice, SIGNAL(timeout()), this, SLOT(timerPingDeviceTimeout()));
    connect(this, SIGNAL(ioDeviceSuccess(bool)), this, SLOT(restartPingDevice(bool)));
    connect(this, SIGNAL(openDeviceSuccess(bool)), this, SLOT(restartPingDevice(bool)));

    DEBUG_LOW_LEVEL << Q_FUNC_INFO << "initialized";
}

LedDevicePaintpack::~LedDevicePaintpack()
{
    DEBUG_LOW_LEVEL << Q_FUNC_INFO << "hid_close(...);";
    closeDevice();
}

void LedDevicePaintpack::setColors(const QList<QRgb> & colors)
{
    DEBUG_MID_LEVEL << Q_FUNC_INFO << hex << (colors.isEmpty() ? -1 : colors.first());

    if (colors.count() > MaximumLedsCount) {
        qWarning() << Q_FUNC_INFO << "data size is greater than max leds count";
        return;
    }

    QMutexLocker locker(&getLightpackApp()->m_mutex);

    resizeColorsBuffer(colors.count());

    m_colorsSaved = colors;

    LightpackMath::gammaCorrection(m_gamma, colors, m_colorsBuffer);
    LightpackMath::brightnessCorrection(m_brightness, m_colorsBuffer);

    int buffIndex = 3;

    for (int i = 0; i < m_colorsBuffer.count(); i++)
    {
        StructRgb color = m_colorsBuffer[i];

        m_writeBuffer[buffIndex++] = (unsigned char)color.r;
        m_writeBuffer[buffIndex++] = (unsigned char)color.g;
        m_writeBuffer[buffIndex++] = (unsigned char)color.b;
    }

    locker.unlock();

    bool ok = writeBufferToDeviceWithCheck(3);

    emit commandCompleted(ok);
}

void LedDevicePaintpack::switchOffLeds()
{
    DEBUG_LOW_LEVEL << Q_FUNC_INFO;

    if (m_colorsSaved.count() == 0)
    {
        for (int i = 0; i < MaximumLedsCount; i++)
            m_colorsSaved << 0;
    } else {
        for (int i = 0; i < m_colorsSaved.count(); i++)
            m_colorsSaved[i] = 0;
    }

    setColors(m_colorsSaved);

    // Stop ping device if switchOffLeds() signal comes
    m_timerPingDevice->stop();

}

void LedDevicePaintpack::setRefreshDelay(int value)
{
    Q_UNUSED(value);
    emit commandCompleted(true);
}

void LedDevicePaintpack::setColorDepth(int value)
{
    Q_UNUSED(value);
    emit commandCompleted(true);
}

void LedDevicePaintpack::setSmoothSlowdown(int value)
{
    Q_UNUSED(value);
    emit commandCompleted(true);
}

void LedDevicePaintpack::setColorSequence(QString /*value*/)
{
    emit commandCompleted(true);
}

void LedDevicePaintpack::setGamma(double value)
{
    DEBUG_LOW_LEVEL << Q_FUNC_INFO << value;

    m_gamma = value;

    if (Settings::isBacklightEnabled())
        setColors(m_colorsSaved);
    else
        commandCompleted(true);
}

void LedDevicePaintpack::setBrightness(int percent)
{

    DEBUG_LOW_LEVEL << Q_FUNC_INFO << percent;

    m_brightness = percent;

    if (Settings::isBacklightEnabled())
        setColors(m_colorsSaved);
    else
        commandCompleted(true);
}

void LedDevicePaintpack::requestFirmwareVersion()
{

    DEBUG_LOW_LEVEL << Q_FUNC_INFO;

    QString fwVersion = "Paintpack DMX";

    emit firmwareVersion(fwVersion);

    emit commandCompleted(true);
}

// update firmware here
void LedDevicePaintpack::updateDeviceSettings()
{
    m_writeBuffer[0] = 0x00;
    m_writeBuffer[1] = 0x0F;
    m_writeBuffer[2] = 0x00;
    m_writeBuffer[3] = 0x01;
    m_writeBuffer[4] = 0x02;
    m_writeBuffer[5] = 0x03;
    m_writeBuffer[6] = 0x04;
    m_writeBuffer[7] = 0x05;
    m_writeBuffer[8] = 0x06;

    hid_write(m_hidDevice, m_writeBuffer, sizeof(m_writeBuffer));
}

void LedDevicePaintpack::open()
{
    DEBUG_MID_LEVEL << Q_FUNC_INFO;

    if (m_hidDevice != NULL)
    {
        closeDevice();
    }

    DEBUG_LOW_LEVEL << Q_FUNC_INFO << QString("hid_open(0x%1, 0x%2)")
                       .arg(USB_PP_VENDOR_ID, 4, 16, QChar('0'))
                       .arg(USB_PP_PRODUCT_ID, 4, 16, QChar('0'));

    m_hidDevice = hid_open(USB_PP_VENDOR_ID, USB_PP_PRODUCT_ID, NULL);

    if (m_hidDevice == NULL)
    {
        DEBUG_LOW_LEVEL << Q_FUNC_INFO << "Paintpack device not found";
        emit openDeviceSuccess(false);
        return;
    }

    // Immediately return from hid_read() if no data available
    hid_set_nonblocking(m_hidDevice, 1);

    DEBUG_LOW_LEVEL << Q_FUNC_INFO << "Paintpack opened";

    requestFirmwareVersion();

    emit openDeviceSuccess(true);
}

bool LedDevicePaintpack::readDataFromDevice()
{
    DEBUG_LOW_LEVEL << Q_FUNC_INFO;

    int bytes_read = hid_read(m_hidDevice, m_readBuffer, sizeof(m_readBuffer));

    if(bytes_read < 0){
        qWarning() << "Error reading data:" << bytes_read;
        emit ioDeviceSuccess(false);
        return false;
    }
    emit ioDeviceSuccess(true);
    return true;
}

bool LedDevicePaintpack::writeBufferToDevice(int command)
{    
    DEBUG_MID_LEVEL << Q_FUNC_INFO << command;
#if 0
    DEBUG_LOW_LEVEL << Q_FUNC_INFO << "thread id: " << this->thread()->currentThreadId();
#endif

    m_writeBuffer[0] = 0x00;
    m_writeBuffer[1] = command;
    m_writeBuffer[2] = 0x00;

    int error = hid_write(m_hidDevice, m_writeBuffer, sizeof(m_writeBuffer));
    if (error < 0)
    {
        // Trying to repeat sending data:
        error = hid_write(m_hidDevice, m_writeBuffer, sizeof(m_writeBuffer));
        if(error < 0){
            qWarning() << "Error writing data:" << error;
            emit ioDeviceSuccess(false);
            return false;
        }
    }
    emit ioDeviceSuccess(true);
    return true;
}

bool LedDevicePaintpack::tryToReopenDevice()
{
    open();

    if (m_hidDevice == NULL)
    {
        return false;
    }

    DEBUG_LOW_LEVEL << Q_FUNC_INFO << "Reopen success";
    return true;
}

bool LedDevicePaintpack::readDataFromDeviceWithCheck()
{
    DEBUG_MID_LEVEL << Q_FUNC_INFO;

    if (m_hidDevice != NULL)
    {
        if (!readDataFromDevice())
        {
            if (tryToReopenDevice())
                return readDataFromDevice();
            else
                return false;
        }
        return true;
    } else {
        if (tryToReopenDevice())
            return readDataFromDevice();
        else
            return false;
    }
}

bool LedDevicePaintpack::writeBufferToDeviceWithCheck(int command)
{
    DEBUG_MID_LEVEL << Q_FUNC_INFO;

    if (m_hidDevice != NULL)
    {
        if (!writeBufferToDevice(command))
        {
            if (!writeBufferToDevice(command))
            {
                if (tryToReopenDevice())
                    return writeBufferToDevice(command);
                else
                    return false;
            }
        }
        return true;
    } else {
        if (tryToReopenDevice())
            return writeBufferToDevice(command);
        else
            return false;
    }
}

void LedDevicePaintpack::resizeColorsBuffer(int buffSize)
{
    if (m_colorsBuffer.count() == buffSize)
        return;

    m_colorsBuffer.clear();

    if (buffSize > MaximumNumberOfLeds::Paintpack)
    {
        qCritical() << Q_FUNC_INFO << "buffSize > MaximumNumberOfLeds::Virtual" << buffSize << ">" << MaximumNumberOfLeds::Paintpack;

        buffSize = MaximumNumberOfLeds::Paintpack;
    }

    for (int i = 0; i < MaximumNumberOfLeds::Paintpack; i++)
    {
        m_colorsBuffer << StructRgb();
    }
}

void LedDevicePaintpack::closeDevice()
{
    DEBUG_MID_LEVEL << Q_FUNC_INFO;

    hid_close(m_hidDevice);
    m_hidDevice = NULL;
}

void LedDevicePaintpack::restartPingDevice(bool isSuccess)
{
    Q_UNUSED(isSuccess);

    if (Settings::isBacklightEnabled() && Settings::isPingDeviceEverySecond())
    {
        m_timerPingDevice->start(PingDeviceInterval);
    } else {
        m_timerPingDevice->stop();
    }
}

void LedDevicePaintpack::timerPingDeviceTimeout()
{
    DEBUG_MID_LEVEL << Q_FUNC_INFO;

    if (m_hidDevice == NULL)
    {
        DEBUG_MID_LEVEL << Q_FUNC_INFO << "hid_open";
        m_hidDevice = hid_open(USB_PP_VENDOR_ID, USB_PP_PRODUCT_ID, NULL);

        if (m_hidDevice == NULL)
        {
            DEBUG_MID_LEVEL << Q_FUNC_INFO << "hid_open fail";
            emit openDeviceSuccess(false);
            return;
        }
        DEBUG_MID_LEVEL << Q_FUNC_INFO << "hid_open ok";

        emit openDeviceSuccess(true);
        closeDevice(); // device should be opened by open() function
        return;
    }

    DEBUG_MID_LEVEL << Q_FUNC_INFO << "hid_write";

    m_writeBuffer[0] = 0x00;
    m_writeBuffer[1] = 0x00;
    int bytes = hid_write(m_hidDevice, m_writeBuffer, sizeof(m_writeBuffer));

    if (bytes < 0)
    {
        DEBUG_MID_LEVEL << Q_FUNC_INFO << "hid_write fail";
        closeDevice();
        emit ioDeviceSuccess(false);
        return;
    }

    DEBUG_MID_LEVEL << Q_FUNC_INFO << "hid_write ok";

    emit ioDeviceSuccess(true);
}
