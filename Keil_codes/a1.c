#include <LPC17xx.h>
#include <RTL.h>
#include <stdio.h>

/* =================================================================
   OPTIMIZED HOSPITAL OT MONITOR (RTOS + SEMAPHORE)
   
   New Feature: Synchronization
   - We use a Semaphore ('sem_data_ready') to signal when new 
     sensor data is available.
   - The Control Task sleeps until the Read Task wakes it up.
   ================================================================= */

// --- SEMAPHORE DECLARATION ---
OS_SEM sem_data_ready; 

void delay_us(int count){ int j; for(j=0;j<count;j++); }
void delay_ms(int ms){ int i,j; for(i=0;i<ms;i++) for(j=0;j<5000;j++); }

/* ---------------- LCD DRIVER ---------------- */
void LCD_PulseEN(void){
    LPC_GPIO4->FIOSET = (1<<28); delay_us(500);
    LPC_GPIO4->FIOCLR = (1<<28);
}
void LCD_SendNibble(uint8_t nib){
    LPC_GPIO1->FIOCLR = 0x00F00000; LPC_GPIO1->FIOSET = (nib & 0x0F)<<20;
    LCD_PulseEN();
}
void LCD_Cmd(uint8_t cmd){
    LPC_GPIO3->FIOCLR = (1<<25)|(1<<26); LCD_SendNibble(cmd>>4); LCD_SendNibble(cmd & 0x0F);
    delay_us(2000);
}
void LCD_Data(uint8_t data){
    LPC_GPIO3->FIOSET = (1<<25); LPC_GPIO3->FIOCLR = (1<<26);
    LCD_SendNibble(data>>4); LCD_SendNibble(data & 0x0F); delay_us(200);
}
void LCD_Goto(uint8_t r,uint8_t c){ LCD_Cmd(0x80 | ((r==0)?0x00:0x40)+c); }
void LCD_Print(char *s){ while(*s) LCD_Data(*s++); }

void LCD_Init(void){
    LPC_PINCON->PINSEL3 &= ~((3<<8)|(3<<10)|(3<<12)|(3<<14));
    LPC_PINCON->PINSEL7 &= ~((3<<18)|(3<<20)); LPC_PINCON->PINSEL9 &= ~(3<<24);
    LPC_GPIO1->FIODIR |= 0x00F00000; LPC_GPIO3->FIODIR |= (1<<25)|(1<<26); LPC_GPIO4->FIODIR |= (1<<28);
    LPC_GPIO3->FIOCLR = (1<<26); delay_ms(50);
    LCD_SendNibble(0x03); delay_ms(5); LCD_SendNibble(0x03); delay_ms(5);
    LCD_SendNibble(0x03); delay_ms(1); LCD_SendNibble(0x02);
    LCD_Cmd(0x28); LCD_Cmd(0x0C); LCD_Cmd(0x06); LCD_Cmd(0x01); delay_ms(10);
}

/* ---------------- GPIO (Active Low) ---------------- */
#define MOTOR_PIN   (1<<1) // P2.1
#define BUZZER_PIN  (1<<2) // P2.2

void GPIO_Init(void){
    LPC_PINCON->PINSEL4 &= ~((3<<2)|(3<<4));
    LPC_GPIO2->FIODIR |= MOTOR_PIN | BUZZER_PIN;
    LPC_GPIO2->FIOSET = MOTOR_PIN | BUZZER_PIN; 
}
void Motor_ON(void)  { LPC_GPIO2->FIOCLR = MOTOR_PIN; }
void Motor_OFF(void) { LPC_GPIO2->FIOSET = MOTOR_PIN; }
void Buzzer_ON(void) { LPC_GPIO2->FIOCLR = BUZZER_PIN; }
void Buzzer_OFF(void){ LPC_GPIO2->FIOSET = BUZZER_PIN; }

/* ---------------- ADC ---------------- */
void ADC_Init(void){
    LPC_SC->PCONP |= (1<<12);
    LPC_PINCON->PINSEL1 &= ~((3<<14)|(3<<16)); LPC_PINCON->PINSEL1 |= ((1<<14)|(1<<16));
    LPC_ADC->ADCR = (4<<8) | (1<<21);
}
uint16_t ADC_Read(uint8_t ch){
    LPC_ADC->ADCR &= ~0xFF; LPC_ADC->ADCR |= (1<<ch) | (1<<24);
    while((LPC_ADC->ADGDR & (1UL<<31))==0);
    return (LPC_ADC->ADGDR>>4) & 0x0FFF;
}

volatile uint16_t mq2_val = 0, mq135_val = 0;
static uint8_t adc_to_pct(uint16_t raw){ return (uint8_t)((raw * 100u) / 4095u); }

/* --- THRESHOLDS --- */
#define MQ2_FAN_START     36u  
#define MQ2_ALARM_START   70u  
#define MQ135_ALARM_START 60u    

/* ---------------- RTOS TASKS ---------------- */

// TASK 1: PRODUCER (Reads Data -> Sends Signal)
__task void Read_Task(void){
    for(;;){
        mq2_val = ADC_Read(0);
        mq135_val = ADC_Read(1);
        
        // SIGNAL: Tell Control Task that new data is ready
        os_sem_send(&sem_data_ready); 
        
        os_dly_wait(10); // Sample every 10 ticks
    }
}

// TASK 2: CONSUMER (Waits for Signal -> Actions)
__task void Control_Task(void){
    uint8_t m2, m135;
    for(;;){
        // WAIT: Sleep here forever until Read_Task sends the signal
        // 0xFFFF means "wait indefinitely"
        os_sem_wait(&sem_data_ready, 0xFFFF);

        // If we are here, new data just arrived!
        m2   = adc_to_pct(mq2_val);
        m135 = adc_to_pct(mq135_val);

        // Logic
        if(m2 > MQ2_FAN_START) Motor_ON();
        else                   Motor_OFF();

        if(m2 >= MQ2_ALARM_START || m135 >= MQ135_ALARM_START) Buzzer_ON();
        else                                                   Buzzer_OFF();
    }
}

// TASK 3: DISPLAY (Updates User Interface)
__task void Display_Task(void){
    char line[17];
    uint8_t p2, p135;
    const char *s2, *s135;
    for(;;){
        p2   = adc_to_pct(mq2_val);
        p135 = adc_to_pct(mq135_val);

        if(p2 <= MQ2_FAN_START)        s2="SAFE";
        else if(p2 < MQ2_ALARM_START)  s2="NORMAL";
        else                           s2="DANGER";

        if(p135 < MQ135_ALARM_START)   s135="NORMAL"; 
        else                           s135="DANGER";

        snprintf(line, 17, "MQ2:%3u%% %-6s", p2, s2);
        LCD_Goto(0,0); LCD_Print(line);
        snprintf(line, 17, "M135:%3u%% %-6s", p135, s135);
        LCD_Goto(1,0); LCD_Print(line);
        
        os_dly_wait(20); // Refresh screen independently
    }
}

__task void Init_Task(void){
    GPIO_Init(); ADC_Init(); LCD_Init();
    
    // 1. Initialize Semaphore with 0 tokens (Locked initially)
    os_sem_init(&sem_data_ready, 0);

    LCD_Goto(0,0); LCD_Print("  OT AIR GUARD  "); 
    LCD_Goto(1,0); LCD_Print(" System Loading ");
    os_dly_wait(200); 
    LCD_Cmd(0x01);    

    os_tsk_create(Read_Task, 3); 
    os_tsk_create(Control_Task, 3); 
    os_tsk_create(Display_Task, 2);
    os_tsk_delete_self();
}

int main(void){ SystemInit(); os_sys_init(Init_Task); while(1); }