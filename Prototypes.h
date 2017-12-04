/* 
 * File:   Prototypes.h
 * Author: nicholasmitchell
 *
 * Created on November 29, 2017, 11:33 AM
 */

#ifndef PROTOTYPES_H
#define	PROTOTYPES_H
#include<p18f452.h>


void DelayFor18TCY( void );
void DelayPORXLCD (void);
void DelayXLCD (void);
void config_TMRS(void);
void config_CAPTURE(void); 
void config_PWM(void);
void config_LCD(void);
void config_PINS(void);
void start_up(void);
void display(char bpm_out[3],char hrv_out_1[4],char Temperature[20],char glucose_out[3]);
void start_temperature_conversion(int *temp_ready_pole_ptr);
void read_temperature_conversion(unsigned char *LS_Byte_ptr,unsigned char *MS_Byte_ptr);
int calculate_temperature_decimal(unsigned char LS_Byte_function,unsigned char MS_Byte_function);
int calculate_decimal(unsigned char LS_Byte_function);
void convert_temperature_reading(unsigned char *LS_Byte_ptr,unsigned char *MS_Byte_ptr,int *sign_ptr,unsigned int *Temperature_int_ptr,int *Decimal_int_ptr);
void start_up_display(void);
void start_up_display_2(void);
void start_up_display_3(void);
int keypad_press(void);
int system_start_up(void);
int pick_measurements_to_save(void);
void display_MEM(int what_to_display,int bpm,int hrv,int gl,int memory_loction);
//void speaker(int moduleType,int speaker_sound,int beep_counter);   
void speaker_on(int moduleType);   
void speaker_off(void);          //speaker sound is used to show if the speaker was ever on in this iteration 


#endif	/* PROTOTYPES_H */

