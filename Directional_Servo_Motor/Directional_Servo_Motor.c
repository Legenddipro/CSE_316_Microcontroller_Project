#define F_CPU 8000000UL
#include <avr/io.h>
#include <util/delay.h>

int main(void)
{
    // Set PD5 as output (OC1A → Servo PWM)
    DDRD |= (1 << PD5);

    // Set PA1 as output (activity LED)
    DDRA |= (1 << PA1);

    // Set PB0 and PB1 as input (no pull-ups)
    DDRB &= ~((1 << PB0) | (1 << PB1));

    // ⚠️ DO NOT enable internal pull-ups:
    // PORTB |= (1 << PB0) | (1 << PB1); // <-- This line is removed

    // Timer1 setup for Fast PWM mode (ICR1 as TOP)
    TCNT1 = 0;
    ICR1 = 2499; // 20ms period (50Hz for servo)

    TCCR1A = (1 << WGM11) | (1 << COM1A1);              // Fast PWM, non-inverting mode
    TCCR1B = (1 << WGM13) | (1 << WGM12) | (1 << CS11); // Prescaler = 8

    // Initial servo position
    uint16_t current = 125; // 0°
    OCR1A = current;

    while (1)
    {
        // Read 2-bit input from PB1 (MSB) and PB0 (LSB)
        uint8_t input = PINB & 0x03;

        switch (input)
        {
        case 0b00:
            current = 210; // 0°
            break;
        case 0b01:
            current = 176; // 60° → apple
            break;
        case 0b10:
            current = 260; // ✅ increased for 180° → orange
            break;
        case 0b11:
            current = 210; // 120° → banana
            break;
        }

        OCR1A = current; // Update PWM pulse for servo

        PORTA ^= (1 << PA1); // Toggle LED to indicate loop activity
        _delay_ms(200);      // Allow servo time to move
    }
}
