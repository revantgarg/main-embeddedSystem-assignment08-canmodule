#include <stdint.h>
#include "inc/tm4c123gh6pm.h"

#define CAN_ID    0x100
#define PWM_LOAD  15999        /* 16 MHz / 16000 = 1 kHz PWM */
#define T_MIN     20           /* deg C where Motor-2 starts */
#define T_MAX     50           /* deg C for 100 % (DHT11 max) */

/* ------------------------------------------------------------------ */
/* Delay using SysTick : 16 MHz -> 16 ticks per microsecond            */
/* ------------------------------------------------------------------ */
void delay_us(uint32_t us)
{
    if (us == 0) return;
    NVIC_ST_CTRL_R = 0;
    NVIC_ST_RELOAD_R = us * 16 - 1;
    NVIC_ST_CURRENT_R = 0;
    NVIC_ST_CTRL_R = 5;
    while ((NVIC_ST_CTRL_R & 0x10000) == 0);
    NVIC_ST_CTRL_R = 0;
}

void delay_ms(uint32_t ms)
{
    while (ms--) delay_us(1000);
}

void lcd_write(uint8_t v, int rs)
{
    if (rs) GPIO_PORTA_DATA_R |=  0x40;         /* RS = 1 : data    */
    else    GPIO_PORTA_DATA_R &= ~0x40;         /* RS = 0 : command */
    GPIO_PORTB_DATA_R = v;
    GPIO_PORTA_DATA_R |= 0x80;                  /* E = 1 */
    delay_us(1);
    GPIO_PORTA_DATA_R &= ~0x80;                 /* E = 0 */
    delay_us(50);
}

void lcd_cmd(uint8_t c)
{
    lcd_write(c, 0);
    if (c < 4) delay_ms(2);
}

void lcd_data(uint8_t d)
{
    lcd_write(d, 1);
}

void lcd_print(const char *s)
{
    while (*s) lcd_data(*s++);
}

void lcd_num3(int n)
{
    lcd_data('0' + (n / 100) % 10);
    lcd_data('0' + (n / 10) % 10);
    lcd_data('0' + n % 10);
}

void lcd_init(void)
{
    SYSCTL_RCGCGPIO_R |= 0x03;                  /* Port A and Port B */
    while ((SYSCTL_PRGPIO_R & 0x03) != 0x03);

    GPIO_PORTB_AFSEL_R &= ~0xFF;                /* PB0-PB7 : D0-D7 */
    GPIO_PORTB_AMSEL_R &= ~0xFF;
    GPIO_PORTB_DIR_R   |=  0xFF;
    GPIO_PORTB_DEN_R   |=  0xFF;

    GPIO_PORTA_AFSEL_R &= ~0xC0;                /* PA6 = RS, PA7 = E */
    GPIO_PORTA_AMSEL_R &= ~0xC0;
    GPIO_PORTA_DIR_R   |=  0xC0;
    GPIO_PORTA_DEN_R   |=  0xC0;
    GPIO_PORTA_DATA_R  &= ~0xC0;

    delay_ms(20);
    lcd_cmd(0x30); delay_ms(5);
    lcd_cmd(0x30); delay_us(150);
    lcd_cmd(0x30);
    lcd_cmd(0x38);                              /* 8-bit, 2 lines, 5x8 font */
    lcd_cmd(0x0C);                              /* display on, cursor off */
    lcd_cmd(0x06);                              /* auto increment */
    lcd_cmd(0x01);                              /* clear */
}

void dir_init(void)
{
    SYSCTL_RCGCGPIO_R |= 0x01;
    while ((SYSCTL_PRGPIO_R & 0x01) == 0);
    GPIO_PORTA_AFSEL_R &= ~0x3C;
    GPIO_PORTA_AMSEL_R &= ~0x3C;
    GPIO_PORTA_DIR_R   |=  0x3C;
    GPIO_PORTA_DEN_R   |=  0x3C;
    GPIO_PORTA_DATA_R  &= ~0x3C;                /* all low = motors free */
}

void set_dir(int dir1, int dir2)
{
    uint32_t v = 0;
    v |= dir1 ? 0x08 : 0x04;                    /* M1: CW -> IN1=1 (PA2), ACW -> IN2=1 (PA3) */
    v |= dir2 ? 0x20 : 0x10;                    /* M2: CW -> IN3=1 (PA4), ACW -> IN4=1 (PA5) */
    GPIO_PORTA_DATA_R = (GPIO_PORTA_DATA_R & ~0x3C) | v;
}

void pwm_init(void)
{
    SYSCTL_RCGCPWM_R  |= 0x02;                  /* PWM1 */
    SYSCTL_RCGCGPIO_R |= 0x20;                  /* Port F */
    while ((SYSCTL_PRGPIO_R & 0x20) == 0);
    while ((SYSCTL_PRPWM_R & 0x02) == 0);

    SYSCTL_RCC_R &= ~0x00100000;                /* no PWM divider, PWM clk = 16 MHz */

    GPIO_PORTF_AFSEL_R |= 0x0C;
    GPIO_PORTF_PCTL_R   = (GPIO_PORTF_PCTL_R & 0xFFFF00FF) | 0x00005500;
    GPIO_PORTF_AMSEL_R &= ~0x0C;
    GPIO_PORTF_DEN_R   |= 0x0C;

    PWM1_3_CTL_R  = 0;                          /* count down mode */
    PWM1_3_GENA_R = 0x08C;                      /* high on LOAD, low on CMPA down */
    PWM1_3_GENB_R = 0x80C;                      /* high on LOAD, low on CMPB down */
    PWM1_3_LOAD_R = PWM_LOAD;
    PWM1_3_CMPA_R = PWM_LOAD;
    PWM1_3_CMPB_R = PWM_LOAD;
    PWM1_3_CTL_R  = 1;                          /* start generator */
    PWM1_ENABLE_R &= ~0xC0;                     /* outputs off for now */
}

void set_speed1(int d)                          /* Motor-1 duty 0-100 % */
{
    if (d <= 0) { PWM1_ENABLE_R &= ~0x40; return; }
    if (d > 100) d = 100;
    PWM1_3_CMPA_R = PWM_LOAD - (PWM_LOAD * d) / 100;
    PWM1_ENABLE_R |= 0x40;
}

void set_speed2(int d)                          /* Motor-2 duty 0-100 % */
{
    if (d <= 0) { PWM1_ENABLE_R &= ~0x80; return; }
    if (d > 100) d = 100;
    PWM1_3_CMPB_R = PWM_LOAD - (PWM_LOAD * d) / 100;
    PWM1_ENABLE_R |= 0x80;
}
void can_init(void)
{
    SYSCTL_RCGCCAN_R  |= 0x01;
    SYSCTL_RCGCGPIO_R |= 0x10;
    while ((SYSCTL_PRGPIO_R & 0x10) == 0);
    while ((SYSCTL_PRCAN_R & 0x01) == 0);

    GPIO_PORTE_AFSEL_R |= 0x30;
    GPIO_PORTE_PCTL_R   = (GPIO_PORTE_PCTL_R & 0xFF00FFFF) | 0x00880000;
    GPIO_PORTE_AMSEL_R &= ~0x30;
    GPIO_PORTE_DEN_R   |= 0x30;

    CAN0_CTL_R  = 0x41;                         /* INIT = 1, CCE = 1 */
    CAN0_BIT_R  = 0x2B41;                       /* 500 kbps @ 16 MHz */
    CAN0_BRPE_R = 0;

    /* message object 1 : receive ID 0x100 */
    while (CAN0_IF2CRQ_R & 0x8000);
    CAN0_IF2CMSK_R = 0xF0;                      /* WRNRD|MASK|ARB|CONTROL */
    CAN0_IF2MSK1_R = 0;
    CAN0_IF2MSK2_R = 0x1FFC;                    /* compare all 11 ID bits */
    CAN0_IF2ARB1_R = 0;
    CAN0_IF2ARB2_R = 0x8000 | (CAN_ID << 2);    /* MSGVAL, DIR = RX */
    CAN0_IF2MCTL_R = 0x1000 | 0x0080 | 8;       /* UMASK | EOB | DLC */
    CAN0_IF2CRQ_R  = 1;
    while (CAN0_IF2CRQ_R & 0x8000);

    CAN0_CTL_R = 0x00;                          /* leave init -> bus on */
    while (CAN0_CTL_R & 0x01);
}

void can_read(uint8_t *d)                       /* read message object 1 */
{
    uint32_t a1, a2, b1;
    while (CAN0_IF2CRQ_R & 0x8000);
    CAN0_IF2CMSK_R = 0x3F;                      /* read ARB|CONTROL|CLRINTPND|NEWDAT|DATAA|DATAB */
    CAN0_IF2CRQ_R  = 1;
    while (CAN0_IF2CRQ_R & 0x8000);
    a1 = CAN0_IF2DA1_R;
    a2 = CAN0_IF2DA2_R;
    b1 = CAN0_IF2DB1_R;
    d[0] = a1 & 0xFF;  d[1] = (a1 >> 8) & 0xFF;
    d[2] = a2 & 0xFF;  d[3] = (a2 >> 8) & 0xFF;
    d[4] = b1 & 0xFF;
}

void lcd_show(int s1, int dir1, int s2, int dir2)
{
    lcd_cmd(0x80);
    lcd_print("M1:");
    lcd_num3(s1);
    lcd_print("% ");
    lcd_print(dir1 ? "ACW" : "CW ");
    lcd_print("     ");

    lcd_cmd(0xC0);
    lcd_print("M2:");
    lcd_num3(s2);
    lcd_print("% ");
    lcd_print(dir2 ? "ACW" : "CW ");
    lcd_print("     ");
}
int main(void)
{
    uint8_t d[5];
    int pot = 0, temp = 0;
    int dir1 = 0, dir2 = 0;
    int speed1 = 0, speed2 = 0;
    int lost = 255;                             /* start in "no data" state */
    uint32_t tick = 0;

    lcd_init();
    dir_init();
    pwm_init();
    can_init();

    lcd_print("CAN Motor Node");
    delay_ms(1000);
    lcd_cmd(0x01);

    while (1)
    {
        /* --- new frame received? --- */
        if (CAN0_NWDA1_R & 0x01)
        {
            can_read(d);
            pot  = d[0];
            temp = (d[1] << 8) | d[2];
            dir1 = d[3] & 0x01;
            dir2 = (d[3] >> 1) & 0x01;

            speed1 = (pot > 100) ? 100 : pot;

            if (d[3] & 0x04)        speed2 = 0;            /* sensor fault */
            else if (temp <= T_MIN) speed2 = 0;
            else if (temp >= T_MAX) speed2 = 100;
            else speed2 = 30 + (temp - T_MIN) * 70 / (T_MAX - T_MIN);   /* 30-100 % */

            set_dir(dir1, dir2);
            set_speed1(speed1);
            set_speed2(speed2);
            lost = 0;
        }
        else if (lost < 255)
        {
            lost++;
        }

        /* --- no frame for 500 ms -> stop motors --- */
        if (lost > 50)
        {
            set_speed1(0);
            set_speed2(0);
            speed1 = 0;
            speed2 = 0;
        }

        /* --- LCD every 100 ms --- */
        if (tick % 10 == 0)
        {
            if (lost > 50)
            {
                lcd_cmd(0x80); lcd_print("NO CAN DATA     ");
                lcd_cmd(0xC0); lcd_print("Motors stopped  ");
            }
            else
            {
                lcd_show(speed1, dir1, speed2, dir2);
            }
        }

        if (CAN0_STS_R & 0x80) CAN0_CTL_R &= ~0x01;  /* bus-off recovery */

        tick++;
        delay_ms(10);
    }
}
