#include "display_control.h"

#include <QtDebug>

#define CLK 107
#define MOSI 106
#define CS 105
#define RST 104

DisplayControl::DisplayControl()
{
    setup();
}

void DisplayControl::setup(void)
{
#ifdef __arm__
    wiringPiSetup () ;
    mcp23017Setup (100, 0x21);
#endif
}

void DisplayControl::spi_screenreg_set(int32_t Addr, int32_t Data0, int32_t Data1)
{
#ifdef __arm__
    int32_t i;
    int32_t control_bit;

    pinMode(MOSI, OUTPUT);
    pinMode(CLK, OUTPUT);
    pinMode(CS, OUTPUT);
    nanosleep(&ts, NULL);
    nanosleep(&ts, NULL);

    digitalWrite(CS, HIGH);
    digitalWrite(MOSI, HIGH);
    digitalWrite(CLK, LOW);
    nanosleep(&ts2, NULL);

    digitalWrite(CS, LOW);
    control_bit = 0x0000;
    Addr = (control_bit | Addr);

    for(i = 0; i<9; i++)
    {
        if(Addr &(1<<(8-i)))
            digitalWrite(MOSI, HIGH);
        else
            digitalWrite(MOSI, LOW);

        // \u6a21\u62dfCLK
        digitalWrite(CLK, HIGH);
        nanosleep(&ts, NULL);
        digitalWrite(CLK, LOW);
        nanosleep(&ts, NULL);
    }

    digitalWrite(CS, HIGH);
    digitalWrite(MOSI, HIGH);
    digitalWrite(CLK, LOW);
    nanosleep(&ts3, NULL);

    if(0xffff == Data0){
        return;
    }

    digitalWrite(CS, LOW);

    control_bit = 0x0100;
    Data0 = (control_bit | Data0);

    for(i = 0; i < 9; i++)  //data
    {
        if(Data0 &(1<<(8-i)))
            digitalWrite(MOSI, HIGH);
        else
            digitalWrite(MOSI, LOW);
        digitalWrite(CLK, HIGH);
        nanosleep(&ts, NULL);
        digitalWrite(CLK, LOW);
        nanosleep(&ts, NULL);
    }

    digitalWrite(CS, HIGH);
    digitalWrite(CLK, LOW);
    digitalWrite(MOSI, LOW);
    nanosleep(&ts2, NULL);

    if(0xffff == Data1)
        return;

    digitalWrite(CS, LOW);

    control_bit = 0x0100;
    Data1 = (control_bit | Data1);

    for(i = 0; i < 9; i++)  //data
    {
        if(Data1 &(1<<(8-i)))
            digitalWrite(MOSI, HIGH);
        else
            digitalWrite(MOSI, LOW);
        digitalWrite(CLK, HIGH);
        nanosleep(&ts, NULL);
        digitalWrite(CLK, LOW);
        nanosleep(&ts, NULL);
    }

    digitalWrite(CS, HIGH);
    digitalWrite(CLK, LOW);
    digitalWrite(MOSI, LOW);
    nanosleep(&ts3, NULL);
#endif
}

bool DisplayControl::setmode(const QString &mode)
{
    if (mode == "standbyon") {
#ifdef __arm__
        QFuture<void> future = QtConcurrent::run([&](){
            delay(400); // wait until dimming of the display is done
            spi_screenreg_set(0x10, 0xffff, 0xffff);
            delay(120);
            spi_screenreg_set(0x28, 0xffff, 0xffff);
        });
#endif
        return true;
    }
    if (mode == "standbyoff") {
#ifdef __arm__
        QFuture<void> future = QtConcurrent::run([&](){
            spi_screenreg_set(0x29, 0xffff, 0xffff);
            spi_screenreg_set(0x11, 0xffff, 0xffff);
        });
#endif
        return true;
    }
    return false;
}

void DisplayControl::setBrightness(int from, int to)
{
    QFuture<void> future = QtConcurrent::run([&](int from, int to) {
#ifdef __arm__
        if (from == 0 && digitalRead(26) == 0) {
            pinMode(26, PWM_OUTPUT);
            pwmSetMode(PWM_MODE_MS);
            pwmSetClock(1000);
            pwmSetRange(100);
        }

        if (from >= to) {
            // dim down
            for (int i=from; i>to-1; i--)
            {
                pwmWrite(26, i);
                delay(10);
                if (i == 0) {
                    delay(100);
                    pinMode(26, OUTPUT);
                    digitalWrite(26, 0);
                }
            }
        } else {
            // dim up
            for (int i=from; i<to+1; i++)
            {
                pwmWrite(26, i);
                delay(10);
            }
        }
#endif
    }, from, to);
}

void DisplayControl::batteryChargingOn()
{
#ifdef __arm__
    pinMode(108, OUTPUT);
    digitalWrite(108, LOW);
    qDebug() << "Turning battery charging on";
#endif
}

void DisplayControl::batteryChargingOff()
{
#ifdef __arm__
    pinMode(108, OUTPUT);
    digitalWrite(108, HIGH);
    qDebug() << "Turning battery charging off";
#endif
}
