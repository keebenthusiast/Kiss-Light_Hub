/*
 * Simple kiss-light CLI client written in Go.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2019-2021, Christian Kissinger
 * kiss-light Hub is released under the New BSD license (see LICENSE).
 * Go to the project repo here:
 * https://gitlab.com/kiss-light-project/Kiss-Light_Hub
 *
 * Written by: Christian Kissinger
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

const (
  KLVersion = 0.3
	BUFFER_SIZE = 2048
)

func Usage() {
  fmt.Println( "To change states of existing devices:\n" )
  fmt.Println( "usage: " + os.Args[0] + " set <device name> on|off" )
  fmt.Println( "                  " + " toggle <device name>" )
  fmt.Println( "                  " + " send <topic> <command>" )
  fmt.Println( "\nAdding/deleting, status, and listing of devices:\n" )
  fmt.Println( "usage: " + os.Args[0] + " add <device name> <mqtt_topic> <dev_type>" )
  fmt.Println( "                  " + " delete <device name> " )
  fmt.Println( "                  " + " status <device name>" )
  fmt.Println( "                  " + " list" )
  fmt.Println( "\nUpdating server IP address, or port:\n" )
  fmt.Println( "usage: " + os.Args[0] + " ip <IP address>" )
  fmt.Println( "                  " + " port <port number>" )
}

/* Set device on or off */
func SetDev( conn net.Conn ) {

  if ( len(os.Args) > 3 ) {

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

        fmt.Printf( "successfully set device %s %s\n", os.Args[2], os.Args[3] )

      } else {

        fmt.Printf( "cannot set device %s %s, server returned %d\n",
                    os.Args[2], os.Args[3], statusCode )

      }

    } else {

      fmt.Printf( "unknown option %s, must pass in \"on\" or \"off\"\n", os.Args[3] )

    }

  } else {

    fmt.Printf( "not enough args\n" );
    fmt.Printf( "usage: %s set <device name> on|off\n", os.Args[0] )

  }

}

/* Toggles a device, given a device name is passed through */
func Toggle( conn net.Conn ) {

  if ( len(os.Args) > 2 ) {

    /* Send command */
    fmt.Fprintf( conn, "TOGGLE %s KL/%.1f\n", os.Args[2], KLVersion )

    /* Read, and parse the response */
    reply, _ := bufio.NewReader( conn ).ReadString( '\n' )
    replyParse := strings.Split( reply, " " )
    statusCode, _ := strconv.ParseInt( replyParse[1], 10, 10 )

    /* Give indication on how well the server responded */
    if ( statusCode == 200 ) {

      fmt.Printf( "toggled device %s successfully\n", os.Args[2] )

    } else {

      fmt.Printf( "cannot toggle device %s, server returned %d\n",
                  os.Args[2], statusCode )

    }

  } else {

    fmt.Printf( "pass in a device name to toggle\n" );
    fmt.Printf( "Usage: %s toggle <dev_name>\n", os.Args[0] );

  }

}

/* Allows the user to send a custom code entirely */
func SendCustomCode( conn net.Conn ) {

  if ( len(os.Args) > 3 ) {

    /* Send command */
    fmt.Fprintf( conn, "TRANSMIT %s %s KL/%.1f\n", os.Args[2], os.Args[3], KLVersion )

    /* Read, and parse the response */
    reply, _ := bufio.NewReader( conn ).ReadString( '\n' )
    replyParse := strings.Split( reply, " " )
    statusCode, _ := strconv.ParseInt( replyParse[1], 10, 10 )

    /* Give indication on how well the server responded */
    if ( statusCode == 200 ) {

      fmt.Printf( "transmitted %s %s successfully\n", os.Args[2], os.Args[3] )

    } else {

      fmt.Printf( "unable to transmit custom topic %s with command %s, server returned %d\n",
                  os.Args[2], os.Args[3], statusCode )
    }

  } else {

    fmt.Printf( "not enough args\n" );
    fmt.Printf( "usage: %s set <device name> on|off\n", os.Args[0] )

  }
}

/* Allows user to add device, optionally manually */
func AddDev( conn net.Conn ) int {

  /* make sure arg count is greater than, or equal to 3 before continuing */
  /* Check if user wants to add device manually. */

  if ( len(os.Args) > 4 ) {

      /* Send command */
      dev_type, _ := strconv.Atoi( os.Args[4] )

      fmt.Fprintf( conn, "ADD %s %s %d KL/%.1f\n", os.Args[2], os.Args[3], dev_type, KLVersion )

      /* Read, and parse the response */
      reply, _ := bufio.NewReader( conn ).ReadString( '\n' )
      replyParse := strings.Split( reply, " " )
      statusCode, _ := strconv.ParseInt( replyParse[1], 10, 10 )

      /* Give indication on how well the server responded */
      if ( statusCode == 200 ) {

        fmt.Printf( "added Device %s successfully\n", os.Args[2] )

      } else {

        fmt.Printf( "unable to add device %s, server returned %d\n", os.Args[2], statusCode )

      }

    } else {

      fmt.Printf( "cannot add, not enough arguments\n" );
      fmt.Printf( "usage: %s add <device name> <mqtt_topic> <dev_type>\n", os.Args[0] )
    }

  return 0
}

/* Allows user to delete specified device name */
func DeleteDev( conn net.Conn ) {

  if ( len(os.Args) > 2) {

    /* Send command */
    fmt.Fprintf( conn, "DELETE %s KL/%.1f\n", os.Args[2], KLVersion )

    /* Read, and parse the response */
    reply, _ := bufio.NewReader( conn ).ReadString( '\n' )
    replyParse := strings.Split( reply, " " )
    statusCode, _ := strconv.ParseInt( replyParse[1], 10, 10 )

    /* Give indication on how well the server responded */
    if ( statusCode == 200 ) {

      fmt.Printf( "deleted device %s successfully\n", os.Args[2] )

    } else {

      fmt.Printf( "unable to delete device %s, server returned %d\n",
                  os.Args[2], statusCode )

    }

  } else {

    fmt.Printf( "pass in a device name to delete\n" );
    fmt.Printf( "Usage: %s delete <dev_name>\n", os.Args[0] );

  }

}

/* Show user a list of readily-available devices */
func GetList( conn net.Conn ) {

  /* Send command */
  fmt.Fprintf( conn, "LIST KL/%.1f\n", KLVersion )

  /* Read, and parse the response */
  reply := bufio.NewScanner( conn )
  reply.Scan()
  replyParse := strings.Split( reply.Text(), " " )
  statusCode, _ := strconv.ParseInt( replyParse[1], 10, 10 )
  dev := strings.Split( replyParse[5], "\n" )
  devCount, _ := strconv.ParseInt( dev[0], 10, 10 )

  if ( statusCode == 200 ) {

    fmt.Printf( "here is the list of %d devices:\n", devCount )
    fmt.Printf( "(device name -- mqtt topic -- device type)\n" )

    for i := int64(0); i < devCount; i++  {

      reply.Scan()
      fmt.Printf( "%s\n", reply.Text() )

    }

  } else {

    fmt.Printf( "error occured while listing, server returned %d\n", statusCode )

  }
}

func getStatus( conn net.Conn ){

  if ( len(os.Args) > 2 ) {

    fmt.Fprintf( conn, "STATUS %s KL/%.1f", os.Args[2], KLVersion )

    /* Read, and parse the response */
    reply, _ := bufio.NewReader( conn ).ReadString( '\n' )
    replyParse := strings.Split( reply, " " )
    statusCode, _ := strconv.ParseInt( replyParse[1], 10, 10 )

    if ( statusCode == 200 || statusCode == 401 ) {

      fmt.Printf( "device %s state is currently %s\n", os.Args[2], replyParse[5] )

      if ( statusCode == 401 ) {

        fmt.Printf( "server returned response code %d\n", statusCode )

      }

    } else {

      fmt.Printf( "unable get device status for %s, server returned %d\n", os.Args[2], statusCode )

    }
  }
}

func main() {

  /* Check usage count first, print Usage() and exit if insufficient */
  if ( len(os.Args) < 2 ) {
    Usage()
    os.Exit( 1 )
  }

  /* Get user's home variablee, and find their ini config */
  usr := os.Getenv( "HOME" )
  cfg, err := ini.Load( usr + "/.config/kisslight/kl-client.ini" )
  if ( err != nil ) {
    fmt.Printf( "unable to load ini file: %v", err )
    os.Exit( 1 )
  }

  /* Check if there is any ini file modifications to be made */
  if ( os.Args[1] == "ip" ) {

    if ( len(os.Args) > 2 ) {

      cfg.Section( "network" ).Key( "ipaddr" ).SetValue( os.Args[2] );
      cfg.SaveTo( usr + "/.config/kisslight/kl-client.ini" );

      fmt.Printf( "IP address %s set\n", os.Args[2] );

    } else {

      fmt.Printf( "please specify IP address\n" );
      fmt.Printf( "usage: %s ip <IP address>\n", os.Args[0] );

      os.Exit( 1 )
    }

    os.Exit( 0 )

  } else if ( os.Args[1] == "port" ) {

    if ( len(os.Args) > 2 ) {

      port, _ := strconv.Atoi( os.Args[2] )

      if ( port > 65535 || port < 1 ) {

        fmt.Printf( "port number should be between 1 and 65535\n" );
        fmt.Printf( "usage: %s port <port number>\n", os.Args[0] );

        os.Exit( 1 )

      }

      cfg.Section( "network" ).Key( "port" ).SetValue( strconv.Itoa( port ) )
      cfg.SaveTo( usr + "/.config/kisslight/kl-client.ini" )

      fmt.Printf( "port %d set\n", port );

      } else {

        fmt.Printf( "please specify port number\n" );
        fmt.Printf( "usage: %s port <port number>\n", os.Args[0] );

        os.Exit( 1 )
      }

    os.Exit( 0 )

  }

  /* connect to this socket, and port */
  conn, _ := net.Dial( "tcp", cfg.Section("network").Key("ipaddr").String() + ":" +
                        strconv.Itoa(cfg.Section("network").Key("port").RangeInt(1155, 1, 65535)) )

  /* ignore socket closing errors. */
  defer conn.Close()

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

    DeleteDev( conn )

  }  else if ( os.Args[1] == "list" ) {

    GetList( conn )

  } else if ( os.Args[1] == "status" ) {

    getStatus( conn )

  } else {

    /* quietly exit cleanly */
    fmt.Fprintf( conn, "Q\n" )
    Usage()
    os.Exit( 1 )

  }

  /* quietly exit cleanly, after all is done */
  fmt.Fprintf( conn, "Q\n" )
}
