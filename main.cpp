#include "mbed.h"
#include "rtos.h"
#include "uLCD_4DGL.h"
#include "SDFileSystem.h"
#include "wave_player.h"
#include <string>
#include <vector>

Serial  dev(p13,p14);                   // Bluetooth module
uLCD_4DGL lcd(p9, p10, p11);            // LCD Display
AnalogOut aOut(p18);                    // Output pin for audio amp 
SDFileSystem sd(p5, p6, p7, p8, "sd");  // SD Card
wave_player wav(&aOut);                 // Class to play audio from wav file 
                                        // NOTE: We use a modified version of this class to enable pause and playing

vector <string> filenames;              // Vector of the filenames on the SD Card
bool playing;                           // Stores if song is playing or paused, linked in wave-player
int vol;                                // Stores the volume, linked in wave-player
// bool changed;                           // Indicates the current song has changed, linked in wave-player
int curr;                               // Stores the current song number that's playing 
int numS;                               // Stores the total number of songs

Mutex mutex;                            // Having this mutex made the mbed crash less

// Thread constantly looks for user input via bluetooth and updates variables accordingly
void blue_func(void const* arg) {
    // Set bluetooth baudrate
    dev.baud(9600);
    // Start loop to scan for user input
    while(1) {
        char bnum = 0;
        char bhit = 0;
        mutex.lock();
        if(dev.readable() && (dev.getc() == '!') && (dev.getc() == 'B')) {
            bnum = dev.getc();
            bhit = dev.getc();
            switch (bnum) {
                // Buttons 1 and 2 pause
                case '1':
                case '2': if (bhit == '1' && playing) playing = false;
                    break;
                // Buttons 3 and 4 play
                case '3':
                case '4': if (bhit == '1' && !playing) playing = true;
                    break;
                // Up Arrow increases the volume
                case '5': if (bhit == '1') vol = vol - 1 < 0 ? 15 : vol - 1;
                    break;
                // Down Arrow decreases the volume
                case '6': if (bhit == '1') vol = vol + 1 > 15 ? 0 : vol + 1;
                    break;
                // Left Arrow goes back a song
                case '7': 
                    if (bhit == '1'){
                        curr = curr - 1 < 1 ? numS : curr - 1;
                        // changed = true;
                    }
                    break;
                // Right Arrow skips a song
                case '8': 
                    if (bhit == '1'){
                        curr =  curr + 1 > numS ? 1 : curr + 1;
                        // changed  = true;
                    }
                    break;
                default: break;
            }
        }
        mutex.unlock();
    }
}

// Thread plays the current wav file and automatically increments to the next one
void wav_func(void const* arg)
{
    // The first while loop just waits for playing to be true
    // This is needed in the event of a pause
    while(1) {
        // changed = false;  // Needs to reset the change flag in the event a song is skipped
        // This while loop will play the current song, and then automatically move to the next one
        while(playing) {
            FILE *wave_file;
            wave_file = fopen(("/sd/" + filenames[curr]).c_str(),"r");
            wav.play(wave_file);
            fclose(wave_file);
            curr =  curr + 1 > numS ? 1 : curr + 1;
        }
        wait(0.1);
    }
}

// Main function
// This will set everything up, and then act as the lcd_thread
// It will constantly referesh the lcd and display based on the current status
int main() {
    //Initializing
    playing = false;
    // changed = false;
    vol = 7; 
    curr = 1;
    numS = -1;

    // Filling the filenames vector
    sd.disk_initialize();
    DIR *dp;
    struct dirent *dirp;
    dp = opendir("/sd/");
    while (dirp = readdir(dp)) {
        filenames.push_back(string(dirp->d_name));
        ++numS;
    }
    closedir(dp);

    // Clear the lcd display
    lcd.cls();

    // If there are no songs, give a flashing message to the user
    if (numS == 0) {
        while (1) {
            wait(0.1);
            lcd.cls();
            lcd.locate(0,0);
            lcd.color(RED);
            lcd.printf("There are no wav files in the directory.");
            lcd.locate(0,1);
            lcd.printf("Please remove the sd card and add your wav files");
            wait(0.1);
        }
    }

    // Start the threads
    Thread blue_thread(blue_func);
    Thread wav_thread(wav_func);

    // While waiting for the user to hit play, display the song names
    // Up to the first 16 will fit on the screen        
    lcd.locate(0,0);
    lcd.color(RED);
    lcd.printf("%d files found:", numS);
    for (int i = 0; (i < 16) && (i < numS) ; ++i) {
        lcd.locate(0, i + 2);
        lcd.color(BLUE);
        lcd.printf("\r%s\r", filenames[i + 1].substr(0, filenames[i + 1].find(".wav")));
        lcd.locate(0, i + 3);
        lcd.printf("\r%s\r", "                  ");
    }
    while (!playing) wait(0.1);

    // Once the user hits play, display the name of the current song and the play or pause icon depending on the action
    bool old_playing = false;  // boolean to keep track of if we need to clear the screen 
    while(1) {
        // Display song name at the top
        if ((old_playing != playing)) { // || changed) {
            mutex.lock();
            lcd.cls();
            lcd.locate(0,0);
            lcd.color(BLUE);
            lcd.printf("\r%s\r", filenames[curr].substr(0, filenames[curr].find(".wav")));
            // Display playing or paused symbol
            if (playing) {
                lcd.circle(60,70,40,GREEN);
                lcd.triangle(45,100,45,40,95,70,WHITE);
            
            } else {
                lcd.circle(60,70,40,RED);
                lcd.rectangle(45,55,55,85,RED);
                lcd.rectangle(65,55,75,85,RED);
            }
            mutex.unlock();
            old_playing = playing;
        }
        wait(0.25);
    }
}