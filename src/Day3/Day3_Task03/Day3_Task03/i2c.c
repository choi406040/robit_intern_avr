#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>

#include "i2c.h"

#define F_SCL     100000UL // SCL 주파수
#define Prescaler 1
#define TWBR_val  (((F_CPU / F_SCL) - 16) / 2)

void i2c_init(void)
{
	PORTD |= (1 << 0) | (1 << 1);
	TWBR = (uint8_t)TWBR_val;
}

void i2c_start(uint8_t adress)
{
	TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
	while (!(TWCR & (1 << TWINT)));

	TWDR = adress;
	TWCR = (1 << TWINT) | (1 << TWEN);

	while (!(TWCR & (1 << TWINT)));
}

void i2c_transmit(uint8_t data)
{
	TWDR = data;
	TWCR = (1 << TWINT) | (1 << TWEN);

	while (!(TWCR & (1 << TWINT)));
}

uint8_t i2c_receive_ack(void)
{
	TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWEA);
	while (!(TWCR & (1 << TWINT)));
	return TWDR;
}

uint8_t i2c_receive_nack(void)
{
	TWCR = (1 << TWINT) | (1 << TWEN);
	while (!(TWCR & (1 << TWINT)));
	return TWDR;
}

void i2c_transmit_nbytes(uint8_t adress, uint8_t* data, uint16_t length)
{
	i2c_start(adress | I2C_WRITE);
	for (uint16_t i = 0; i < length; i++)
	{
		i2c_transmit(data[i]);
		_delay_ms(1);
	}
	i2c_stop();
}

void i2c_receive_nbytes(uint8_t adress, uint8_t* data, uint16_t length)
{
	i2c_start(adress | I2C_READ);
	for (uint16_t i = 0; i < length - 1; i++)
	{
		data[i] = i2c_receive_ack();
	}
	data[(length - 1)] = i2c_receive_nack();
	i2c_stop();
}

void i2c_stop(void)
{
	TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWSTO);
}
