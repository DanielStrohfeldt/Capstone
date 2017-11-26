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

// ******************* DEFINES ***************** //
#define PORT 53211
#define NUM_THREADS 2 

// ******************* PROTOTYPES ************** //
void * run_SERVER( void * id );
void * run_GPIO( void * id );
void * run_WRITER( void * id );
void cleanup( void );
void ctrlc_HANDLER( int signal );

// ******************* GLOBALS ***************** //
char recv_buffer[1024] = {0};
char send_buffer[1024] = {0};
char CH0_CMD[3];
char CH1_CMD[3];
char spi_recv_buffer[3];
sem_t empty_buffer;
sem_t full_buffer;

// ******************* CONSTANTS ************** //
char * READ_CH0_CMD = "11010000";
char * READ_CH1_CMD = "11110000";

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
			sem_post( &empty_buffer ); // Signal sent data
		}
	}

	return NULL;
	
}	

void * run_GPIO( void * id )
{
	int i = 0;	
	bcm2835_init();		// Initialize GPIO
	bcm2835_spi_begin();// Allow SPI Communications

	bcm2835_spi_setBitOrder( BCM2835_SPI_BIT_ORDER_MSBFIRST ); // Setup SPI Bus
	bcm2835_spi_setDataMode( BCM2835_SPI_MODE0 );
	bcm2835_spi_setClockDivider( BCM2835_SPI_CLOCK_DIVIDER_2048 );
	bcm2835_spi_chipSelect( BCM2835_SPI_CS0 );
	bcm2835_spi_setChipSelectPolarity( BCM2835_SPI_CS0, LOW );

	send_buffer[0] = '!';
	while( 1 )
	{
		sem_wait( &empty_buffer ); // Wait on the empty buffer unlock semaphore
		i ++ ;
		// Fill the send buffer with data from GPIO pins
		bcm2835_spi_transfernb( CH0_CMD, spi_recv_buffer, 3 );
		// Copy the return data from the spi channel read into the buffer
		
		// Get data from 2nd AC channel
		bcm2835_spi_transfernb( CH1_CMD, spi_recv_buffer, 3 );
		// Copy the return data from the spi channel read into the buffer
		
		// Check GPIO Pins For Zero Crossings etc
		
		// Copy the return data from GPIO pins into the buffer
		
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
	bcm2835_spi_end(); // End spi communications
	bcm2835_close(); // Close GPIO

}

void ctrlc_HANDLER( int signal )
{
	// Handle an interrupt from control c signal
	cleanup();
	exit(1);

}

