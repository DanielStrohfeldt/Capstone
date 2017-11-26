import network

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
print( network.connect_test_box.recv(1024) )


