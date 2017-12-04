//THIS WORKS WITH THE HR,HRV,TEMPERATURE AND DUMMY VALUES FOR GLUCOSE GOOD
//ALSO HAS WELCOME SCREEN WITH #1 PRESS
//ALSO HAS THE ABILITY TO SAVE THE PREVIOUS READINGS FOR HR,HRV,GL,ALL
//Speaker working (constant tone)
//Allows after viewing memory to go back and start bk measuring 
//Allows user to pick how many samples anytime and then goes to display values after amount is taken

//HAS TO RUN IN DEBUGGING MODE HOWEVER
#include<p18f452.h>
#include<timers.h>
#include<delays.h>
#include"xlcd.h"
#include<string.h>
#include<stdio.h>
#include<stdlib.h>
#include<ctype.h>
#include<capture.h>
#include<math.h>
#include <pwm.h>                   
#include"Prototypes.h"

#pragma config  OSC=HS
#pragma config  LVP=OFF
#pragma config  WDT=OFF

/*What does this suppose to do 
 * 1. Everytime the users high pulses it breakes the infra red light casuing a peak at the output
 * 2. This peak has a max of about 2V after the front end
 * 3. To register a high the pin has to have a voltage greater than 0.25Vdd-0.8Vdd (TTL logic pins)
 * 4. We will be using TMR1 in counter mood, thus anytime TOCKI(RA4) registers a high it increments the counter
 * 5. We will use TMR3 to operate as a timer that will overflow at .1s.
 * 6. When this overflows it will increment a counter variable that will count to 100.
 * 7. When this reaches 100 we will read the value in TMR1 using ReadTimer1 and this will be the 
 *    the amount of beats in 10 seconds
 *  8. We will then multiply this value by 6 to give the heart beat in beats per minuite 
 * 
 */
#define BLOCK   0
#define RESTART 0

#define TRUE   1
#define FALSE 0

#define bitmask 240             //KEYPAD
#define DATA_AVAILABLE  1
#define NO_NEW_DATA 0

#define STORE_HR 3
#define DISPLAY_HR 3
#define STORE_HRV 7
#define DISPLAY_HRV 7
#define STORE_GL 11
#define DISPLAY_GL 11
#define STORE_ALL 15
#define DISPLAY_ALL 15


#define GO_UP 5
#define GO_DOWN 9



void low_isr(void);
void high_isr(void);
/*
 **********************************************************************************************************
 *                                  GLOBAL VARIABLES
 **********************************************************************************************************
 */
int count_TMR1=0;           //This keeps a track of the amount of time TMR3 overflows       //HR
int count_RB=0;             //This counts the amount of time RB6 sees a rising edge 
int count_RB_previous=0;
            
int result_h=0;                                                                             //HRV
int result_l=0;
int overflow_1=0;
int overflow_2=0;
int counter=0;
int sample=0;

int hrv_ready=0;
int hr_ready=0;

int read=0;                             //KEYPAD

int count_TMR1_800ms=0;                 //TEMP

int TMR_0_COUNTER_10sec=0;

unsigned int MEMORY[30]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
int memory_loction=3;
int memory_display_loction=3;
int display_flag=0;      //this is a flag to show when stored readings are being dislayed(1 display readings)


/*
 **********************************************************************************************************
 *                               LOW PRIORITY INTERRUPT
 **********************************************************************************************************
 */
#pragma code low_vector=0x18
void interrupt_at_low_vector(void)
{
    _asm
    goto low_isr
    _endasm
}
#pragma code        //return to the default section of code

#pragma interruptlow low_isr
void low_isr(void)
{ 
            //CODE FOR LOW PRIOIRITY INTERRUPTS
}

/*
 **********************************************************************************************************
 *                               HIGH PRIORITY INTERRUPT
 **********************************************************************************************************
 */
#pragma code high_vector=0x08
void interrupt_at_high_vector(void)
{
    _asm
    goto high_isr
    _endasm
}
#pragma code

#pragma interrupt high_isr
void high_isr(void)
{
    
    INTCONbits.GIE=0;
//    if(INTCONbits.INT0IF==1)
//    {
//       INTCONbits.INT0IF=0;
//       WriteTimer0(26368);
//       set_all_high=1;
//       
//    }
    if(INTCONbits.INT0IF==1)              //RB0 Rising Edge             //HR
    {
        //count_RB++;
        INTCONbits.INT0IF=0;
    }
    if(PIR2bits.TMR3IF==1)              //TMR3 Overflow                 /100ms
    {
        PIR2bits.TMR3IF=0;    //clear interrupt flag bit for timer3
        count_TMR1++;
        TMR_0_COUNTER_10sec++;
        if(count_TMR1_800ms==8)
        {
            count_TMR1_800ms=0;
        }
        count_TMR1_800ms++;   //timer for 800ms for the temperature
        WriteTimer3(52992);
        PIR2bits.TMR3IF=0;    //clear interrupt flag bit for timer3
    }
    if(PIR1bits.CCP1IF==1)      //if capture interrupt is set           //HRV
    {
        overflow_2=overflow_1;
        PIR1bits.CCP1IF=0;      //clear interrupt flag bit
        result_h=CCPR1H;
        result_l=CCPR1L;
        TMR1H=0;
        TMR1L=0;
        overflow_1=0;
        count_RB++;
        TMR_0_COUNTER_10sec=0;

        
        sample++;
    }
    if(PIR1bits.TMR1IF==1)                                              //HRV
    {
        PIR1bits.TMR1IF=0;
        overflow_1++;             
    }
    if(INTCON3bits.INT1F==1)              //RB1 changed states.         //KEYPAD
    {
        read=DATA_AVAILABLE;
        
        INTCON3bits.INT1F=0;
    }
    INTCONbits.GIE=1;
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
 **************************************************************************************
 *                         CALCULATE BPM USING BEATS IN 5 SECONDS
 *************************************
 * Timing 
 * This function has two paths
 * i. Path 1- This path runs the first time the function is called (takes 547 cycles)
 * ii. Path 2- This path runs every other time. This detector which is designed for
 *     a max HR of 240. The worst case scenario for this path is 468 cycles.    
 **************************************************************************************
 */

int BPM(int program_start)           
{

    int BpM=0;
    int count_RB_local=count_RB;
    count_RB=0;
    
    if(program_start==TRUE)
    {
        BpM = count_RB_local*12;
        count_RB_previous=BpM;
    }
    if(program_start==FALSE)
    {
        count_RB_local=count_RB_local*12;
        count_RB_previous=((count_RB_previous+count_RB_local)/2);
        BpM=count_RB_previous;
    }
            return BpM;

}

/*
 **************************************************************************************
 *                              CALCULATE HR VARIABILITY
 * *************************************************************************************
 * This function calculates the HRV of the user using the following logic:
 * i. The value in Timer1 starts to increment whenever a pulse is detected. This is because
 *    the previous value was cleared and Timer1 was reinitialized to 0. Hence as a new pulse 
 *    is detected Timer1 would start to increment and when another pulse is detected the value
 *    is Timer1 would be stored in 
 **************************************************************************************
 */
float calculate_RR_Interval(void)
{
    float time=0.0;
    float overflow_factor=0.0;
    overflow_factor=overflow_2*526.0;
    time=result_h*256.0;
    time=time+result_l;
    time=time*0.008;
    time=time+overflow_factor;
    return time;
}
/*
 **************************************************************************************
 *                             SAMPLES OVER 50
 * *************************************************************************************
 */
int samples_over_50(float *RR_value_float_0_ptr,float *RR_value_float_1_ptr,int *samples_over_50ms_ptr)
{
    float difference=0.0;
    float difference_sqrd=0.0;
    float difference_positive=0.0;
    float RR_value_float[2]={0.0,0.0};  
    int samples_over_50ms=*samples_over_50ms_ptr;
    RR_value_float[0]=*RR_value_float_0_ptr;
    RR_value_float[1]=*RR_value_float_1_ptr;
        switch(sample)
        {
                case 1:
                    RR_value_float[0]=calculate_RR_Interval();   
                    sample++;
                break;
                case 3:
                    RR_value_float[1]=calculate_RR_Interval();
                break;
        }
        if(sample>=3)
        {
            difference=RR_value_float[0]-RR_value_float[1]; 
            difference_sqrd=pow(difference,2); 
            difference_positive=sqrt(difference_sqrd);
            
            RR_value_float[0]=RR_value_float[1]; 
            sample --;

            if(difference_positive>50)
            {
                samples_over_50ms++;
            }
            counter++;
        }
    *RR_value_float_0_ptr=RR_value_float[0];
    *RR_value_float_1_ptr=RR_value_float[1];
    return samples_over_50ms;
}

/*
 **********************************************************************************************************
 *                                          MEASURE HRV
 **********************************************************************************************************
 */
unsigned int meausre_HRV(float *RR_value_float_0_ptr,float *RR_value_float_1_ptr,int *samples_over_50ms_ptr)
{
    int samples_over_50ms=0;


    unsigned int pNN50_int=0;
    

    samples_over_50ms=samples_over_50(RR_value_float_0_ptr,RR_value_float_1_ptr,samples_over_50ms_ptr);
        if(counter==10)
        {
            pNN50_int=((samples_over_50ms*100)/counter);
            counter=0;
            samples_over_50ms=0;
            hrv_ready=1; 
        }
    *samples_over_50ms_ptr=samples_over_50ms;
    return pNN50_int;
}

void STORE_READING(int what_to_store,int *bpm_ptr,int *hrv_ptr,int *gl_ptr)
{
    int bpm=*bpm_ptr;
    int hrv=*hrv_ptr;
    int gl=*gl_ptr;
    switch (what_to_store)
    {
        case STORE_HR:
            MEMORY[memory_loction]=bpm;
            memory_loction++;
            break;
        case STORE_HRV:
            MEMORY[memory_loction]=hrv;
            memory_loction++;
            break;
        case STORE_GL:
            MEMORY[memory_loction]=gl;
            memory_loction++;            
            break;
        case STORE_ALL:
            MEMORY[memory_loction]=bpm;
            memory_loction++;
            MEMORY[memory_loction]=hrv;
            memory_loction++;
            MEMORY[memory_loction]=gl;
            memory_loction++;
            break;        
    }
}

void DISPLAY_MEMORY(int measurement_to_display)
{
    int hr_mem=0;
    int hrv_mem=0;
    int gl_mem=0;

    switch (keypad_press())
    {
        case GO_UP:
            switch (measurement_to_display)
            {
                case DISPLAY_HR:
                    memory_loction--;
                    hr_mem=MEMORY[memory_loction];
                    display_MEM(measurement_to_display,hr_mem,hrv_mem,gl_mem,memory_loction);
                break;
                
                case DISPLAY_HRV:
                    memory_loction--;                    
                    hrv_mem=MEMORY[memory_loction];
                    display_MEM(measurement_to_display,hr_mem,hrv_mem,gl_mem,memory_loction);
                break;
                
                case DISPLAY_GL:
                    memory_loction--;                    
                    gl_mem=MEMORY[memory_loction];
                    display_MEM(measurement_to_display,hr_mem,hrv_mem,gl_mem,memory_loction);
                break; 
                
                case DISPLAY_ALL:
                    memory_display_loction=memory_display_loction-3;
                   
                    hr_mem=MEMORY[memory_display_loction];
                    
                    memory_display_loction++;
                    hrv_mem=MEMORY[memory_display_loction];
                    
                    memory_display_loction++;
                    gl_mem=MEMORY[memory_display_loction];
                    display_MEM(measurement_to_display,hr_mem,hrv_mem,gl_mem,memory_display_loction);
                    memory_display_loction=memory_display_loction-2;
                break; 
            }
        break;
        case GO_DOWN:
    
            switch (measurement_to_display)
            {
                case DISPLAY_HR:
                    memory_loction++;
                    hr_mem=MEMORY[memory_loction];
                    display_MEM(measurement_to_display,hr_mem,hrv_mem,gl_mem,memory_loction);
                break;
                
                case DISPLAY_HRV:
                    memory_loction++;
                    hrv_mem=MEMORY[memory_loction];
                    display_MEM(measurement_to_display,hr_mem,hrv_mem,gl_mem,memory_loction);
                break;
                
                case DISPLAY_GL:
                    memory_loction++;
                    gl_mem=MEMORY[memory_loction];
                    display_MEM(measurement_to_display,hr_mem,hrv_mem,gl_mem,memory_loction);
                break; 
                
                case DISPLAY_ALL:
                    memory_display_loction=memory_display_loction+3;
                   
                    hr_mem=MEMORY[memory_display_loction];
                    
                    memory_display_loction++;
                    hrv_mem=MEMORY[memory_display_loction];
                    
                    memory_display_loction++;
                    gl_mem=MEMORY[memory_display_loction];
                    display_MEM(measurement_to_display,hr_mem,hrv_mem,gl_mem,memory_display_loction);
                    memory_display_loction=memory_display_loction-2;

                break; 
            }
            
        break;   
        default:
            break;
    }    
    
}
/*
 **********************************************************************************************************
 *                                          MAIN
 **********************************************************************************************************
 */
void main(void)
{
    int bpm=0;                                      //HR
    int *bpm_ptr=&bpm;
    char bpm_out[3];
    int program_start=1;
    
        
    char hrv_out[3];                        //HRV
    int samples_over_50ms=0;
    int *samples_over_50ms_ptr=&samples_over_50ms;


    unsigned int pNN50_int=0;
    unsigned int *pNN50_int_ptr=&pNN50_int;
    float RR_value_float[2]={0.0,0.0};
    float *RR_value_float_0_ptr=&RR_value_float[0];
    float *RR_value_float_1_ptr=&RR_value_float[1];
    

    
    unsigned char MS_Byte;
    unsigned char LS_Byte;
    unsigned char *MS_Byte_ptr=&MS_Byte;
    unsigned char *LS_Byte_ptr=&LS_Byte;
    int Decimal_int =0;
    int *Decimal_int_ptr=&Decimal_int;
    unsigned int Temperature_int = 0;
    unsigned int *Temperature_int_ptr = &Temperature_int;
    unsigned char Degree_Symbol = 0xDF;
    int sign = 0;
    int *sign_ptr = &sign;
    char Temperature[20];
    int temp_ready_pole=0;
    int *temp_ready_pole_ptr=&temp_ready_pole;
    int temp_ready=0;

    int dummy_glucose[10]={70,73,93,77,71,95,81,93,73,72};
    char glucose_out[3];                        //HRV
    int glucose_count=0;
    
    char continue_display=0; 
    int measurement_to_store=15;//=0;
    int dumb_glucose=0;
    int *dumb_glucose_ptr=&dumb_glucose;
    int number_of_measurements_to_be_stored=100; //initialsie to 100 so if not specified it will do forever 
    int sample_interval=0;                          //the value can be changed
    int sample_interval_permanant=0;            //this stores the value of the sample interval from the keypad and is not altered
    
    config_LCD();
    while(system_start_up()>0)
    {
    }
    while((measurement_to_store=pick_measurements_to_save())==0)
    {
        //measurement_to_store=pick_measurements_to_save();
    }
    start_up_display_3();
    
    start_up();
    INTCONbits.GIE=1;
    read=0;
    while(1)
    {
        //KEYPAD PRESS
        if(read==DATA_AVAILABLE|number_of_measurements_to_be_stored==0)
        {
            if(keypad_press()==5|keypad_press()==9|number_of_measurements_to_be_stored==0)  //will come here if 5/8 is pressed or amount of samples are taken
            {
                if(number_of_measurements_to_be_stored==0&read==0)
                {
                        while(BusyXLCD());                                     
                        WriteCmdXLCD(0b00000001);
    
                        while(BusyXLCD());
                        SetDDRamAddr( 0x00 );
                        putrsXLCD( "Chosen number of");
                        
                        while(BusyXLCD());
                        SetDDRamAddr( 0x40 );
                        putrsXLCD( "samples taken.");
                        number_of_measurements_to_be_stored=100;
                }
                while(keypad_press()!=12)   //if u press star u will come out of this loop
                {
                   if(read==DATA_AVAILABLE) 
                   {
                      speaker_off(); 
                      DISPLAY_MEMORY(measurement_to_store);
                      read=0;
                      if(display_flag==0)    //this ensures that this only runs when we first switch to display readings 
                      {
                            memory_display_loction=memory_loction;
                            display_flag=1;
                      }
                   }
                }
                    count_RB=0;
                    count_TMR1=0;
                    program_start=1;
            }
            if(keypad_press()==14)  //if key is # then wait for the # of samples
            {
                while(keypad_press()==14)
                {
                        while(BusyXLCD());                                     
                        WriteCmdXLCD(0b00000001);
    
                        while(BusyXLCD());
                        SetDDRamAddr( 0x00 );
                        putrsXLCD( "No. of sample:..");
                        Delay10KTCYx(60);
                }
                number_of_measurements_to_be_stored=keypad_press();
                read=0;
            }
            if(keypad_press()==13)  //if key is 0 then wait for the period in which samples are taken 
            {
                while(keypad_press()==13)
                {
                        while(BusyXLCD());                                     
                        WriteCmdXLCD(0b00000001);
    
                        while(BusyXLCD());
                        SetDDRamAddr( 0x00 );
                        putrsXLCD( "Sample interval:");
                        Delay10KTCYx(60);
                }
                sample_interval_permanant=keypad_press();
                sample_interval=sample_interval_permanant;
                read=0;
            }
        }
    
        //TO ALLOW TEMP TO DISPLAY WITHOUT ANY OTHER INPUT
        if(TMR_0_COUNTER_10sec==100)
        {
            hr_ready=1;
            hrv_ready=1;
            temp_ready=1;
            bpm=0;
            count_RB_previous=0;
            program_start=1;
            continue_display=1;
            count_RB=0;
            count_TMR1=0;
            
        }
        //TEMPERATURE
        if(temp_ready_pole==0)
        {
            start_temperature_conversion(temp_ready_pole_ptr);
            count_TMR1_800ms=0;                 //start the 800ms timer after this initialises the start reading 

        }
        
        if(count_TMR1_800ms==8)
        {
            
           read_temperature_conversion(LS_Byte_ptr,MS_Byte_ptr);

            convert_temperature_reading(LS_Byte_ptr,MS_Byte_ptr,sign_ptr,Temperature_int_ptr,Decimal_int_ptr);
            if(sign == 0)
            {
                Degree_Symbol = 0xDF;
                sprintf(Temperature,"Temp:+%d.%d%cC",Temperature_int,Decimal_int,Degree_Symbol); 
            }
            temp_ready_pole=0;
            count_TMR1_800ms=0;
            temp_ready=1;

        }
        //HEART RATE
        if(count_TMR1>=50)      
        {    
            
            bpm=BPM(program_start);
            count_TMR1=RESTART;
            program_start=0;
            count_TMR1=RESTART;
            hr_ready=1;

        }
        //HEART RATE VARIABILITY
        pNN50_int=meausre_HRV(RR_value_float_0_ptr,RR_value_float_1_ptr,samples_over_50ms_ptr);
        
        //DISPLAY LIVE VALUES
        if(hr_ready==1&hrv_ready>0&temp_ready==1)
        {
            speaker_off();
            if(bpm>120|bpm<60)
            {
               speaker_on(1);
            }
            if(pNN50_int>70)
            {
               speaker_on(2);
            }
            if(dummy_glucose[glucose_count]<70)
            {
               speaker_on(3);
            }
            itoa(bpm,bpm_out);
            itoa(pNN50_int,hrv_out);
            if(continue_display==0)
            {
                itoa(dummy_glucose[glucose_count],glucose_out);
                dumb_glucose=dummy_glucose[glucose_count];
                glucose_count++;
            }
            else
            {
                dumb_glucose=0;
                itoa(0,glucose_out);
                continue_display=0;
            }
            display(bpm_out,hrv_out,Temperature,glucose_out);
            if(sample_interval==0)
            {
                STORE_READING(measurement_to_store,bpm_ptr,pNN50_int_ptr,dumb_glucose_ptr);
                number_of_measurements_to_be_stored--;
                sample_interval=sample_interval_permanant;

            }
            else{
            sample_interval--;
            }
            hr_ready=0;
            hrv_ready=0;
            temp_ready=0;
            TMR_0_COUNTER_10sec=0;

            
            if(glucose_count>9){glucose_count=0;}

        }
        
        
    }
}
