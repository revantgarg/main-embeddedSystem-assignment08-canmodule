#include <stdint.h>
#include "inc/tm4c123gh6pm.h"

#define CAN_ID   0x100
#define DHT_PIN  0x04          /* PD2 */

void delay_us(uint32_t us)
{
    if (us == 0) return;
    NVIC_ST_CTRL_R = 0;
    NVIC_ST_RELOAD_R = us * 16 - 1;
    NVIC_ST_CURRENT_R = 0;
    NVIC_ST_CTRL_R = 5;                         /* enable, system clock */
    while ((NVIC_ST_CTRL_R & 0x10000) == 0);    /* wait for COUNT flag  */
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
    if (c < 4) delay_ms(2);                     /* clear / home are slow */
}

void lcd_data(uint8_t d)
{
    lcd_write(d, 1);
}

void lcd_print(const char *s)
{
    while (*s) lcd_data(*s++);
}

void lcd_num3(int n)                            /* prints 000 - 999 */
{
    lcd_data('0' + (n / 100) % 10);
    lcd_data('0' + (n / 10) % 10);
    lcd_data('0' + n % 10);
}

void lcd_init(void)
{
    SYSCTL_RCGCGPIO_R |= 0x03;                  /* Port A and Port B clock */
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
void adc_init(void)
{
    SYSCTL_RCGCADC_R  |= 0x01;
    SYSCTL_RCGCGPIO_R |= 0x10;                  /* Port E */
    while ((SYSCTL_PRGPIO_R & 0x10) == 0);
    while ((SYSCTL_PRADC_R & 0x01) == 0);

    GPIO_PORTE_DIR_R   &= ~0x08;
    GPIO_PORTE_AFSEL_R |=  0x08;
    GPIO_PORTE_DEN_R   &= ~0x08;
    GPIO_PORTE_AMSEL_R |=  0x08;                /* PE3 analog */

    ADC0_ACTSS_R &= ~0x08;                      /* disable SS3 */
    ADC0_EMUX_R  &= ~0xF000;                    /* software trigger */
    ADC0_SSMUX3_R = 0;                          /* AIN0 */
    ADC0_SSCTL3_R = 0x06;                       /* END0 | IE0 */
    ADC0_ACTSS_R |= 0x08;                       /* enable SS3 */
}

uint32_t adc_read(void)
{
    ADC0_PSSI_R = 0x08;                         /* start SS3 */
    while ((ADC0_RIS_R & 0x08) == 0);
    uint32_t v = ADC0_SSFIFO3_R & 0xFFF;
    ADC0_ISC_R = 0x08;
    return v;
}
void switch_init(void)
{
    SYSCTL_RCGCGPIO_R |= 0x20;                  /* Port F */
    while ((SYSCTL_PRGPIO_R & 0x20) == 0);
    GPIO_PORTF_LOCK_R = 0x4C4F434B;             /* unlock PF0 */
    GPIO_PORTF_CR_R  |= 0x01;
    GPIO_PORTF_DIR_R &= ~0x11;
    GPIO_PORTF_PUR_R |=  0x11;
    GPIO_PORTF_DEN_R |=  0x11;
}
void dht_init(void)
{
    SYSCTL_RCGCGPIO_R |= 0x08;                  /* Port D */
    while ((SYSCTL_PRGPIO_R & 0x08) == 0);
    GPIO_PORTD_AFSEL_R &= ~DHT_PIN;
    GPIO_PORTD_AMSEL_R &= ~DHT_PIN;
    GPIO_PORTD_PUR_R   |=  DHT_PIN;             /* backup pull-up */
    GPIO_PORTD_DATA_R  &= ~DHT_PIN;             /* output value = 0 */
    GPIO_PORTD_DIR_R   &= ~DHT_PIN;             /* released (high) */
    GPIO_PORTD_DEN_R   |=  DHT_PIN;
}

void dht_low(void)                              /* output first, then write 0 */
{
    GPIO_PORTD_DIR_R  |=  DHT_PIN;
    GPIO_PORTD_DATA_R &= ~DHT_PIN;
}
void dht_release(void) { GPIO_PORTD_DIR_R &= ~DHT_PIN; }
int  dht_pin(void)     { return (GPIO_PORTD_DATA_R & DHT_PIN) != 0; }

int dht_wait(int level)                         /* wait for pin = level, 0 on timeout */
{
    int t = 200;
    while (dht_pin() != level)
    {
        if (--t == 0) return 0;
        delay_us(1);
    }
    return 1;
}

int dht_read(int *temp)                         /* 0 = OK, 1-5 = error stage */
{
    uint8_t b[5] = {0, 0, 0, 0, 0};
    int i;

    dht_low();                                  /* start signal: low >= 18 ms */
    delay_ms(20);
    dht_release();

    if (!dht_wait(0)) return 1;                 /* E1: no response at all */
    if (!dht_wait(1)) return 2;                 /* E2: stuck low */
    if (!dht_wait(0)) return 3;                 /* E3: stuck high */

    for (i = 0; i < 40; i++)
    {
        if (!dht_wait(1)) return 4;             /* E4: bit timing */
        delay_us(40);                           /* '0' = 26-28 us high, '1' = 70 us high */
        b[i / 8] <<= 1;
        if (dht_pin())
        {
            b[i / 8] |= 1;
            if (!dht_wait(0)) return 4;
        }
    }

    if (((b[0] + b[1] + b[2] + b[3]) & 0xFF) != b[4]) return 5;   /* E5: checksum */
    *temp = b[2];                               /* integer deg C */
    return 0;
}

/* ------------------------------------------------------------------ */
/* CAN0 on PE4 (RX) / PE5 (TX), 500 kbps                               */
/* ------------------------------------------------------------------ */
void can_init(void)
{
    SYSCTL_RCGCCAN_R  |= 0x01;
    SYSCTL_RCGCGPIO_R |= 0x10;                  /* Port E */
    while ((SYSCTL_PRGPIO_R & 0x10) == 0);
    while ((SYSCTL_PRCAN_R & 0x01) == 0);

    GPIO_PORTE_AFSEL_R |= 0x30;
    GPIO_PORTE_PCTL_R   = (GPIO_PORTE_PCTL_R & 0xFF00FFFF) | 0x00880000;
    GPIO_PORTE_AMSEL_R &= ~0x30;
    GPIO_PORTE_DEN_R   |= 0x30;

    CAN0_CTL_R  = 0x41;                         /* INIT = 1, CCE = 1 */
    /* 16 MHz, prescaler 2 -> 16 TQ per bit = 500 kbps
       TSEG1 = 12, TSEG2 = 3, SJW = 2 (sample point ~81%) */
    CAN0_BIT_R  = 0x2B41;
    CAN0_BRPE_R = 0;
    CAN0_CTL_R  = 0x00;                         /* leave init -> bus on */
    while (CAN0_CTL_R & 0x01);
}

void can_send(uint8_t *d)                       /* message object 1, 5 bytes */
{
    while (CAN0_IF1CRQ_R & 0x8000);             /* wait IF1 not busy */
    CAN0_IF1CMSK_R = 0xB3;                      /* WRNRD|ARB|CONTROL|DATAA|DATAB */
    CAN0_IF1ARB1_R = 0;
    CAN0_IF1ARB2_R = 0x8000 | 0x2000 | (CAN_ID << 2);   /* MSGVAL, DIR = TX */
    CAN0_IF1MCTL_R = 0x0100 | 0x0080 | 5;       /* TXRQST | EOB | DLC = 5 */
    CAN0_IF1DA1_R  = d[0] | (d[1] << 8);
    CAN0_IF1DA2_R  = d[2] | (d[3] << 8);
    CAN0_IF1DB1_R  = d[4];
    CAN0_IF1DB2_R  = 0;
    CAN0_IF1CRQ_R  = 1;                         /* write to object 1 and send */
}

/* ------------------------------------------------------------------ */
/* LCD screen                                                          */
/* Line 1: T:025*C P:067%                                              */
/* Line 2: S1:CW  S2:ACW                                               */
/* ------------------------------------------------------------------ */
void lcd_show(int temp, int fault, int pot, int dir1, int dir2)
{
    lcd_cmd(0x80);
    lcd_print("T:");
    if (fault) { lcd_data('E'); lcd_data('0' + fault); lcd_print("  "); }
    else { lcd_num3(temp); lcd_data(0xDF); }    /* 0xDF = degree symbol */
    lcd_print("C P:");
    lcd_num3(pot);
    lcd_print("%  ");

    lcd_cmd(0xC0);
    lcd_print("S1:");
    lcd_print(dir1 ? "ACW" : "CW ");
    lcd_print(" S2:");
    lcd_print(dir2 ? "ACW" : "CW ");
    lcd_print("   ");
}

/* ------------------------------------------------------------------ */
int main(void)
{
    uint8_t frame[5];
    uint8_t count = 0;
    int pot = 0, temp = 0, t;
    int fault = 0;
    int dir1 = 0, dir2 = 0;
    int sw1, sw2, prev1 = 0, prev2 = 0;
    uint32_t tick = 0;

    lcd_init();
    adc_init();
    switch_init();
    dht_init();
    can_init();

    lcd_print("CAN Sensor Node");
    delay_ms(1000);
    lcd_cmd(0x01);

    delay_ms(1000);                             /* DHT11 needs 1 s after power-up */

    while (1)
    {
        /* --- switches, checked every 10 ms (acts as debounce) --- */
        sw1 = ((GPIO_PORTF_DATA_R & 0x10) == 0);
        sw2 = ((GPIO_PORTF_DATA_R & 0x01) == 0);
        if (sw1 && !prev1) dir1 ^= 1;           /* toggle on each press */
        if (sw2 && !prev2) dir2 ^= 1;
        prev1 = sw1;
        prev2 = sw2;

        /* --- temperature every 2 s (DHT11 max 1 reading per second) --- */
        if (tick % 200 == 0)
        {
            fault = dht_read(&t);               /* 0 = OK, 1-5 = error code */
            if (fault == 0) temp = t;
        }

        /* --- every 100 ms: pot, send frame, update LCD --- */
        if (tick % 10 == 0)
        {
            pot = adc_read() * 100 / 4095;

            frame[0] = pot;
            frame[1] = (temp >> 8) & 0xFF;
            frame[2] = temp & 0xFF;
            frame[3] = (dir1 & 1) | ((dir2 & 1) << 1) | ((fault ? 1 : 0) << 2);
            frame[4] = count++;
            can_send(frame);

            lcd_show(temp, fault, pot, dir1, dir2);
        }

        if (CAN0_STS_R & 0x80) CAN0_CTL_R &= ~0x01;  /* bus-off recovery */

        tick++;
        delay_ms(10);
    }
}
