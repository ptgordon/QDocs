/*
 * qmatrix.c
 *
 * Created: 8/25/2016 1:24:47 PM
 * Author : patto
 */ 

#define F_CPU 2000000

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <inttypes.h>

uint8_t setupcrccheck = 0x00;
uint8_t crccheck = 0x00;
uint8_t crccheckflag = 0x00;
uint8_t *setupsblock;
uint8_t crcout;
uint8_t i;
uint8_t temp;
uint8_t	X = 0X00;
uint8_t last_command = 0x0F;
uint8_t eeprom_crc = 0x0E;
uint8_t enter_setups_mode = 0x01;
uint8_t restart_ic = 0x04;
uint8_t report_first_key = 0x06;
uint8_t report_all_keys = 0x07;
uint8_t general_status = 0x05;
uint8_t fmea_status = 0x0C;
uint8_t key_status = 0x0B;
uint8_t null_command = 0x00;
uint8_t drty;
uint8_t loop_process = 0x00;
uint8_t initialize = 0x00;
uint8_t key_detect = 0x01;
uint8_t error_handle = 0x02;
uint8_t get_status = 0x03;
uint8_t key_report = 0x04;
uint8_t key_location = 0x00;
uint8_t Y0 = 0x00;
uint8_t Y1 = 0x00;
uint8_t Y2 = 0x00;
uint32_t Y = 0x00;
uint8_t error_code = 0x00;

void SPIC0(void);

void SPIC0(void)
{
	SPIC.CTRL	|= 0xD0;					// Enable clkX2, SPI, msb, and Master mode
	SPIC.CTRL	|= SPI_MODE_3_gc;			// mode 2 setup mode
	SPIC.CTRL	|= SPI_PRESCALER_DIV4_gc;	// 2X2M/4 = 1M
	SPIC.INTCTRL|= SPI_INTLVL_HI_gc;		// Select the interrupt level
}

uint8_t eight_bit_crc(uint8_t crc, uint8_t data){  //calculates CRCs
	uint8_t index;
	uint8_t fb;

	index = 8;
	do{
		fb = (crc ^ data) & 0x01;
		data >>= 1;
		crc >>= 1;
			
		if(fb){
			crc ^= 0x8c;
		}
	}while (--index); 
	
	return crc;
}

uint8_t * setupsblockpopulate() {  //values according to "standard settings" in qmatrix datasheet
	
	static uint8_t setups[100];
	int j;

	for (j=0; j<16; j++){
		setups[j] = 0xaa;
	}
	
	for (j=16; j<24; j++){
		setups[j] = 0xa3;
	}
	
	for (j=24; j<28; j++){
		setups[j] = 0x52;
	}
	
	for (j=28; j<32; j++){
		setups[j] = 0x50;
	}
	
	for (j=32; j<37; j++){
		setups[j] = 0x52;
	}
	
	for (j=37; j<40; j++){
		setups[j] = 0x50;
	}
	
	for (j=40; j<43; j++){
		setups[j] = 0x50;
	}
	
	for (j=43; j<48; j++){
		setups[j] = 0x50;
	}
	
	for (j=48; j<72; j++){
		setups[j] = 0x00;
	}

	for (j=72; j<96; j++){
		setups[j] = 0x00;
	}
	j = 96;
	setups[j] = 0x00;
	
	j=97;
	setups[j] = 0x02;

	j=98;
	setups[j] = 0x54;

	j=99;
	setups[j] = 0x00;

	return setups;
}

ISR(SPIC_INT_vect)	// ISR called when SPI finishes with transmitting byte in SPIn.Data register.
{
	cli();

	// if you care about incoming data copy SPIC.DATA into your RX buffER
	PORTC.OUT |= 0x10;
	X = SPIC.DATA;
	sei();
}

void DRDYCHECK(void)   // this pin must be high in order for IC to accept commands
{
	drty=PORTC.IN;
	while((drty & 0x08)!= 0x08)
	{
		_delay_us(10);
		drty=PORTC.IN;
	}
}

void COMMAND(uint8_t command)  //general command format
{
	DRDYCHECK();
	PORTC.OUT &= 0xEF;
	SPIC.DATA = command;
	_delay_us(40);
}

void LAST_COMMAND(void)  //always done after restart or comms error
{
	if(loop_process == initialize){
		while (X != 0xF0)
		{
			COMMAND(0x0F);
		}
		COMMAND(eeprom_crc);
		COMMAND(null_command);
		temp = X;
		if((temp != setupcrccheck) && (crccheckflag == 0x01)){
			loop_process = error_handle;
		}
		else if (temp != setupcrccheck){
			LOAD_SETUPS_BLOCK();
			crccheckflag = 0x01;
		}
		else{
			loop_process = key_detect;
		}
	}
}

void LOAD_SETUPS_BLOCK(void)  //see datasheet for values.  setups block configured earlier in main.
{
		COMMAND(enter_setups_mode);
		COMMAND(enter_setups_mode);
		COMMAND(null_command);
		for(i=0; i<100; i++){
			COMMAND(*(setupsblock + i));
		}
		COMMAND(crccheck);
		COMMAND(null_command);
		COMMAND(restart_ic);
}

void KEY_CHECK(void)
{
	if(loop_process == key_detect){
		PORTB.OUT = 0X02;
		COMMAND(report_first_key);
		COMMAND(null_command);
		temp = X;
		COMMAND(null_command);
		crcout = eight_bit_crc(0x00, report_first_key);
		crccheck = crcout;
		crcout = eight_bit_crc(crccheck, temp);
		if(crcout != X){
			loop_process = error_handle;
		}
		else if((temp & 0x40) == 0x40){
			loop_process = get_status;
		}
		else if((temp & 0x9F) == 0x1F){
			PORTA.OUT = 0x00;
			PORTD.OUT = 0x00;
			PORTF.OUT = 0x00;
			_delay_ms(10);
		}
		else{
			loop_process = key_report;
		}
		
	}
}

void DETECT_REPORT(void)
{
	if(loop_process == key_report){
		if((temp & 0x80) != 0x80){
			key_location = temp & 0x1F;
			Y = 1 << key_location;
			Y0 = Y;
			Y1 = Y>>8;
			Y2 = Y>>16; 
			PORTA.OUT = Y0;
			PORTF.OUT = Y1;
			PORTD.OUT = Y2;
			loop_process = key_detect;
			_delay_ms(10);
		}
		else{
			COMMAND(report_all_keys);
			COMMAND(null_command);
			Y0 = X;
			COMMAND(null_command);
			Y1 = X;
			COMMAND(null_command);
			Y2 = X;
			COMMAND(null_command);
			crcout = eight_bit_crc(0x00, report_all_keys);
			crccheck = crcout;
			crcout = eight_bit_crc(crccheck, Y0);
			crccheck = crcout;
			crcout = eight_bit_crc(crccheck, Y1);
			crccheck = crcout;
			crcout = eight_bit_crc(crccheck, Y2);
			if(crcout != X){
				loop_process = error_handle;
			}
			else{
				PORTA.OUT = Y0;
				PORTF.OUT = Y1;
				PORTD.OUT = Y2;
				loop_process = key_detect;
				_delay_ms(10);
			}
		}
	}
}

void STATUS_CHECK(void)
{
	if(loop_process == get_status){
		COMMAND(general_status);
		COMMAND(null_command);
		temp = X;
		COMMAND(null_command);
		crcout = eight_bit_crc(0x00, general_status);
		crccheck = crcout;
		crcout = eight_bit_crc(crccheck, temp);
		if(crcout != X){
			loop_process = error_handle;
		}
		else if((temp & 0x64) == 0x40){
			while (temp != 0xF0)
			{
				COMMAND(last_command);
			}
			loop_process = key_detect;
		}
		else if((temp & 0x64)!= 0x00){
			loop_process = error_handle;
		}
		else{
			_delay_ms(10);
			loop_process = key_detect;
		}
	}
}

void ERROR(void)
{
	if(loop_process == error_handle){
		crccheckflag = 0x00;
		if((temp & 0x20)==0x20){
			PORTB.OUT = 0x01;
			COMMAND(fmea_status);
			COMMAND(null_command);
			crcout = eight_bit_crc(0x00, fmea_status);
			crccheck = crcout;
			crcout = eight_bit_crc(crccheck, X);
			if(crcout != SPIC.DATA){
				loop_process = error_handle;
			}
			else{
				PORTD.OUT = X;
				loop_process = get_status;
			}
		}
		else if((temp & 0x04) == 0x04){
			PORTB.OUT = 0x02;
			COMMAND(key_status);
			COMMAND(null_command);
			Y0 = SPIC.DATA;
			COMMAND(null_command);
			Y1 = SPIC.DATA;
			COMMAND(null_command);
			Y2 = SPIC.DATA;
			COMMAND(null_command);
			crcout = eight_bit_crc(0x00, key_status);
			crccheck = crcout;
			crcout = eight_bit_crc(crccheck, Y0);
			crccheck = crcout;
			crcout = eight_bit_crc(crccheck, Y1);
			crccheck = crcout;
			crcout = eight_bit_crc(crccheck, Y2);
			crccheck = crcout;
			if(crcout != SPIC.DATA){
				loop_process = error_handle;
			}
			else{
				PORTA.OUT = Y0;
				PORTF.OUT = Y1;
				PORTD.OUT = Y2;
				loop_process = key_detect;
			}
		}
		else{
			loop_process = initialize;
		}
	}
}

int main(void)
{
	setupsblock = setupsblockpopulate();
	for(i=0; i<100; i++){
		crcout = eight_bit_crc(setupcrccheck, *(setupsblock + i));
		setupcrccheck = crcout;
	}
	PORTA.DIR = 0xFF;
	PORTB.DIR = 0xFF;
	PORTD.DIR = 0xFF;
	PORTF.DIR = 0xFF;
	PORTC.DIR = 0xB4;
	PORTC.OUT |= 0x14;
	SPIC0();
	PMIC.CTRL |= PMIC_HILVLEN_bm;
	sei();
	X = 0;
	crccheckflag = 0;
	loop_process = initialize;
	while(1){
		LAST_COMMAND();
		KEY_CHECK();
		DETECT_REPORT();
		STATUS_CHECK();
		ERROR();
	}
}
