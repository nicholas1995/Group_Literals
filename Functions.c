
#include <p18f452.h>
#include "Prototypes.h"
#include<delays.h>
#include"xlcd.h"
#include<timers.h>
#include<capture.h>
#include<math.h>
#include<string.h>
#include<stdio.h>
#include<stdlib.h>
#include <pwm.h>                   
#include"ow.h"


#define bitmask 240             //KEYPAD
void DelayFor18TCY( void )
{
    Delay1TCY();
    Delay1TCY();
    Delay1TCY();
    Delay1TCY();
    Delay10TCYx(1);
}

void DelayPORXLCD (void)
{
 Delay10KTCYx(6);
}

void DelayXLCD (void)
{
 Delay10KTCYx(2); 
}

/*
 ***********************************************************************************************************
 *                                       FUNCTIONS
 ***********************************************************************************************************  
 */
/*
 *******************************************************************************
 *                                  CONFIGERATION
 *******************************************************************************
 */
/*
 ***********************************************************
 *                        TIMER 1 & TIMER 3  
 ***********************************************************
 */
void config_TMRS(void)
{   
//    OpenTimer0(TIMER_INT_ON &           //Interrupt enabled             //HRV
//               T0_16BIT &            //16-bit mode
//               T0_PS_1_256 &            //1:256 prescale 
//               T0_SOURCE_INT);          //Timer1 source for both CCP?s
//    INTCONbits.INT0IF=0;
//    WriteTimer0(26368);

    OpenTimer1(TIMER_INT_ON &           //Interrupt enabled             //HRV
               T1_16BIT_RW &            //16-bit mode
               T1_PS_1_8 &              //1:8 prescale 
               T1_OSC1EN_OFF &          //Disable Timer1 oscillator
               T1_SYNC_EXT_OFF&         //Don?t sync external clock input
               T1_SOURCE_INT &
               T1_SOURCE_CCP);          //Timer1 source for both CCP?s
   PIR1bits.TMR1IF=0;      
   
    OpenTimer3(TIMER_INT_ON &                                           //HR
             T3_16BIT_RW &
             T3_SOURCE_INT &
             T0_PS_1_8 &
             T3_SYNC_EXT_OFF);
    WriteTimer3(52992);
    RCONbits.IPEN = 1;              //Enable Priority Levels
}
/*
 ***********************************************************
 *                        CAPTURE (RC2)
 ***********************************************************
 * This function configures CAPTURE (RC2) to operate in the following modes:
 ***********************************************************
 */
void config_CAPTURE(void)                                               //HRV
{
    OpenCapture1(CAPTURE_INT_ON &       //Interrupts Enabled
                C1_EVERY_RISE_EDGE);    //Interrupt on every rising edge
}
void config_PWM(void)
{
    SetDCPWM2(12);
    OpenTimer2(TIMER_INT_OFF & T2_PS_1_4 & T2_POST_1_1);
}
/*
 ***********************************************************
 *                        XLCD
 ***********************************************************
 */
void config_LCD(void)
{
    OpenXLCD( FOUR_BIT & LINES_5X7 );
    while( BusyXLCD() );
    WriteCmdXLCD( FOUR_BIT & LINES_5X7 );
    while( BusyXLCD() );
    WriteCmdXLCD( BLINK_ON );
    while( BusyXLCD() );
    WriteCmdXLCD( SHIFT_DISP_LEFT );
}
/*
 ***********************************************************
 *                        PINS 
 ***********************************************************
 */
void config_PINS(void)
{
    //CONFIGURE PORT B TO USE INTERRUPT ON CHANGE                       //HR                    
    TRISBbits.RB0=1;
    INTCONbits.INT0IE=1;//INT0 External Interrupt Enable bit
    INTCON2bits.INTEDG0=1;//INTERRUPT ON RISING EDGE
    
    INTCON3bits.INT1E=1;                                                //KEYPAD
    INTCON3bits.INT1F=0;
    TRISBbits.RB1=1;
    TRISCbits.RC4=1;
    TRISCbits.RC5=1;
    TRISCbits.RC6=1;
    TRISCbits.RC7=1;
    TRISCbits.RC0=0;
    
    TRISBbits.RB3=0;            //set RC7 as output to transistor       //TEMPERATURE

    TRISCbits.RC2 = 0;               //SPEAKER



    
}
/*
 **************************************************************************************
 *                         FUNCTION CALLS AT START 
 **************************************************************************************
 */
void start_up(void)
{
    //This must be run at the start of the program to initialise everything
    config_TMRS();
    config_PINS();
    config_CAPTURE();
    config_PWM();
}

/*
 **************************************************************************************
 *                              DISPLAY READING 
 **************************************
 * Timing 

 **************************************************************************************
 */
void display(char bpm_out[3],char hrv_out_1[4],char Temperature[20],char glucose_out[3])
{
    while(BusyXLCD());                                      //HR
    WriteCmdXLCD(0b00000001);

    while(BusyXLCD());
    SetDDRamAddr( 0x00 );
    putrsXLCD( "HR(bpm):");

    while(BusyXLCD());
    SetDDRamAddr( 0x08 );
    putsXLCD(bpm_out);
    
    while(BusyXLCD());                                  //HRV
    SetDDRamAddr( 0x40 );
    
    putrsXLCD( "HRV(%):");
    while(BusyXLCD());
    SetDDRamAddr( 0x47 );
    putsXLCD(hrv_out_1);
    
   while(BusyXLCD());                                   //TEMP
    SetDDRamAddr( 0x10 );

    while(BusyXLCD());
    putsXLCD(Temperature);
    
      while(BusyXLCD());                                  //HRV
    SetDDRamAddr( 0x50 );
    
    putrsXLCD( "GL(mg/dL):");
    while(BusyXLCD());
    SetDDRamAddr( 0x5A);
    putsXLCD(glucose_out);
    
}



//TEMPERATURE FUNCTIONS
/*
 **************************************************************************************
 *                         START TEMPERATURE CONVERSION
 **************************************************************************************
 */
void start_temperature_conversion(int *temp_ready_pole_ptr)
{
    ow_reset();  //reset 1822P
    ow_write_byte(0xCC); // Skip ROM Check
    ow_write_byte(0x44); //Temperature conversion 
    PORTBbits.RB3 = 1;  //Strong pullup to provide current that parasitic capacitance can't provide
    *temp_ready_pole_ptr=1;
}
/*
 **************************************************************************************
 *                         READ TEMPERATURE CONVERSION
 **************************************************************************************
 */
void read_temperature_conversion(unsigned char *LS_Byte_ptr,unsigned char *MS_Byte_ptr)
{
    unsigned char LS_Byte=*LS_Byte_ptr;
    unsigned char MS_Byte=*MS_Byte_ptr;

       PORTBbits.RB3 = 0; 
        ow_reset();
        ow_write_byte(0xCC); 
        ow_write_byte(0xBE); 
        LS_Byte = ow_read_byte(); 
        MS_Byte = ow_read_byte();
        
        *LS_Byte_ptr=LS_Byte;
        *MS_Byte_ptr=MS_Byte;

}

/*
 **************************************************************************************
 *                         CALCULATE TEMPERATURE DECIMAL 
 **************************************************************************************
 */
int calculate_temperature_decimal(unsigned char LS_Byte_function,unsigned char MS_Byte_function)
{
    unsigned int MS_Byte_Temporary = 0;
    unsigned int LS_Byte_Temporary =0;
    unsigned int Temperature_int_function = 0;

    
    LS_Byte_Temporary = LS_Byte_function >> 4; 
    MS_Byte_Temporary = MS_Byte_function << 4;
    Temperature_int_function = MS_Byte_Temporary | LS_Byte_Temporary; 
    return Temperature_int_function;
}

/*
 **************************************************************************************
 *                         CALCULATE DECIMAL 
 **************************************************************************************
 */
int calculate_decimal(unsigned char LS_Byte_function)
{
    unsigned char decimal_temp;
    int Decimal_int_function =0;

    decimal_temp=(LS_Byte_function & 0x0F);
    switch(decimal_temp)
    {
        case 0:                         //0000
            Decimal_int_function=0;
            break;
        case 1:                         //0001     
            Decimal_int_function=1;
            break;   
        case 2:                         //0010
            Decimal_int_function=1;
            break; 
        case 3:                         //0011
            Decimal_int_function=2;
            break; 
        case 4:                         //0100
            Decimal_int_function=3;
            break; 
        case 5:                         //0101
            Decimal_int_function=3;
            break;     
        case 6:                         //0110
            Decimal_int_function=4;
            break; 
        case 7:                         //0111
            Decimal_int_function=4;
            break;     
        case 8:                         //1000
            Decimal_int_function=5;
            break; 
        case 9:                         //1001
            Decimal_int_function=6;
            break;     
        case 10:                         //1010
            Decimal_int_function=6;
            break;     
        case 11:                         //1011
            Decimal_int_function=7;
            break; 
        case 12:                         //1100
            Decimal_int_function=8;
            break;     
        case 13:                         //1101
            Decimal_int_function=8;
            break;     
        case 14:                         //1110
            Decimal_int_function=9;
            break;     
        case 15:                         //1111
            Decimal_int_function=9;
            break;
        default:
            break;       
    }
    return Decimal_int_function;
}
/*
 **************************************************************************************
 *                         CONVERT TEMPERATURE READING
 **************************************************************************************
 */
void convert_temperature_reading(unsigned char *LS_Byte_ptr,unsigned char *MS_Byte_ptr,int *sign_ptr,unsigned int *Temperature_int_ptr,int *Decimal_int_ptr)
{
    unsigned char LS_Byte=*LS_Byte_ptr;
    unsigned char MS_Byte=*MS_Byte_ptr;
    int sign = *sign_ptr;
    unsigned int Temperature_int = *Temperature_int_ptr;
    int Decimal_int =*Decimal_int_ptr;
    


        Temperature_int=calculate_temperature_decimal(LS_Byte,MS_Byte);
        
        Decimal_int=calculate_decimal(LS_Byte);

        sign = (MS_Byte >> 3);
        
        *sign_ptr=sign;
        *Temperature_int_ptr=Temperature_int;
        *Decimal_int_ptr=Decimal_int;
}

/*
 **********************************************************************************************************
 *                                          START UP DISPLAY
 **********************************************************************************************************
 */
void start_up_display(void)
{
    while(BusyXLCD());                                     
    WriteCmdXLCD(0b00000001);

    while(BusyXLCD());
    SetDDRamAddr( 0x00 );
    putrsXLCD( "Group LITERALS");

    while(BusyXLCD());                                 
    SetDDRamAddr( 0x40 );
    putrsXLCD( "Enter #1 to");
    
    while(BusyXLCD());
    SetDDRamAddr( 0x10 );
    putrsXLCD( "continue");

}
void start_up_display_2(void)
{
    while(BusyXLCD());                                     
    WriteCmdXLCD(0b00000001);

    while(BusyXLCD());
    SetDDRamAddr( 0x00 );
    putrsXLCD( "A-store HR");

    while(BusyXLCD());                                 
    SetDDRamAddr( 0x40 );
    putrsXLCD( "B-store HRV");
    
    while(BusyXLCD());
    SetDDRamAddr( 0x10 );
    putrsXLCD( "C-store GL");
    
    while(BusyXLCD());
    SetDDRamAddr( 0x50 );
    putrsXLCD( "D-store All");
}

void start_up_display_3(void)
{
    while(BusyXLCD());                                     
    WriteCmdXLCD(0b00000001);
    
    while(BusyXLCD());
    SetDDRamAddr( 0x00 );
    putrsXLCD( "Reading...");
}

int keypad_press(void)
{
    int value;
    value=PORTC>>4;
    return value;
}
/*
 **********************************************************************************************************
 *                                          SYSTEM START UP
 **********************************************************************************************************
 */
int system_start_up(void)
{
    int keypad_test=1; 
    start_up_display();
        Delay10KTCYx(60);
        if(INTCON3bits.INT1F==1)
        {
            keypad_test=keypad_press();
            INTCON3bits.INT1F=0;
        }
        return keypad_test;
}

int pick_measurements_to_save(void)
{
    int keypad_value=0; 
    start_up_display_2();
        Delay10KTCYx(60);
        if(INTCON3bits.INT1F==1)
        {
            keypad_value=keypad_press();
            INTCON3bits.INT1F=0;
        }
        return keypad_value;
}

void display_MEM(int what_to_display,int bpm,int hrv,int gl,int memory_loction)
{
    char bpm_out[4];
    char hrv_out[4];
    char glucose_out[3];
    char memory_location_char[2];
    int memory_location_actual=memory_loction-2;   //because we initialized memory_location to 3 so that we have some empty space in the array beforehand 

    itoa(bpm,bpm_out);
    itoa(hrv,hrv_out);
    itoa(gl,glucose_out);
    itoa(memory_location_actual,memory_location_char);

    
    switch (what_to_display)
    {
        case 3:     //HR
            while(BusyXLCD());                                      //HR
            WriteCmdXLCD(0b00000001);

            while(BusyXLCD());
            SetDDRamAddr( 0x00 );
            putrsXLCD( "HR(bpm)");

            while(BusyXLCD());
            SetDDRamAddr( 0x07 );
            putsXLCD(memory_location_char);
            
            while(BusyXLCD());
            SetDDRamAddr( 0x09 );
            putrsXLCD( ":");

            while(BusyXLCD());
            SetDDRamAddr( 0x0A);
            putsXLCD(bpm_out);
        break;
            
        case 7:     //HRV
            while(BusyXLCD());                                      //HRV
            WriteCmdXLCD(0b00000001);

            while(BusyXLCD());
            SetDDRamAddr( 0x00 );
            putrsXLCD( "HRV(%)");
            
            while(BusyXLCD());
            SetDDRamAddr( 0x06 );
            putsXLCD(memory_location_char);
            
            while(BusyXLCD());
            SetDDRamAddr( 0x08 );
            putrsXLCD( ":");

            while(BusyXLCD());
            SetDDRamAddr( 0x09 );
            putsXLCD(hrv_out);
        break;
        
        case 11:     //GL
            while(BusyXLCD());                                      //GL
            WriteCmdXLCD(0b00000001);

            while(BusyXLCD());
            SetDDRamAddr( 0x00 );
            putrsXLCD( "GL(mg/dL)");
            
            while(BusyXLCD());
            SetDDRamAddr( 0x09 );
            putsXLCD(memory_location_char);
            
            while(BusyXLCD());
            SetDDRamAddr( 0x0B );
            putrsXLCD( ":");

            while(BusyXLCD());
            SetDDRamAddr( 0x0C );
            putsXLCD(glucose_out);
        break;
            
        case 15:    //ALL
            
            while(BusyXLCD());                                      //HR
            WriteCmdXLCD(0b00000001);

            while(BusyXLCD());
            SetDDRamAddr( 0x00 );
            putrsXLCD( "HR(bpm):");

            while(BusyXLCD());
            SetDDRamAddr( 0x08 );
            putsXLCD(bpm_out);

            while(BusyXLCD());                                  //HRV
            SetDDRamAddr( 0x40 );
            putrsXLCD( "HRV(%):");

            while(BusyXLCD());
            SetDDRamAddr( 0x47 );
            putsXLCD(hrv_out);

            while(BusyXLCD());                                  //GL
            SetDDRamAddr( 0x10 );
            putrsXLCD( "GL(mg/dL):");

            while(BusyXLCD());
            SetDDRamAddr( 0x1A);
            putsXLCD(glucose_out);
        break;
    }
    
}

/*
 **********************************************************************************************************
 *                                          SECOND DISPLAY
 **********************************************************************************************************
 */

void speaker_on(int moduleType)          //speaker sound is used to show if the speaker was ever on in this iteration 
{                                                                         //if 0 that means this is the first time it is coming here for this iteration 
    int pwmParameter=0;                                                   //speaker_state is used to show if the speaker is on when it comes into this loop 

        switch(moduleType)
        {
            case 1:
                pwmParameter=65;
                break;
            case 2:
                pwmParameter=30;
                break;
            case 3:
                pwmParameter=45;
                break;
        }
         OpenPWM2(pwmParameter);
}
void speaker_off(void)          //speaker sound is used to show if the speaker was ever on in this iteration 
{                                                                         //if 0 that means this is the first time it is coming here for this iteration 
         OpenPWM2(00);
}



