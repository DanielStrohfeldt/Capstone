// ******************* INCLUDES **************** //
#include <time.h>
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
#define BYTE_MASK_0 0x00
#define BYTE_MASK_1 0x00
#define BYTE_MASK_2 0xFF
#define BYTE_MASK_3 0xF0
#define SPI_BUFFER_SIZE 4
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
void mask_swap_spi_buffer( char * spi_buf );
void spi_to_binary( char * spi_buf );
void copy_spi_buffer( char * spi_buf );
void copy_digital( int pin_val );
// ********************************************* //


// ******************* GLOBALS ***************** //
char recv_buffer[1024] = {0};
char send_buffer[1024] = {0};
char CH0_CMD[4];
char CH1_CMD[4];
char spi_recv_buffer[4];
char quantized_spi_result[16];
sem_t empty_buffer;
sem_t full_buffer;
struct timeval time_now;
uint64_t current_time_microseconds;
uint32_t send_buffer_length;
// ********************************************* //


// ******************* CONSTANTS ************** //
char * READ_CH0_CMD = "11000000";
char * READ_CH1_CMD = "11100000";
char * OUTPUT_FILE = "data_log.csv";
char COMMA = ',';
// ******************************************** //


int main() {

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
	CH0_CMD[3] = '\0';

	temp = strtol( READ_CH1_CMD, 0, 2 );
	CH1_CMD[0] = temp;					 // Read From Channel 1
	CH1_CMD[1] = '\0';
	CH1_CMD[2] = '\0';
	CH1_CMD[3] = '\0';

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
	char dc_voltage[4];
	char dc_current[4];
	char * timestamp;
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
		send_buffer_length = 0;


		// Fill the send buffer with data from GPIO pins
		bcm2835_spi_transfernb( CH0_CMD, spi_recv_buffer, SPI_BUFFER_SIZE );
		mask_swap_spi_buffer( spi_recv_buffer );
		copy_spi_buffer( spi_recv_buffer );
		
		// Check GPIO Pins For Zero Crossings etc
		v_cross = bcm2835_gpio_lev( V_ZERO_CROSS );				// get v zero cross
		i_cross = bcm2835_gpio_lev( I_ZERO_CROSS );				// get i zero cross
		act_low_sig_flt = bcm2835_gpio_lev( ACTIVE_LOW_SIGNAL_FAULT );// get active low
		gettimeofday( &time_now, NULL );
		current_time_microseconds = time_now.tv_sec*(uint64_t)1000000 + time_now.tv_usec;
		// Copy the return data from the spi channel read into the buffer
		


		// Get data from 2nd AC channel
		bcm2835_spi_transfernb( CH1_CMD, spi_recv_buffer, SPI_BUFFER_SIZE );
		mask_swap_spi_buffer( spi_recv_buffer );
		copy_spi_buffer( spi_recv_buffer );
		// Copy the return data from the spi channel read into the buffer
	

		// Check GPIO Pins For Zero Crossings etc
		act_hi_sig_flt = bcm2835_gpio_lev( ACTIVE_LOW_SIGNAL_FAULT );// get active high
		over_v_sig = bcm2835_gpio_lev( OVER_VOLTAGE_SIGNAL );	// get over v signal
		under_v_sig = bcm2835_gpio_lev( UNDER_VOLTAGE_SIGNAL );	// get under v signal
		
		// Copy the return data from GPIO pins into the buffer
		copy_digital( v_cross );
		copy_digital( i_cross );
		copy_digital( act_low_sig_flt );
		copy_digital( act_hi_sig_flt );
		copy_digital( over_v_sig );
		copy_digital( under_v_sig );


		// Print the values of the pin readings
		fprintf( stderr, "Voltage Cross : %d\n", v_cross );
		fprintf( stderr, "Current Cross : %d\n", i_cross );
		fprintf( stderr, "Active Low Signal Fault : %d\n", act_low_sig_flt );
		fprintf( stderr, "Active High Signal Fault : %d\n", act_hi_sig_flt );
		fprintf( stderr, "Over Voltage Signal : %d\n", over_v_sig );
		fprintf( stderr, "Under Voltage Signal : %d\n", under_v_sig );
		fprintf( stderr, "Time : %d\n", current_time_microseconds );	
		// If iteration is 10 Check DC Values

		if ( i == 0 )
		{
			bcm2835_spi_chipSelect( BCM2835_SPI_CS1 ); // Select DC ADC
			
			bcm2835_spi_transfernb( CH0_CMD, spi_recv_buffer, SPI_BUFFER_SIZE );
			// Copy the reutrn data from spi bus into return buffer
			mask_swap_spi_buffer( spi_recv_buffer );
		    strncpy( dc_voltage, spi_recv_buffer, 4 );	
			
			bcm2835_spi_transfernb( CH1_CMD, spi_recv_buffer, SPI_BUFFER_SIZE );
			// Copy the reutrn data from spi bus into return buffer
			mask_swap_spi_buffer( spi_recv_buffer );
		    strncpy( dc_current, spi_recv_buffer, 4 );	
			
			bcm2835_spi_chipSelect( BCM2835_SPI_CS0 ); // Select AC ADC
			i = 10; // reset iteration counter
		}

		copy_spi_buffer( dc_voltage );
		copy_spi_buffer( dc_current );
		
		// Append timestamp to buffer
		
		i --;
		sem_post( &full_buffer ); // Unlock the full_buffer semaphore
	}

	return NULL;

}

void * run_WRITER( void * id )
{
	// Thread for writing CSV Logs
	
	//out_file = fopen( OUTPUT_FILE, "a" );    // Open the file

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

void mask_swap_spi_buffer( char * spi_buf )
{
	char temp;
	spi_buf[0] = spi_buf[0] & BYTE_MASK_0;
	spi_buf[1] = spi_buf[1] & BYTE_MASK_1;
	spi_buf[2] = spi_buf[2] & BYTE_MASK_2;
	spi_buf[3] = spi_buf[3] & BYTE_MASK_3;

	temp = spi_buf[2];			// swap so data is ordered
	spi_buf[2] = spi_buf[3];
	spi_buf[3] = temp;

}

void copy_spi_buffer( char * spi_buf )
{
	uint32_t current_index = send_buffer_length;
	uint32_t x,y;
	char * data;
	
	for ( x = 0; x < SPI_BUFFER_SIZE; x ++ )
	{

		data = char_to_binary( spi_buf[x] );
		if ( x == 0 | x == 1 )
		{
			x = x;
		}
		else
		{

			for ( y = 0; y < CHAR_SIZE; y ++ )
			{
				send_buffer[send_buffer_length] = data[y];
				send_buffer_length ++;
			}
		}
	}

	send_buffer[send_buffer_length] = COMMA;
	send_buffer_length ++;

}

void print_spi_buffer( char * spi_buf )
{
	int x, y;
	char * data;
		
	fprintf( stderr, "\n" );

	for ( x = 0; x < SPI_BUFFER_SIZE; x ++ )
	{

		data = char_to_binary( spi_buf[x] );
		if ( x == 0 | x == 1 )
		{
			x = x;
		}
		else
		{

			for ( y = 0; y < CHAR_SIZE; y ++ )
			{

				fprintf( stderr, "%c", data[y] );

			}
		}
	}
		
	fprintf( stderr, "\n" );

}

char * char_to_binary( char character )
{
	int x;
	char * binary_data = malloc( CHAR_SIZE * sizeof( char ) );
	// LSB of char first MSB of SPI first
	for ( x = 0; x < CHAR_SIZE; x ++ )
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

void spi_to_binary( char * spi_buf )
{
	char lsb = spi_buf[2];
	char msb = spi_buf[3];
	int x;

	for ( x = 0; x < CHAR_SIZE * 2; x ++ )
	{

		if ( x > 7 )
		{
			// Use lsb
			x = x;
		}

		else
		{
			// Use msb
			x = x;
		}

	}

}

void copy_digital( int pin_val )
{
	if ( pin_val )
	{
		
		send_buffer[send_buffer_length] = '1';

	}
	else
	{

		send_buffer[send_buffer_length] = '0';

	}

	send_buffer_length ++;
	send_buffer[send_buffer_length] = COMMA;
	send_buffer_length ++;

}

