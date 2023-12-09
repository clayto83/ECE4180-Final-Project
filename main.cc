
#include "mbed.h"
#include "rtos.h"
#include "uLCD_4DGL.h"
#include "SDFileSystem.h"
#include "wave_player.h"
//#include "USBHostMSD.h"
#include <string>
#include <vector>

using namespace std;

//RawSerial  pc(USBTX, USBRX); // computer
RawSerial  dev(p28,p27); // tx, rx - Adafruit BLE

// set up objects
uLCD_4DGL lcd(p9, p10, p11); // tx, rx, rst
SDFileSystem sd(p5, p6, p7, p8, "SD"); //SD card
//USBHostMSD msd("usb"); // USB Stick
AnalogOut DACout(p18);
wave_player wav(&DACout); // sound
DigitalOut led1(LED1); // is working?
DigitalOut led2(LED2); // is working?
char buffer[4] = {0,0,0,0};

class RGBLed
{
private:
    PwmOut _red, _green, _blue;
public:
    RGBLed(PinName red, PinName green, PinName blue):
        _red(red),
        _green(green),
        _blue(blue) {
        _red.period(0.001);
    }
    void write(float r, float g, float b) {
        _red = r;
        _green = g;
        _blue = b;
    }
};

RGBLed rgb(p23, p22, p21); // r, g, b
int red, green, blue = 0;
int idx = 0;

Mutex lcd_mutex; // mutex for lcd
Mutex print_mutex; // mutex for serial

vector<string> filenames; //filenames are stored in a vector string

bool on = true;
bool play = false;
int num = 1;
int fileNum = 0;

void read_file_names(char *dir)
{
    DIR *dp;
    struct dirent *dirp;
    dp = opendir(dir);
  //read all directory and file names in current directory into filename vector
    while((dirp = readdir(dp)) != NULL) {
        filenames.push_back(string(dirp->d_name));
    }
    closedir(dp);
}

// function threads

void dev_read()
{
    while(dev.readable()) {
        print_mutex.lock();
        buffer[idx] = dev.getc();
        Thread::wait(100);
        print_mutex.unlock();
    }
    idx++;
    if (idx > 4) {
        idx = 0;
        //strcpy(buffer, "0000");
    }
}

void rgb_thread(void const* arg)
{
    while(1) {
        rgb.write(1.0, 0, 0);
        Thread::wait(500);
        rgb.write(0, 1.0, 0);
        Thread::wait(500);
        rgb.write(0, 0, 1.0);
        Thread::wait(500);
    }
}

void wav_thread(void const* arg)
{
    string songname = filenames[num];
    string a = "/SD/";
    string fname = a + songname;
    FILE *wave_file;
    wave_file = fopen("/SD/Cher_Believe.wav","r");
        if(wave_file == NULL) {
                lcd_mutex.lock();
                lcd.printf("Could not open file for read\n\r");
                lcd_mutex.unlock();
        } else {
                lcd_mutex.lock();
                lcd.printf("Found ");
                int i = 0;
                while (filenames[0][i]) lcd.printf("%c", filenames[num].at(i++));
                lcd.printf("\n\r");
                lcd_mutex.unlock();
                wav.play(wave_file);
            }
        fclose(wave_file);
    }



void lcd_thread1(void const* arg)
{
    int rad = 5;
    while(1) {
        if (buffer[0] == 'm') rad = 10;
        if (buffer[0] == 'n') rad = 15;
        lcd_mutex.lock();
        lcd.cls();
        lcd.filled_circle(64, 64, rad, BLUE);
        lcd_mutex.unlock();
       Thread::wait(1000);
    }
}

void lcd_thread2(void const* arg)
{
    while(1) {
        lcd_mutex.lock();
        //lcd.circle(20,20,5, GREEN);
        //lcd.circle(104,104,5, RED);
        //lcd.circle(20,104,5, GREEN);
        //lcd.circle(104,20,5, RED);
        lcd.locate(0,2);
        lcd.printf("There are %d files in directory:\n", fileNum);
        //lcd.locate(0,4);
        //lcd.printf("Playing file ");
        //int i = 0;
        //while (filenames[0][i]) lcd.printf("%c", filenames[1].at(i++));
        //lcd.printf("\n");
        //lcd.printf(filenames[0]);
        lcd_mutex.unlock();
        Thread::wait(500);
    }
}

int main()
{
    //pc.baud(9600);
    dev.baud(9600);
    dev.attach(&dev_read, Serial::RxIrq);
    read_file_names("/SD");
    for(vector<string>::iterator it=filenames.begin(); it < filenames.end(); it++)  
  {
    printf("%s\n\r",(*it).c_str());
  }
    fileNum = filenames.size() - 1;
    lcd.locate(0,4);
    lcd.printf("There are %d files in directory:\n", fileNum);
    lcd.locate(0,0);

    Thread th1(rgb_thread);
    Thread th2(wav_thread);
    //Thread th3(lcd_thread1);
    Thread th4(lcd_thread2);

    char bnum = 0;
    char bhit = 0;
    while (1) {
        if (dev.readable()) {
            if (dev.getc() == '!') {
                if (dev.getc() == 'B') { // button data packet
                    bnum = dev.getc(); // button number
                    bhit = dev.getc(); // 1=hit, 0=release
                    switch (bnum) {
                        case '5': // button 5 up arrow //sets play to false
                            if (bhit == '1') play = false;
                            lcd_mutex.lock();
                            lcd.locate(0,8);
                            lcd.printf("play=false");
                            lcd_mutex.unlock();
                            break;
                        case '6': //button 5 down arrow //ssets play to true
                            if (bhit == '1') play = true;
                            lcd_mutex.lock();
                            lcd.locate(0,8);
                            lcd.printf("play=true");
                            lcd_mutex.unlock();
                            break;
                        case '7': // button 7 left arrow (goes back a song)
                            if (bhit == '1') { 
                                num = num == 0 ? fileNum - 1 : num - 1;
                                //th2.terminate();
                                lcd_mutex.lock();
                                lcd.locate(0,8);
                                lcd.printf("previous song");
                                lcd_mutex.unlock();
                                //Thread th2(wav_thread);
                            }
                            break;
                        case '8': // button 8 right arrow (skips song)
                            if (bhit == '1') { 
                                num = num == fileNum - 1 ? 0 : num + 1;
                                //th2.terminate();
                                lcd_mutex.lock();
                                lcd.locate(0,8);
                                lcd.printf("next song");
                                lcd_mutex.unlock();
                                //Thread th2(wav_thread);
                            }
                            break;
                        case '1': // buttons 1 and 2
                        case '2': //turn
                            on = true; // turn it on
                            break;
                        case '3':
                        case '4': // buttons 3 and 4
                            on = false; // turn it off
                            break;
                        default:
                            break;
                    }
                }
            }
        }
        wait(1.0/20.0);
    }
}
