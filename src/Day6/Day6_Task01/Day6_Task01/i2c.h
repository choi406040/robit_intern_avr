#ifndef I2C_H_
#define I2C_H_

#define I2C_READ  0x01
#define I2C_WRITE 0x00

typedef unsigned char uint8_t;
typedef unsigned int  uint16_t;

void i2c_init(void);
void i2c_start(uint8_t adress);
void i2c_transmit(uint8_t data);
uint8_t i2c_receive_ack(void);
uint8_t i2c_receive_nack(void);
void i2c_transmit_nbytes(uint8_t adress, uint8_t* data, uint16_t length);
void i2c_receive_nbytes(uint8_t adress, uint8_t* data, uint16_t length);
void i2c_stop(void);

#endif
