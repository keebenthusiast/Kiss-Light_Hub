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
  fmt.Println( "To change states of existing devices:" )
  fmt.Println( "usage: " + os.Args[0] + " set <device name> <command>" +
               " <arg>" )
  fmt.Println( "                " + " toggle <device name>" )
  fmt.Println( "                " + " send <topic> <command>" )
  fmt.Println( "\nAdding/deleting, status, and listing of devices:" )
  fmt.Println( "usage: " + os.Args[0] + " add <device name> <mqtt_topic>" +
               " <device type> (<valid_commands>)" )
  fmt.Println( "                " + "(<valid_commands>) for custom or " +
               "powerstrip devices." )
  fmt.Println( "                " + " delete <device name>" )
  fmt.Println( "                " + " status <device name>" )
  fmt.Println( "                " + " list" )
  fmt.Println( "\nvalid device types:" )
  fmt.Println( "[0|outlet], [1|strip], [2|dim], [3|cct], [4|rgb]" +
               "[5|rgbw], [6|rgbcct], [7|custom]" )
  fmt.Println( "\nUpdating device name, mqtt topic, or state:" )
  fmt.Println( "usage: " + os.Args[0] + " update name <device name>" +
               " <new device name>" )
  fmt.Println( "                " + " update topic <device name>" +
               " <new mqtt topic>" )
  fmt.Println( "                " + " update state <device name>" )
  fmt.Println( "\nUpdating server IP address, or port:" )
  fmt.Println( "usage: " + os.Args[0] + " ip <IP address>" )
  fmt.Println( "                " + " port <port number>" )
}

/* Set device on or off */
func SetDev( conn net.Conn ) {

  if ( len(os.Args) > 4 ) {

    arg3 := strings.ToUpper( os.Args[3] )
    arg4 := strings.ToUpper( os.Args[4] )

    /* Send command */
    fmt.Fprintf( conn, "SET %s %s %s KL/%.1f\n", os.Args[2], arg3,
                arg4, KLVersion )

    /* Read, and parse the response */
    reply, _ := bufio.NewReader( conn ).ReadString( '\n' )
    replyParse := strings.Split( reply, " " )
    statusCode, _ := strconv.ParseInt( replyParse[1], 10, 10 )

    /* Give indication on how well the server responded */
    if ( statusCode == 201 ) {

      fmt.Printf( "successfully set device %s %s\n", os.Args[2], os.Args[3] )

    } else {

      fmt.Printf( "cannot set device %s %s, server returned %d\n",
                  os.Args[2], os.Args[3], statusCode )
    }

  } else {

    fmt.Printf( "not enough args\n" )
    fmt.Printf( "usage: %s set <device name> <command> <arg>\n", os.Args[0] )
  }
}

/* Toggles specified device power, given a device name is passed through */
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

      fmt.Printf( "toggled device %s power successfully\n", os.Args[2] )

    } else {

      fmt.Printf( "cannot toggle device %s power, server returned %d\n",
                  os.Args[2], statusCode )
    }

  } else {

    fmt.Printf( "pass in a device name to toggle its power\n" )
    fmt.Printf( "usage: %s toggle <dev_name>\n", os.Args[0] )
  }
}

/* Allows the user to send a custom code entirely */
func SendCustomCode( conn net.Conn ) {

  if ( len(os.Args) > 3 ) {

    /* Send command */
    fmt.Fprintf( conn, "TRANSMIT %s %s KL/%.1f\n", os.Args[2], os.Args[3],
                 KLVersion )

    /* Read, and parse the response */
    reply, _ := bufio.NewReader( conn ).ReadString( '\n' )
    replyParse := strings.Split( reply, " " )
    statusCode, _ := strconv.ParseInt( replyParse[1], 10, 10 )

    /* Give indication on how well the server responded */
    if ( statusCode == 205 ) {

      fmt.Printf( "transmitted %s %s successfully\n", os.Args[2], os.Args[3] )

    } else {

      fmt.Printf( "unable to transmit custom topic %s with command %s, " +
                  "server returned %d\n", os.Args[2], os.Args[3], statusCode )
    }

  } else {

    fmt.Printf( "not enough args\n" )
    fmt.Printf( "usage: %s send <topic> <command>\n", os.Args[0] )
  }
}

/* Allows user to add device, optionally manually */
func AddDev( conn net.Conn ) int {

  /* make sure arg count is greater than, or equal to 4 before continuing */
  /* Check if user wants to add device manually. */
  if ( len(os.Args) > 4 ) {

      /* Verify input */
      switch {

        case strings.EqualFold(os.Args[4], "1"),
             strings.EqualFold(os.Args[4], "powerstrip"),
             strings.EqualFold(os.Args[4], "strip"),
             strings.EqualFold(os.Args[4], "7"),
             strings.EqualFold(os.Args[4], "custom"):

             /* verify arg len for this type of device */
             if ( len(os.Args) == 5 ) {

              fmt.Printf( "cannot add, not enough arguments\n" )
              fmt.Printf( "usage: %s add <device name> <mqtt_topic> %s %s",
                          os.Args[0], "<device type>", "(<valid_commands>)\n" )
              fmt.Printf( "(<valid_commands>) for custom or powerstrip %s\n",
                    "devices." )
              fmt.Println( "\nvalid device types:" )
              fmt.Println( "[0|outlet], [1|strip], [2|dim], [3|cct], [4|rgb]" +
                           "[5|rgbw], [6|rgbcct], [7|custom]" )

              return 1
             }

             /*determine exact device type as an integer */
             devType := -1

             switch {
              case strings.EqualFold(os.Args[4], "1"),
                   strings.EqualFold(os.Args[4], "powerstrip"),
                   strings.EqualFold(os.Args[4], "strip"):
                   devType = 1

              case strings.EqualFold(os.Args[4], "7"),
                   strings.EqualFold(os.Args[4], "custom"):
                   devType = 7
             }

             /* Send command */
             fmt.Fprintf( conn, "ADD %s %s %d %s KL/%.1f\n", os.Args[2],
                          os.Args[3], devType, os.Args[5], KLVersion )

        case strings.EqualFold(os.Args[4], "0"),
             strings.EqualFold(os.Args[4], "outlet"),
             strings.EqualFold(os.Args[4], "toggleable"),
             strings.EqualFold(os.Args[4], "2"),
             strings.EqualFold(os.Args[4], "dim"),
             strings.EqualFold(os.Args[4], "dimmable"),
             strings.EqualFold(os.Args[4], "dimmablebulb"),
             strings.EqualFold(os.Args[4], "3"),
             strings.EqualFold(os.Args[4], "cct"),
             strings.EqualFold(os.Args[4], "cctbulb"),
             strings.EqualFold(os.Args[4], "4"),
             strings.EqualFold(os.Args[4], "rgb"),
             strings.EqualFold(os.Args[4], "rgbbulb"),
             strings.EqualFold(os.Args[4], "5"),
             strings.EqualFold(os.Args[4], "rgbw"),
             strings.EqualFold(os.Args[4], "rgbwbulb"),
             strings.EqualFold(os.Args[4], "6"),
             strings.EqualFold(os.Args[4], "rgbcct"),
             strings.EqualFold(os.Args[4], "rgbcctbulb"):

             /*determine exact device type as an integer */
             devType := -1

             switch {
               case strings.EqualFold(os.Args[4], "0"),
                    strings.EqualFold(os.Args[4], "outlet"),
                    strings.EqualFold(os.Args[4], "toggleable"):
                    devType = 0

               case strings.EqualFold(os.Args[4], "2"),
                    strings.EqualFold(os.Args[4], "dim"),
                    strings.EqualFold(os.Args[4], "dimmable"),
                    strings.EqualFold(os.Args[4], "dimmablebulb"):
                    devType = 2

               case strings.EqualFold(os.Args[4], "3"),
                    strings.EqualFold(os.Args[4], "cct"),
                    strings.EqualFold(os.Args[4], "cctbulb"):
                    devType = 3

               case strings.EqualFold(os.Args[4], "4"),
                    strings.EqualFold(os.Args[4], "rgb"),
                    strings.EqualFold(os.Args[4], "rgbbulb"):
                    devType = 4

               case strings.EqualFold(os.Args[4], "5"),
                    strings.EqualFold(os.Args[4], "rgbw"),
                    strings.EqualFold(os.Args[4], "rgbwbulb"):
                    devType = 5

               case strings.EqualFold(os.Args[4], "6"),
                    strings.EqualFold(os.Args[4], "rgbcct"),
                    strings.EqualFold(os.Args[4], "rgbcctbulb"):
                    devType = 6
             }

            /* Send command */
             fmt.Fprintf( conn, "ADD %s %s %d KL/%.1f\n", os.Args[2],
                          os.Args[3], devType, KLVersion )

        default:
          fmt.Printf( "cannot add, invalid device type %s\n", os.Args[4] )
          fmt.Printf( "usage: %s add <device name> <mqtt_topic> %s %s",
              os.Args[0], "<device type>", "(<valid_commands>)\n" )
          fmt.Printf( "(<valid_commands>) for custom or powerstrip %s\n",
              "devices." )
          fmt.Println( "\nvalid device types:" )
          fmt.Println( "[0|outlet], [1|strip], [2|dim], [3|cct], [4|rgb]" +
                       "[5|rgbw], [6|rgbcct], [7|custom]" )

          return 1
      }

      /* Read, and parse the response */
      reply, _ := bufio.NewReader( conn ).ReadString( '\n' )
      replyParse := strings.Split( reply, " " )
      statusCode, _ := strconv.ParseInt( replyParse[1], 10, 10 )

      /* Give indication on how well the server responded */
      if ( statusCode == 202 ) {

        fmt.Printf( "added device %s successfully\n", os.Args[2] )

      } else {

        fmt.Printf( "unable to add device %s, server returned %d\n",
                    os.Args[2], statusCode )
      }

    } else {

      fmt.Printf( "cannot add, not enough arguments\n" )
      fmt.Printf( "usage: %s add <device name> <mqtt_topic> <device type> %s",
                  os.Args[0], "(<valid_commands>)\n" )
      fmt.Printf( "(<valid_commands>) for custom or powerstrip devices.\n" )
      fmt.Println( "\nvalid device types:" )
      fmt.Println( "[0|outlet], [1|strip], [2|dim], [3|cct], [4|rgb]" +
                   "[5|rgbw], [6|rgbcct], [7|custom]" )
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
    if ( statusCode == 203 ) {

      fmt.Printf( "deleted device %s successfully\n", os.Args[2] )

    } else {

      fmt.Printf( "unable to delete device %s, server returned %d\n",
                  os.Args[2], statusCode )
    }

  } else {

    fmt.Printf( "pass in a device name to delete\n" )
    fmt.Printf( "Usage: %s delete <dev_name>\n", os.Args[0] )

  }
}

/* Show user a list of readily-available devices */
func GetList( conn net.Conn ) {

  /* Send command */
  fmt.Fprintf( conn, "LIST KL/%.1f\n", KLVersion )

  /* Read, and parse the response */
  reader := bufio.NewReader( conn )
  reply, _, _ := reader.ReadLine()
  replyParse := strings.Split( string(reply), " " )
  statusCode, _ := strconv.ParseInt( replyParse[1], 10, 10 )
  dev := strings.Split( replyParse[5], "\n" )
  devCount, _ := strconv.ParseInt( dev[0], 10, 10 )

  if ( statusCode == 204 ) {

    fmt.Printf( "here is the list of %d devices:\n", devCount )
    fmt.Printf( "(device name -- mqtt topic -- device type)\n" )

    for {

      reply, _, _ = reader.ReadLine()

      terminatingChar := string(reply)

      /* Check if it is the terminating character */
      if ( strings.EqualFold(terminatingChar, ".") ) {

        break
      }

      fmt.Println( string(reply) )
    }

  } else {

    fmt.Printf( "error occured while listing, server returned %d\n",
                statusCode )
  }
}

/* Get status of the device of interest */
func GetStatus( conn net.Conn ){

  if ( len(os.Args) > 2 ) {

    fmt.Fprintf( conn, "STATUS %s KL/%.1f", os.Args[2], KLVersion )

    /* Read, and parse the response */
    reader := bufio.NewReader( conn )
    reply, _, _ := reader.ReadLine()
    replyParse := strings.Split( string(reply), " " )
    statusCode, _ := strconv.ParseInt( replyParse[1], 10, 10 )

    if ( statusCode == 206 ) {

      fmt.Printf( "device %s state is currently:\n", os.Args[2] )

      for {

        reply, _, _ = reader.ReadLine()

        terminatingChar := string(reply)

        /* Check if it is the terminating character */
        if ( strings.EqualFold(terminatingChar, ".") ) {

          break
        }

        fmt.Println( string(reply) )
      }

    } else if ( statusCode == 401 ) {

      fmt.Printf( "server returned response code %d\n", statusCode )

    } else {

      fmt.Printf( "unable get device status for %s, server returned %d\n",
                  os.Args[2], statusCode )
    }

  } else {

    fmt.Printf( "pass in a device name to get the current status\n" )
    fmt.Printf( "usage: %s status <device name>\n", os.Args[0] )
  }
}

func UpdateDevice( conn net.Conn ) {

  if ( len(os.Args) > 3 ) {

    if ( strings.EqualFold(os.Args[2], "name") ) {

      if ( len(os.Args) > 5 ) {

        fmt.Printf( "not enough args\n" )
        fmt.Printf( "usage: %s name <device name> <new device name>\n",
                os.Args[0] )
        fmt.Printf( "       %s topic <device name> <new mqtt topic>\n",
                os.Args[0] )
        fmt.Printf( "       %s state <device name>\n", os.Args[0] )

        return
      }

      fmt.Fprintf( conn, "UPDATE NAME %s %s KL/%.1f", os.Args[3], os.Args[4],
               KLVersion )

    } else if ( strings.EqualFold(os.Args[2], "topic") ) {

      if ( len(os.Args) > 5 ) {

        fmt.Printf( "not enough args\n" )
        fmt.Printf( "usage: %s name <device name> <new device name>\n",
                os.Args[0] )
        fmt.Printf( "       %s topic <device name> <new mqtt topic>\n",
                os.Args[0] )
        fmt.Printf( "       %s state <device name>\n", os.Args[0] )

        return
      }

      fmt.Fprintf( conn, "UPDATE TOPIC %s %s KL/%.1f", os.Args[3], os.Args[4],
               KLVersion )

    } else if ( strings.EqualFold(os.Args[2], "state") ) {

      fmt.Fprintf( conn, "UPDATE STATE %s KL/%.1f", os.Args[3], KLVersion )

    } else {

      fmt.Printf( "update %s not valid.\n", os.Args[2] )
      fmt.Printf( "usage: %s name <device name> <new device name>\n",
                os.Args[0] )
      fmt.Printf( "       %s topic <device name> <new mqtt topic>\n",
                os.Args[0] )
      fmt.Printf( "       %s state <device name>\n", os.Args[0] )

      return
    }

    /* Read, and parse the response */
    reply, _ := bufio.NewReader( conn ).ReadString( '\n' )
    replyParse := strings.Split( reply, " " )
    statusCode, _ := strconv.ParseInt( replyParse[1], 10, 10 )

    /* Give indication on how well the server responded */
    switch statusCode {

      case 208, 209:
        fmt.Printf( "updated device %s %s to %s\n", os.Args[3], os.Args[2],
                os.Args[4] )

      case 210:
        fmt.Printf( "updated device %s state\n", os.Args[3] )

      default:
        fmt.Printf( "Unable to update %s for %s, server returned %d\n",
                os.Args[2], os.Args[3], statusCode )
    }

  } else {

    fmt.Printf( "not enough args\n" )
    fmt.Printf( "usage: %s name <device name> <new device name>\n",
                os.Args[0] )
    fmt.Printf( "       %s topic <device name> <new mqtt topic>\n",
                os.Args[0] )
    fmt.Printf( "       %s state <device name>\n", os.Args[0] )
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
  if ( strings.EqualFold(os.Args[1], "ip") ) {

    if ( len(os.Args) > 2 ) {

      cfg.Section( "network" ).Key( "ipaddr" ).SetValue( os.Args[2] )
      cfg.SaveTo( usr + "/.config/kisslight/kl-client.ini" )

      fmt.Printf( "IP address %s set\n", os.Args[2] )

    } else {

      fmt.Printf( "please specify IP address\n" )
      fmt.Printf( "usage: %s ip <IP address>\n", os.Args[0] )

      os.Exit( 1 )
    }

    os.Exit( 0 )

  } else if ( strings.EqualFold(os.Args[1], "port") ) {

    if ( len(os.Args) > 2 ) {

      port, _ := strconv.Atoi( os.Args[2] )

      if ( port > 65535 || port < 1 ) {

        fmt.Printf( "port number should be between 1 and 65535\n" )
        fmt.Printf( "usage: %s port <port number>\n", os.Args[0] )

        os.Exit( 1 )

      }

      cfg.Section( "network" ).Key( "port" ).SetValue( strconv.Itoa(port) )
      cfg.SaveTo( usr + "/.config/kisslight/kl-client.ini" )

      fmt.Printf( "port %d set\n", port )

      } else {

        fmt.Printf( "please specify port number\n" )
        fmt.Printf( "usage: %s port <port number>\n", os.Args[0] )

        os.Exit( 1 )
      }

    os.Exit( 0 )

  }

  /* connect to this socket, and port */
  conn, _ := net.Dial( "tcp", cfg.Section("network").Key("ipaddr").String() +
                        ":" + strconv.Itoa(cfg.Section("network").Key(
                        "port").RangeInt(1155, 1, 65535)) )

  /* ignore socket closing errors. */
  defer conn.Close()

  /* Parse first argument */
  if ( strings.EqualFold(os.Args[1], "set") ) {

    SetDev( conn )

  } else if ( strings.EqualFold(os.Args[1], "toggle") ) {

    Toggle( conn )

  } else if ( strings.EqualFold(os.Args[1], "send") ) {

    SendCustomCode( conn )

  } else if ( strings.EqualFold(os.Args[1], "add") ) {

    AddDev( conn )

  } else if ( strings.EqualFold(os.Args[1], "delete") ) {

    DeleteDev( conn )

  } else if ( strings.EqualFold(os.Args[1], "update") ) {

    UpdateDevice( conn )

  }  else if ( strings.EqualFold(os.Args[1], "list") ) {

    GetList( conn )

  } else if ( strings.EqualFold(os.Args[1], "status") ) {

    GetStatus( conn )

  } else {

    /* quietly exit cleanly */
    fmt.Fprintf( conn, "Q\n" )
    Usage()
    os.Exit( 1 )

  }

  /* quietly exit cleanly, after all is done */
  fmt.Fprintf( conn, "Q\n" )
}
