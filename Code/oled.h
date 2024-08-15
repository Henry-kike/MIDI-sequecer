#ifndef OLED_H
#define OLED_H

void display_ini();
void show_play(uint8_t fig, uint8_t bpm); //fig--> 0-3
void animated_stop();
void show_stop();
void show_number(uint8_t num);
void show_bpm(uint8_t n);
void show_max_notes();
void show_sync();

#endif // OLED_H