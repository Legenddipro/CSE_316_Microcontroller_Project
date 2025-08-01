#define F_CPU 8000000UL    /* Define CPU Frequency e.g. here it's 8MHz */
#include <avr/io.h>        /* Include AVR std. library file */
#include <stdio.h>         /* Include std. library file */
#include <util/delay.h>    /* Include Delay header file */

int main(void)
{
    DDRB = 0b00000000;       // PORTB as input
//  PORTB |= (1 << PB0);     // ✅ Enable pull-up on PB0

    DDRA = 0b11111111;      /* PORTA as output (unchanged) */
    DDRC |= (1 << PC0);     /* ✅ Use PC0 for LED */
    DDRD |= (1 << PD5);     /* Make OC1A (PD5) pin as output */

    TCNT1 = 0;              /* Set timer1 count zero */
    ICR1 = 2499;            /* Set TOP count for timer1 in ICR1 register (20ms period) */

    /* Fast PWM, TOP in ICR1, Clear OC1A on compare match, clk/8 */
    TCCR1A = (1 << WGM11) | (1 << COM1A1);
    TCCR1B = (1 << WGM12) | (1 << WGM13) | (1 << CS11);  // Prescaler 8

    unsigned char status = 0;
    uint16_t current = 187;  // Neutral (90°)
    OCR1A = current;

    while (1)
    {
        status = (PINB & 1);  // Read PB0 only

        if (status == 0) {
            current = 125;    // 0° (1ms pulse)
        } else {
            current = 250;    // 180° (2ms pulse)
        }

        OCR1A = current;

        PORTA = status;       // Output status to PORTA (same as your original)
        PORTC ^= (1 << PC0);  // ✅ Toggle LED on PC0
        _delay_ms(200);
    }
}