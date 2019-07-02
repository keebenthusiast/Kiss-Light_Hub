package main

import ( 
       "net"
       "fmt"
       "os"
)

func Toggle( arg string ) {

  /* connect to this socket */
  conn, _ := net.Dial("tcp", "192.168.7.123:1155")

  /* 
   * only works with my device,
   * but I plan to make this support a database
   * of user-added devices, and maybe server-imported
   * devices.
   */
  if ( arg == "on" ) {
    fmt.Fprintf( conn, "TRANSMIT 5592371 189\n" )
    fmt.Println( "Light is turned on." )
  } else if ( arg == "off" ) {
    fmt.Fprintf( conn, "TRANSMIT 5592380 189\n" )
    fmt.Println( "Light is turned off." )
  } else {
    /* quietly exit cleanly */
    fmt.Fprintf( conn, "Q\n" )
    fmt.Println( "Usage: " + os.Args[0] + " on|off" );
    os.Exit( 1 )
  }

  /* quietly exit cleanly */
  fmt.Fprintf( conn, "Q\n" )
}

func main() {

  if ( len( os.Args ) < 2 ) {
    fmt.Println( "Usage: " + os.Args[0] + " on|off" );
    os.Exit( 1 )
  }

  Toggle( os.Args[1] )

}