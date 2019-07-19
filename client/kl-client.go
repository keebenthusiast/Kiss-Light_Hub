/*
 * Simple client written in Go.
 *
 * Written By: Christian Kissinger
 */

package main

import ( 
       "net"
       //"flag"
       "fmt"
       "os"
       "strconv"
       "strings"
       "bufio"
       "gopkg.in/ini.v1"
)

const KLVersion = 0.1

func Usage() {
  fmt.Println( "Usage: " + os.Args[0] + " set <device name> on|off" )
  fmt.Println( "                  " + " toggle <device name>" )
  fmt.Println( "                  " + " send <code> <pulse>" )
  //fmt.Println( "                  " + " setFan <fan device name> " )
  fmt.Println( "\nAdding/deleting devices can be done as follows:\n" )
  fmt.Println( "Usage: " + os.Args[0] + " add <device name> (--manual --on=On_code|--off=Off_code <pulse>)" )
  fmt.Println( "                  " + " delete <device name> " )
}

/* Set device on or off lmao */
func SetDev( devName string, arg string, conn net.Conn ) {
  fmt.Printf( "Not yet Implemented\n" )
}

/* Toggles a device, given a device name is passed through */
func Toggle( devName string, conn net.Conn ) {

  /* Send command */
  fmt.Fprintf( conn, "TOGGLE %s KL/%.1f\n", devName, KLVersion )

  /* Read, and parse the response */
  reply, _ := bufio.NewReader( conn ).ReadString( '\n' )
  replyParse := strings.Split( reply, " " )
  statusCode, _ := strconv.ParseInt( replyParse[1], 10, 10 )

  /* Give indication on how well the server responded */ 
  if ( statusCode == 200 ) {
    fmt.Printf( "Toggled Device '%s' Successfully\n", devName )
  } else {
    fmt.Printf( "Cannot Toggle Device '%s'\n", devName )
  }

}

/* Allows the user to send a custom code entirely */
func SendCustomCode( args []string, conn net.Conn ) {

  /* Send command */
  code, _:= strconv.Atoi(args[2])
  pulse, _ := strconv.Atoi(args[3])
  fmt.Fprintf( conn, "TRANSMIT %d %d KL/%.1f\n", code, pulse, KLVersion )

  /* Read, and parse the response */
  reply, _ := bufio.NewReader( conn ).ReadString( '\n' )
  replyParse := strings.Split( reply, " " )
  statusCode, _ := strconv.ParseInt( replyParse[1], 10, 10 )

  /* Give indication on how well the server responded */ 
  if ( statusCode == 200 ) {
    fmt.Printf( "Transmitted code:%d pulse:%d Successfully\n", code, pulse )
  } else {
    fmt.Printf( "Unable to transmit custom code and pulse\n" )
  }
}

/* Allows user to add device, optionally manually */
func AddDev( conn net.Conn ) {
  fmt.Printf( "Not yet Implemented\n" )
}

/* Allows user to delete specified device name */
func DeleteDev( devName string, conn net.Conn ) {

  /* Send command */
  fmt.Fprintf( conn, "DELETE %s KL/%.1f\n", devName, KLVersion )

  /* Read, and parse the response */
  reply, _ := bufio.NewReader( conn ).ReadString( '\n' )
  replyParse := strings.Split( reply, " " )
  statusCode, _ := strconv.ParseInt( replyParse[1], 10, 10 )

  /* Give indication on how well the server responded */ 
  if ( statusCode == 200 ) {
    fmt.Printf( "Deleted Device '%s' Successfully\n", devName )
  } else {
    fmt.Printf( "Unable to Delete Device '%s'\n", devName )
  }

}

func main() {

  /* Check usage count first, print Usage() and exit if insufficient */
  if ( len( os.Args ) < 2 ) {
    Usage()
    os.Exit( 1 )
  }

  /* Get user's home varialbe, and find their ini config */
  usr := os.Getenv( "HOME" )
  cfg, err := ini.Load( usr + "/.config/kisslight/kisslight.ini" )
  if ( err != nil ) {
    fmt.Printf( "Unable to load ini file: %v", err )
    os.Exit( 1 )
  }

  /* connect to this socket */
  /*
   * TODO: Figure out how to set default port, and if a port is written
   * and uncommented from the ini file, use that instead.
   */
  conn, _ := net.Dial( "tcp", cfg.Section("network").Key("ipaddr").String() + ":" + 
                       "1155" )


  /* Parse first argument */
  if ( os.Args[1] == "set" ) {

    SetDev( os.Args[2], os.Args[3], conn )

  } else if ( os.Args[1] == "toggle" ) {

    Toggle( os.Args[2], conn )

  } else if ( os.Args[1] == "send" ) {

    SendCustomCode( os.Args, conn )
  
  } else if ( os.Args[1] == "add" ) {

    AddDev( conn )

  } else if ( os.Args[1] == "delete" ) {

    DeleteDev( os.Args[2], conn )

  } else {

    /* quietly exit cleanly */
    fmt.Fprintf( conn, "Q\n" )
    Usage()
    os.Exit( 1 )

  }

  /* quietly exit cleanly, after all is done */
  fmt.Fprintf( conn, "Q\n" )
}