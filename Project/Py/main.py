import network

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
with open( 'data_log.csv', 'a' ) as data_log:
    while( 1 ):
        data_log.write( network.connect_test_box.recv(2048) )
        network.connect_test_box.send("1")

