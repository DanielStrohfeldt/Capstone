import network
import atexit

def cleanup():
    ''' Function to close the data files that are open when exit occurs'''
    data_log.close()
    data_log_temp.close()

############################ CONSTANTS ###############################################
COMMA = ','                     # For appending to the string received via wifi
ENTRY_LENGTH = 12               # Length of expected data transferred from the socket
FAULT_DETECTED = 0              # faults detected = 0
AC_V_SCALE = 1.0
AC_I_SCALE = 1.0
DC_V_SCALE = 1.0
DC_I_SCALE = 1.0
############################ VARIABLES FOR ITERATION ##################################
############################ VARIABLES FOR ITERATION ##################################
MAX_AC_VOLTAGE_DETECTED = 0
MAX_AC_CURRENT_DETECTED = 0
MAX_DC_VOLTAGE_DETECTED = 0
MAX_DC_CURRENT_DETECTED = 0
AVERAGE_FREQUENCY = 0
CURRENT_ZERO_CROSS = -1 
PREV_ZERO_CROSS = -1
CURRENT_TIMESTAMP = -1
PREV_TIMESTAMP = -1
atexit.register( cleanup )

print( """
          __  ___      ____  ___    ___  ____    __  ___
         / / / / | /| / /  |/  /___/ _ \/ __ \  /  |/  /
        / /_/ /| |/ |/ / /|_/ /___/ ___/ /_/ / / /|_/ / 
        \____/ |__/|__/_/  /_/   /_/   \___\_\/_/  /_/  
                                                          
                                                           """ )


test_box_ip_address = "" 
while len( test_box_ip_address.split( '.' ) ) != 4:
    test_box_ip_address = raw_input ( "Enter the IP address of the test device : " )
    if len( test_box_ip_address.split( '.' ) ) != 4:
        print ( "Invalid IP Address" ) 

print( "Test Box IP Address {}".format( test_box_ip_address ))

try:
    network.connect( test_box_ip_address, network.connect_test_box )
except Exception as e:
    print( type(e) )
    print( e.args )
    print( e )
    print( "Couldn't Connect to Test Device" )
    exit(1);

network.connect_test_box.send( "Remote Desktop Connected" )
received_data = network.connect_test_box.recv(2048)

if( received_data == "Raspberry Pi Connected"):

    data_log_temp = open( 'temp_log.csv', 'w' )
    temp_list = []
    
    while( 1 ):
    
        received_data = network.connect_test_box.recv(2048)
        
        try: 
            received_data = unicode( received_data, "utf-8" ) 
        except UnicodeDecodeError:
            received_data = '\n'

        if ( received_data == "Transfer Complete" ):
            freq_list = [] 
            CURRENT_ZERO_CROSS = -1
            PREV_ZERO_CROSS = -1
            CURRENT_TIMESTAMP = -1
            PREV_TIMESTAMP = -1
            print( "Data Transfer Complete, Creating New Log" ) 
            data_log_temp.close()
            data_log_temp = open( 'temp_log.csv', 'r' )
            data_log = open( 'data_log.csv', 'a' )
            
            for line in data_log_temp.readlines():
                line = line.split(',')
                line[0] = str( int( line[0], 2 ) )          # convert AC Voltage data from binary to integer
                line[2] = str( int( line[2], 2 ) )          # convert AC Current data from binary to integer
                line[4] = str( int( line[4], 2 ) )          # convert DC Voltage data from binary to integer
                line[5] = str( int( line[5], 2 ) )          # convert DC Current data from binary to integer
                #print( "{} : Length {}".format( line, len(line)) )

                if( len( line ) == 12 ):                    # filter bad data reads
                   line[11] = '\n'                          # append a new line character to the end of the data
                   line = COMMA.join( line )                # join data into a string to be written to file
                   data_log.write( line )

                   CURRENT_ZERO_CROSS = line[1]
                  
                   if( CURRENT_ZERO_CROSS != PREV_ZERO_CROSS ):

                       if( PREV_ZERO_CROSS != -1 ): 
                           PREV_ZERO_CROSS = CURRENT_ZERO_CROSS
                           freq_list.append( 1 / ((CURRENT_TIME_STAMP - PREV_TIME_STAMP) * 10 ** -6) )
                           PREV_TIME_STAMP = CURRENT_TIME_STAMP
                       else:
                           PREV_ZERO_CROSS = CURRENT_ZERO_CROSS
                           PREV_TIME_STAMP = CURRENT_TIME_STAMP

                   if( line[0] > MAX_AC_VOLTAGE ):
                       MAX_AC_VOLTAGE = line[0]

                   if( line[2] > MAX_AC_CURRENT ):
                       MAX_AC_CURRENT = line[2]

                   if( line[3] > MAX_DC_VOLTAGE ):
                       MAX_DC_VOLTAGE = line[3]

                   if( line[4] > MAX_DC_CURRENT ):
                       MAX_DC_CURRENT = line[4]

                   if( line[6] == 1 ):
                       print( "Fault Detected {} : Timestamp {}".format( "Active Low Fault", line[10] ))
                       FAULT_DETECTED = 1

                   if( line[7] == 1 ):
                       print( "Fault Detected {} : Timestamp {}".format( "Active High Fault", line[10] ))
                       FAULT_DETECTED = 1
 
                   if( line[8] == 1 ):
                       print( "Fault Detected {} : Timestamp {}".format( "Over Voltage", line[10] ))
                       FAULT_DETECTED = 1
                   
                   if( line[9] == 1 ):
                       print( "Fault Detected {} : Timestamp {}".format( "Under Voltage", line[10] ))
                       FAULT_DETECTED = 1
 
            if ( FAULT_DETECTED == 0 ):
                print( "No Faults Detected in Data" )

            # CALC EVERYTHING
            MAX_AC_VOLTAGE = ( MAX_AC_VOLTAGE * 5.0 / 4095 ) * AC_V_SCALE
            MAX_AC_CURRENT = ( MAX_AC_CURRENT * 5.0 / 4095 ) * AC_I_SCALE
            MAX_DC_VOLTAGE = ( MAX_DC_VOLTAGE * 5.0 / 4095 ) * DC_V_SCALE
            MAX_DC_CURRENT = ( MAX_DC_CURRENT * 5.0 / 4095 ) * DC_I_SCALE
            average_frequency = sum( i for i in freq_list ) * 1.0 / len(freq_list)
            
            # PRINT EVERYTHING 
            print( "MAX AC Voltage : {}\t MAX AC Current : {}".format( MAX_AC_VOLTAGE, MAX_AC_CURRENT ))
            print( "MAX DC Voltage : {}\t MAX DC Current : {}".format( MAX_DC_VOLTAGE, MAX_DC_CURRENT )) 
            print( "Average Frequency : {}".format( average_frequency ))

            data_log_temp.close()
            data_log.close()
            data_log_temp = open( 'temp_log.csv', 'w' )
            FAULT_DETECTED = 0
            MAX_AC_VOLTAGE = 0
            MAX_AC_CURRENT = 0
            MAX_DC_VOLTAGE = 0
            MAX_DC_CURRENT = 0
            print( "Receiving Data Over Network Socket" )

        else:
            data_log_temp.write( received_data )
                 
        network.connect_test_box.send("1")

