/*
 * Simple client written in Go.
 * But this version works at
 * a local level only.
 *
 * Written By: Christian Kissinger
 */

 package main

 import ( 
        "net"
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
   fmt.Println( "\nAdding/deleting, and listing devices can be done as follows:\n" )
   fmt.Println( "Usage: " + os.Args[0] + " add <device name> ([--manual|-m] <on or off code> <pulse>)" )
   fmt.Println( "                  " + " delete <device name> " )
   fmt.Println( "                  " + " list" )
   fmt.Println( "\nEnter scan mode can be done using the following:\n" )
   fmt.Println( "Usage: " + os.Args[0] + " scan" )
 }
 
 /* Set device on or off */
 func SetDev( conn net.Conn ) {
 
   if ( os.Args[3] == "on" || os.Args[3] == "off" ) {
 
     arg3 := strings.ToUpper( os.Args[3] )
 
     /* Send command */
     fmt.Fprintf( conn, "SET %s %s KL/%.1f\n", os.Args[2], arg3, KLVersion )
 
     /* Read, and parse the response */
     reply, _ := bufio.NewReader( conn ).ReadString( '\n' )
     replyParse := strings.Split( reply, " " )
     statusCode, _ := strconv.ParseInt( replyParse[1], 10, 10 )
 
     /* Give indication on how well the server responded */ 
     if ( statusCode == 200 ) {
 
       fmt.Printf( "Successfully Set Device '%s' %s\n", os.Args[2], os.Args[3] )
 
     } else {
 
       var reason string
 
       if ( statusCode == 406 ) {
 
         reason = "No such device"
       
       } else {
 
         reason = "Internal Server Error on device"        
 
       }
 
       fmt.Printf( "Cannot Set Device '%s' %s, %s\n", os.Args[2], os.Args[3], reason )
 
     }
 
   } else {
 
     fmt.Printf( "Unknown option %s, must pass in \"on\" or \"off\"\n", os.Args[3] )
 
   }
 
 }
 
 /* Toggles a device, given a device name is passed through */
 func Toggle( conn net.Conn ) {
 
   /* Send command */
   fmt.Fprintf( conn, "TOGGLE %s KL/%.1f\n", os.Args[2], KLVersion )
 
   /* Read, and parse the response */
   reply, _ := bufio.NewReader( conn ).ReadString( '\n' )
   replyParse := strings.Split( reply, " " )
   statusCode, _ := strconv.ParseInt( replyParse[1], 10, 10 )
 
   /* Give indication on how well the server responded */ 
   if ( statusCode == 200 ) {
 
     fmt.Printf( "Toggled Device '%s' Successfully\n", os.Args[2] )
 
   } else {
 
     var reason string
 
       if ( statusCode == 406 ) {
 
         reason = "No such device"
       
       } else {
 
         reason = "Internal Server Error on device"        
 
       }
 
     fmt.Printf( "Cannot Toggle Device '%s', %s\n", os.Args[2], reason )
 
   }
 
 }
 
 /* Allows the user to send a custom code entirely */
 func SendCustomCode( conn net.Conn ) {
 
   /* Send command */
   code, _:= strconv.Atoi( os.Args[2] )
   pulse, _ := strconv.Atoi( os.Args[3] )
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
 
 /* similar to sniffForCode, but returns code and pulse respectively */
 func getCode( conn net.Conn ) (int64, int64) {
 
   /* Send command */
   fmt.Fprintf( conn, "SNIFF KL/%.1f\n", KLVersion )
 
   /* Read, and parse the response */
   reply, _ := bufio.NewReader( conn ).ReadString( '\n' )
   replyParse := strings.Split( reply, " " )
   statusCode, _ := strconv.ParseInt( replyParse[1], 10, 10 )
 
   /* Give indication on how well the server responded */ 
   if ( statusCode == 200 ) {
 
     fmt.Printf( "Scanning, please press the desired button\n" )
 
   } else {
 
     fmt.Printf( "Unable to scan, will not add device '%s'\n", os.Args[2] )
     return -1, -1
 
   }
 
   /* Read, and parse the second response */
   reply, _ = bufio.NewReader( conn ).ReadString( '\n' )
   replyParse = strings.Split( reply, " " )
   statusCode, _ = strconv.ParseInt( replyParse[1], 10, 10 )
 
   /* Give indication on how well the server responded the second time */ 
   if ( statusCode == 200 ) {
 
     fmt.Printf( "Scanning successful, attempting to add device '%s'\n", os.Args[2] )
 
     code, _ := strconv.ParseInt( replyParse[3], 10, 24 )
     pulseNum := strings.Split( replyParse[5], "\n" )
     pulse, _ := strconv.ParseInt( pulseNum[0], 10, 10 )
 
     return code, pulse
   
   } else {
 
     fmt.Printf( "Error occurred, likely unknown encoding\n" )
     return -1, -1
 
   }
 }
 
 /* Allows user to add device, optionally manually */
 func AddDev( conn net.Conn ) int {
 
   /* make sure arg count is greater than, or equal to 3 before continuing */
   /* Check if user wants to add device manually. */
 
   if ( len(os.Args) > 3 ) {
 
     if ( os.Args[3] == "--manual" || os.Args[3] == "-m" ) {
 
       code, _ := strconv.ParseInt( os.Args[4], 10, 24 )
       pulse, _ := strconv.ParseInt( os.Args[5], 10, 10 )
 
       /* Send command, assuming code is valid */
       if ( (code & 15) == 12 ) {
 
         fmt.Fprintf( conn, "ADD %s %d %d %d KL/%.1f\n", os.Args[2], (code - 9), code, pulse, KLVersion )
 
       } else if ( (code & 15) == 3 ) {
 
         fmt.Fprintf( conn, "ADD %s %d %d %d KL/%.1f\n", os.Args[2], code, (code + 9), pulse, KLVersion )
 
       } else {
 
         fmt.Printf( "Code %d is invalid, not adding.\n", code )
 
         return 1
 
       }
 
       /* Read, and parse the response */
       reply, _ := bufio.NewReader( conn ).ReadString( '\n' )
       replyParse := strings.Split( reply, " " )
       statusCode, _ := strconv.ParseInt( replyParse[1], 10, 10 )
 
       /* Give indication on how well the server responded */ 
       if ( statusCode == 200 ) {
 
         fmt.Printf( "Added Device '%s' Successfully\n", os.Args[2] )
 
       } else {
 
         fmt.Printf( "Unable to Add Device '%s'\n", os.Args[2] )
 
       }
 
     } else {
 
       code, pulse := getCode( conn )
 
       if ( (code & 15) == 3 ) {
 
         fmt.Fprintf( conn, "ADD %s %d %d %d KL/%.1f\n", os.Args[2], code, (code + 9), pulse, KLVersion )
 
       } else if ( (code & 15) == 12 ) {
 
         fmt.Fprintf( conn, "ADD %s %d %d %d KL/%.1f\n", os.Args[2], (code - 9), code, pulse, KLVersion )
 
       } else {
 
         fmt.Printf( "Code %d is invalid, not adding.\n", code )
         return 1
 
       }
 
       /* Read, and parse the response */
       reply, _ := bufio.NewReader( conn ).ReadString( '\n' )
       replyParse := strings.Split( reply, " " )
       statusCode, _ := strconv.ParseInt( replyParse[1], 10, 10 )
 
       /* Give indication on how well the server responded */ 
       if ( statusCode == 200 ) {
 
         fmt.Printf( "Added Device '%s' Successfully\n", os.Args[2] )
 
       } else {
 
         fmt.Printf( "Unable to Add Device '%s'\n", os.Args[2] )
 
       }
 
     } 
 
   } else if ( len(os.Args) == 3 ) {
 
     code, pulse := getCode( conn )
 
     if ( (code & 15) == 3 ) {
 
       fmt.Fprintf( conn, "ADD %s %d %d %d KL/%.1f\n", os.Args[2], code, (code + 9), pulse, KLVersion )
 
     } else if ( (code & 15) == 12 ) {
 
       fmt.Fprintf( conn, "ADD %s %d %d %d KL/%.1f\n", os.Args[2], (code - 9), code, pulse, KLVersion )
 
     } else {
 
       fmt.Printf( "Code %d is invalid, not adding.\n", code )
       return 1
 
     }
 
     /* Read, and parse the response */
     reply, _ := bufio.NewReader( conn ).ReadString( '\n' )
     replyParse := strings.Split( reply, " " )
     statusCode, _ := strconv.ParseInt( replyParse[1], 10, 10 )
 
     /* Give indication on how well the server responded */ 
     if ( statusCode == 200 ) {
 
       fmt.Printf( "Added Device '%s' Successfully\n", os.Args[2] )
 
     } else {
 
       fmt.Printf( "Unable to Add Device '%s'\n", os.Args[2] )
 
     }
 
   } else {
 
     fmt.Printf( "Unable to Add Device, no device name specified.\n" )
 
   }
 
   return 0
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
 
 /* Allows user to scan for an RF signal */
 func SniffForCode( conn net.Conn ) int {
 
   /* Send command */
   fmt.Fprintf( conn, "SNIFF KL/%.1f\n", KLVersion )
 
   /* Read, and parse the response */
   reply, _ := bufio.NewReader( conn ).ReadString( '\n' )
   replyParse := strings.Split( reply, " " )
   statusCode, _ := strconv.ParseInt( replyParse[1], 10, 10 )
 
   /* Give indication on how well the server responded */ 
   if ( statusCode == 200 ) {
 
     fmt.Printf( "Scanning, please press the desired button\n" )
 
   } else {
 
     fmt.Printf( "Unable to scan\n" )
     return -1
 
   }
 
   /* Read, and parse the second response */
   reply, _ = bufio.NewReader( conn ).ReadString( '\n' )
   replyParse = strings.Split( reply, " " )
   statusCode, _ = strconv.ParseInt( replyParse[1], 10, 10 )
 
   /* Give indication on how well the server responded */ 
   if ( statusCode == 200 ) {
     
     var onOrOff string
     code, _ := strconv.ParseInt( replyParse[3], 10, 24 )
     pulseNum := strings.Split( replyParse[5], "\n" )
     pulse, _ := strconv.ParseInt( pulseNum[0], 10, 10 )
 
     if ( (code & 15) == 3 ) {
 
       onOrOff = "On was scanned."
 
     } else if ( (code & 15) == 12 ) {
 
       onOrOff = "Off was scanned."
 
     } else {
 
       onOrOff = "Code is invalid."
 
     }
 
     fmt.Printf( "Scanning successful, Code=%d, Pulse=%d, %s\n", code, pulse, onOrOff )
 
   } else {
 
     fmt.Printf( "Error occurred, likely unknown encoding\n" )
     return 1
     
   }
 
   return 0
 }
 
 /* Show user a list of readily-available devices */
 func GetList( conn net.Conn ) {
 
   /* Send command */
   fmt.Fprintf( conn, "LIST KL/%.1f\n", KLVersion )
 
   /* Read, and parse the response */
   /*reply, _ := bufio.NewReader( conn ).ReadString( '\n' )
   replyParse := strings.Split( reply, " " )
   statusCode, _ := strconv.ParseInt( replyParse[1], 10, 10 )
   dev := strings.Split( replyParse[5], "\n" )
   devCount, _ := strconv.ParseInt( dev[0], 10, 10 )*/
 
   reply := bufio.NewScanner( conn )
   reply.Scan()
   replyParse := strings.Split( reply.Text(), " " )
   statusCode, _ := strconv.ParseInt( replyParse[1], 10, 10 )
   dev := strings.Split( replyParse[5], "\n" )
   devCount, _ := strconv.ParseInt( dev[0], 10, 10 )
 
   if ( statusCode == 200 ) {
 
     fmt.Printf( "Here is the list:\n" )
     
     for i := int64(0); i < devCount; i++  {
       
       reply.Scan()
       fmt.Printf( "%s\n", reply.Text() )
 
     }
 
   } else {
 
     fmt.Printf( "Error occured, must be on the server's end\n" )
 
   }
 }

func main() {

  /* Check usage count first, print Usage() and exit if insufficient */
  if ( len(os.Args) < 2 ) {
    Usage()
    os.Exit( 1 )
  }

  /* Load the server's ini config */
  cfg, err := ini.Load( "/etc/kisslight.ini" )
  if ( err != nil ) {
    fmt.Printf( "Unable to load ini file: %v", err )
    os.Exit( 1 )
  }

  /* Connect to this socket, and port */
  conn, _ := net.Dial( "tcp", "127.0.0.1:" + 
  strconv.Itoa(cfg.Section("network").Key("port").RangeInt(1155, 1, 65535)) )

  /* Parse first argument */
  if ( os.Args[1] == "set" ) {

    SetDev( conn )

  } else if ( os.Args[1] == "toggle" ) {

    Toggle( conn )

  } else if ( os.Args[1] == "send" ) {

    SendCustomCode( conn )
  
  } else if ( os.Args[1] == "add" ) {

    AddDev( conn )

  } else if ( os.Args[1] == "delete" ) {

    DeleteDev( os.Args[2], conn )

  } else if ( os.Args[1] == "scan" ) {

    SniffForCode( conn )

  } else if ( os.Args[1] == "list" ) {

    GetList( conn )

  } else {

    /* quietly exit cleanly */
    fmt.Fprintf( conn, "Q\n" )
    Usage()
    os.Exit( 1 )

  }

  /* quietly exit cleanly, after all is done */
  fmt.Fprintf( conn, "Q\n" )
}