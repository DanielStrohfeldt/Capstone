// spi.c
// 
//
//
//
//********************* INCLUDES *************************/

#include <bcm2835.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

//********************* DEFINES  *************************/


//********************* CONSTANTS *************************/
char * READ_CH0_CMD = "110100000000000000000000";
char CH0_CMD[] = { "Ð", "\0", "\0" };
char * READ_CH1_CMD = "111100000000000000000000";
char CH0_CMD[] = { "ð", "\0", "\0" };

int main ( int argc, char ** argv )
{

	char recv_buffer[3];

	bcm2835_init();
	bcm2835_spi_begin();

	/*
	if ( !bcm2835_init() )
	{
		fprintf( stderr, "BCM2835 Library Init Failed : Run As Root.\n" );
		return 1;
	}

	if ( !bcm2835_spi_begin() )
	{
		fprintf( stderr, "BCM2835 Library Failed SPI : Run As Root.\n" );
		return 1;
	}
	*/

	bcm2835_spi_setBitOrder( BCM2835_SPI_BIT_ORDER_MSBFIRST );
	bcm2835_spi_setDataMode( BCM2835_SPI_MODE0 );
	bcm2835_spi_setClockDivider( BCM2835_SPI_CLOCK_DIVIDER_1024 );
	bcm2835_spi_chipSelect( BCM2835_SPI_CS0 );
	bcm2835_spi_setChipSelectPolarity( BCM2835_SPI_CS0, LOW );

    uint16_t x;	
	for ( x = 0; x < 1000; x++ )
	{
	    bcm2835_spi_transfernb( CH0_CMD, recv_buffer, 3 );
	    fprintf( stderr, "Sent Data ( SPI ) : %s \t Read Data ( SPI ) : %s \n", CH0_CMD, recv_buffer );
	}
    
	bcm2835_spi_end();	
	bcm2835_close();
	return 0;

}

