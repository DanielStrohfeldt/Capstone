// ******************* INCLUDES **************** //
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <semaphore.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <bcm2835.h>
// ********************************************* //


// ******************* DEFINES ***************** //
#define PORT 53211
#define NUM_THREADS 2 
#define CHAR_SIZE 8
// ********************************************* //


// ******************* PIN OUT ***************** //
#define ADC_VCC		 				RPI_BPLUS_GPIO_J8_22
#define V_ZERO_CROSS 				RPI_BPLUS_GPIO_J8_11
#define I_ZERO_CROSS 				RPI_BPLUS_GPIO_J8_13
#define ACTIVE_LOW_SIGNAL_FAULT 	RPI_BPLUS_GPIO_J8_35
#define ACTIVE_HIGH_SIGNAL_FAULT 	RPI_BPLUS_GPIO_J8_29
#define OVER_VOLTAGE_SIGNAL 		RPI_BPLUS_GPIO_J8_31
#define UNDER_VOLTAGE_SIGNAL 		RPI_BPLUS_GPIO_J8_33
#define FAULT_RESET_SIGNAL 			RPI_BPLUS_GPIO_J8_32
// ******************************************** //


// ******************* PROTOTYPES ************** //
void * run_SERVER( void * id );
void * run_GPIO( void * id );
void * run_WRITER( void * id );
void cleanup( void );
void ctrlc_HANDLER( int signal );
void print_spi_buffer( char * spi_buf );
char * char_to_binary( char character );
char * mask_spi_buffer( char * spi_buf );
// ********************************************* //


// ******************* GLOBALS ***************** //
char recv_buffer[1024] = {0};
char send_buffer[1024] = {0};
char CH0_CMD[3];
char CH1_CMD[3];
char spi_recv_buffer[3];
char * binary_data = malloc( CHAR_SIZE * sizeof( char ) );
sem_t empty_buffer;
sem_t full_buffer;
// ********************************************* //


// ******************* CONSTANTS ************** //
char * READ_CH0_CMD = "11010000";
char * READ_CH1_CMD = "11110000";
// ******************************************** //


int main()
{
	atexit( cleanup ); // Clean Up Function
	signal( SIGINT, ctrlc_HANDLER );
	sem_init( &empty_buffer, 0, 1 ); // Create Semaphore Lock
	sem_init( &full_buffer, 0, 0 ); // Create Semaphore Lock

	pthread_t threads[NUM_THREADS]; // Create Array of PTHREADS
	int ids[NUM_THREADS]; // Array to Hold Ids of PTHREADS
	int i;

	char temp;
	temp = strtol( READ_CH0_CMD, 0, 2 ); // Initialize SPI Commands
	CH0_CMD[0] = temp;					 // Read From Channel 0
	CH0_CMD[1] = '\0';
	CH0_CMD[2] = '\0';

	temp = strtol( READ_CH1_CMD, 0, 2 );
	CH1_CMD[0] = temp;					 // Read From Channel 1
	CH1_CMD[1] = '\0';
	CH1_CMD[2] = '\0';

	for ( i = 0; i < NUM_THREADS; i++ )
	{
		ids[i] = i;
		if ( i == 0 )
		{
			pthread_create( &threads[i], NULL, run_SERVER, &ids[i] );
		}
		
		if ( i == 1 )
		{
			pthread_create( &threads[i], NULL, run_GPIO, &ids[i] );
		}

		if ( i == 2 )
		{
			pthread_create( &threads[i], NULL, run_WRITER, &ids[i] );
		}
	}

	for ( i = 0; i < NUM_THREADS; i++ )
	{
		pthread_join( threads[i], NULL );
	}

	return 0;

}

void * run_SERVER( void * id )
{
	int server_fd, new_socket, valread;
	struct sockaddr_in address;
	int opt = 1;
	int addrlen = sizeof( address );
	
	char * hello = "Raspberry PI Connected";

	// Creation of Socket File Descriptor
	if (( server_fd = socket( AF_INET, SOCK_STREAM, 0)) == 0 )
	{
		fprintf(stderr, "Socket Failed to Open\n");
		exit(1);
	}

	// Attach Socket to Port 53211
	if ( setsockopt( server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt) ))
	{
		fprintf(stderr, "Socket Failed to Attach\n");
		exit(1);
	}

	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons( PORT );

	if ( bind( server_fd, ( struct sockaddr * ) &address, sizeof( address ) ) < 0 )
	{
		fprintf(stderr, "Socket Failed to Bind\n");
		exit(1);
	}

	if ( listen( server_fd, 3 ) < 0 )
	{
		fprintf(stderr, "Socket Failed to Bind\n");
		exit(1);
	}

	if (( new_socket = accept( server_fd, ( struct sockaddr * ) &address, ( socklen_t * ) &addrlen )) < 0 )
	{
		fprintf(stderr, "Socket Failed to Accept\n");
		exit(1);
	}


	valread = read( new_socket, recv_buffer, 1024 );
	printf( "%s\n", recv_buffer );

	if ( strcmp( recv_buffer, "Remote Desktop Connected" ) == 0 )
	{
		send( new_socket, hello, strlen(hello), 0 );
		while( 1 )
		{
			sem_wait( &full_buffer ); // Wait until IO operations have completed
	
			send( new_socket, send_buffer, strlen( send_buffer ), 0 ); // send buffer
			read( new_socket, recv_buffer, 1024 );

			sem_post( &empty_buffer ); // Signal sent data
		}
	}

	return NULL;
	
}	

void * run_GPIO( void * id )
{
	// ****************** VARIABLE SETUP ********** //
	int i = 0;
	uint8_t v_cross, i_cross, act_low_sig_flt, act_hi_sig_flt;
	uint8_t over_v_sig, under_v_sig;
	char * received_buffer;
	// ******************************************** //


	// ****************** GPIO SETUP *************** //
	bcm2835_init();				// Initialize GPIO
	bcm2835_spi_begin();		// Allow SPI Communications
	// ********************************************* //


	// ***************** 5V ADC SUPPLY ************* //
	bcm2835_gpio_fsel( ADC_VCC, BCM2835_GPIO_FSEL_OUTP );
	bcm2835_gpio_write( ADC_VCC, HIGH );
	// ********************************************* //


	// ***************** SIGNAL MONITOR SETUP  ************** //
	bcm2835_gpio_fsel( V_ZERO_CROSS, BCM2835_GPIO_FSEL_INPT );
	bcm2835_gpio_set_pud( V_ZERO_CROSS, BCM2835_GPIO_PUD_DOWN );		// turn off
	bcm2835_gpio_fsel( I_ZERO_CROSS, BCM2835_GPIO_FSEL_INPT );
	bcm2835_gpio_set_pud( I_ZERO_CROSS, BCM2835_GPIO_PUD_DOWN );		// turn off
	bcm2835_gpio_fsel( ACTIVE_LOW_SIGNAL_FAULT, BCM2835_GPIO_FSEL_INPT );
	bcm2835_gpio_set_pud( ACTIVE_LOW_SIGNAL_FAULT, BCM2835_GPIO_PUD_DOWN );
	bcm2835_gpio_fsel( ACTIVE_HIGH_SIGNAL_FAULT, BCM2835_GPIO_FSEL_INPT );
	bcm2835_gpio_set_pud( ACTIVE_HIGH_SIGNAL_FAULT, BCM2835_GPIO_PUD_DOWN );
	bcm2835_gpio_fsel( OVER_VOLTAGE_SIGNAL, BCM2835_GPIO_FSEL_INPT );
	bcm2835_gpio_set_pud( OVER_VOLTAGE_SIGNAL, BCM2835_GPIO_PUD_DOWN );
	bcm2835_gpio_fsel( UNDER_VOLTAGE_SIGNAL, BCM2835_GPIO_FSEL_INPT );
	bcm2835_gpio_set_pud( UNDER_VOLTAGE_SIGNAL, BCM2835_GPIO_PUD_DOWN );
	bcm2835_gpio_fsel( FAULT_RESET_SIGNAL, BCM2835_GPIO_FSEL_OUTP );
	// ****************************************************** // 


	// ****************** SPI SETUP ***************** //
	bcm2835_spi_setBitOrder( BCM2835_SPI_BIT_ORDER_MSBFIRST ); // Setup SPI Bus
	bcm2835_spi_setDataMode( BCM2835_SPI_MODE0 );
	bcm2835_spi_setClockDivider( BCM2835_SPI_CLOCK_DIVIDER_2048 );
	bcm2835_spi_chipSelect( BCM2835_SPI_CS0 );
	bcm2835_spi_setChipSelectPolarity( BCM2835_SPI_CS0, LOW );
	// ********************************************** //



	send_buffer[0] = '!';
	while( 1 )
	{
		sem_wait( &empty_buffer ); // Wait on the empty buffer unlock semaphore
		i ++ ;


		// Fill the send buffer with data from GPIO pins
		bcm2835_spi_transfernb( CH0_CMD, spi_recv_buffer, 3 );
		received_buffer = mask_spi_buffer( spi_recv_buffer );
		print_spi_buffer( received_buffer );
		
		// Check GPIO Pins For Zero Crossings etc
		v_cross = bcm2835_gpio_lev( V_ZERO_CROSS );				// get v zero cross
		i_cross = bcm2835_gpio_lev( I_ZERO_CROSS );				// get i zero cross
		act_low_sig_flt = bcm2835_gpio_lev( ACTIVE_LOW_SIGNAL_FAULT );// get active low

		// Copy the return data from the spi channel read into the buffer
		


		// Get data from 2nd AC channel
		bcm2835_spi_transfernb( CH1_CMD, spi_recv_buffer, 3 );
		received_buffer = mask_spi_buffer( spi_recv_buffer );
		print_spi_buffer( received_buffer );
		// Copy the return data from the spi channel read into the buffer
	

		// Check GPIO Pins For Zero Crossings etc
		act_hi_sig_flt = bcm2835_gpio_lev( ACTIVE_LOW_SIGNAL_FAULT );// get active high
		over_v_sig = bcm2835_gpio_lev( OVER_VOLTAGE_SIGNAL );	// get over v signal
		under_v_sig = bcm2835_gpio_lev( UNDER_VOLTAGE_SIGNAL );	// get under v signal
		
		// Copy the return data from GPIO pins into the buffer

		// Print the values of the pin readings
		fprintf( stderr, "Voltage Cross : %d\n", v_cross );
		fprintf( stderr, "Current Cross : %d\n", i_cross );
		fprintf( stderr, "Active Low Signal Fault : %d\n", act_low_sig_flt );
		fprintf( stderr, "Active High Signal Fault : %d\n", act_hi_sig_flt );
		fprintf( stderr, "Over Voltage Signal : %d\n", over_v_sig );
		fprintf( stderr, "Under Voltage Signal : %d\n", under_v_sig );
			
		// If iteration is 10 Check DC Values

		if ( i == 10 )
		{
			bcm2835_spi_chipSelect( BCM2835_SPI_CS1 ); // Select DC ADC
			
			bcm2835_spi_transfernb( CH0_CMD, spi_recv_buffer, 3 );
			// Copy the reutrn data from spi bus into return buffer
			
			bcm2835_spi_transfernb( CH1_CMD, spi_recv_buffer, 3 );
			// Copy the reutrn data from spi bus into return buffer
			
			bcm2835_spi_chipSelect( BCM2835_SPI_CS0 ); // Select AC ADC
			i = 0; // reset iteration counter
		}

		// memcpy( &send_buffer, &spi_recv_buffer, 3 );
		
		// Append timestamp to buffer
		
		sem_post( &full_buffer ); // Unlock the full_buffer semaphore
	}

	return NULL;

}

void * run_WRITER( void * id )
{
	// Thread for writing CSV Logs
	sleep(1);
	return NULL;

}

void cleanup( void )
{

	fprintf( stderr, "Cleaning Up\n" );
	bcm2835_gpio_write( ADC_VCC, LOW );
	bcm2835_spi_end(); // End spi communications
	bcm2835_close(); // Close GPIO

}

void ctrlc_HANDLER( int signal )
{
	// Handle an interrupt from control c signal
	cleanup();
	exit(1);

}

char * mask_spi_buffer( char * spi_buf )
{

	char * new_buffer = malloc( 3 * sizeof( char ) );
	char mask_byte_0 = 0x1F;	// 00011111
	char mask_byte_1 = 0xFC;	// 11111100
	char mask_byte_2 = 0x00;	// 00000000

	new_buffer[0] = spi_buf[0] & mask_byte_0;
	new_buffer[1] = spi_buf[1] & mask_byte_1;
	new_buffer[2] = spi_buf[2] & mask_byte_2;

	return new_buffer;

}

void print_spi_buffer( char * spi_buf )
{
	int spi_buf_size = 3;			// length of char buffer
	int char_size = 8;
	int x, y;
	char * data;
		
	fprintf( stderr, "\n" );

	for ( x = 0; x < spi_buf_size; x ++ )
	{

		data = char_to_binary( spi_buf[x] );

		for ( y = 0; y < char_size; y ++ )
		{

			fprintf( stderr, "%c", data[y] );

		}
		
		fprintf( stderr, "\n" );
	}
		
	fprintf( stderr, "\n" );

}

char * char_to_binary( char character )
{
	int char_size = 8;
	int x;

	for ( x = 0; x < char_size; x ++ )
	{

		if ( character >> x & 1 )
		{	
			binary_data[x] = '1';
		}
		else
		{
			binary_data[x] = '0';
		}

	}

	return binary_data;

}
