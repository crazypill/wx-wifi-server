//
//  main.c
//  wx-wifi
//
//  Created by Alex Lelievre on 7/22/20.
//  Copyright © 2020 Alex Lelievre. All rights reserved.
//
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <time.h>

#define PORT    5555
#define MAXMSG  512

static FILE* s_logFile = NULL;


int make_socket (uint16_t port)
{
  int sock;
  struct sockaddr_in name;

  /* Create the socket. */
  sock = socket (PF_INET, SOCK_STREAM, 0);
  if (sock < 0)
    {
      perror ("socket");
      exit (EXIT_FAILURE);
    }

  /* Give the socket a name. */
  name.sin_family = AF_INET;
  name.sin_port = htons (port);
  name.sin_addr.s_addr = htonl (INADDR_ANY);
  if (bind (sock, (struct sockaddr *) &name, sizeof (name)) < 0)
    {
      perror ("bind");
      exit (EXIT_FAILURE);
    }

  return sock;
}

int read_from_client( int filedes )
{
  char buffer[MAXMSG];
  ssize_t nbytes = 0;

    memset( buffer, 0, sizeof( buffer ) );
    nbytes = read( filedes, buffer, MAXMSG );
    if( nbytes < 0 )
    {
        perror( "read" );
        return (int)nbytes;
    }
    else if( nbytes == 0 )
    {
        // End-of-file
        return -1;
    }

    // lazy open our log file...
    if( !s_logFile )
        s_logFile = fopen( "wx.log.csv", "a" );

#ifdef DEBUG
    fprintf( stderr, "Server: got message: `%s'\n", buffer );
#endif
    
    if( s_logFile )
    {
        time_t t = time(NULL);
        struct tm tm = *localtime(&t);
        fprintf( s_logFile, "%d-%02d-%02d, %02d:%02d:%02d, %s\n", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, buffer );
        fflush( s_logFile ); // in case someone is watching it...
    }

    return 0;
}




int main( void )
{
    int       sock;
    fd_set    active_fd_set;
    fd_set    read_fd_set;
    socklen_t size = 0;
    struct sockaddr_in clientname = {};

    // Create the socket and set it up to accept connections.
    sock = make_socket( PORT );
    if( listen( sock, 1 ) < 0 )
    {
        perror( "listen" );
        exit( EXIT_FAILURE );
    }

    printf( "wx-wifi-server listening on port %d\n", PORT );
    
    // Initialize the set of active sockets
    FD_ZERO( &active_fd_set );
    FD_SET( sock, &active_fd_set );

    while( 1 )
    {
        // Block until input arrives on one or more active sockets
        read_fd_set = active_fd_set;
        if( select( FD_SETSIZE, &read_fd_set, NULL, NULL, NULL ) < 0 )
        {
            perror ("select");
            continue;
//          exit( EXIT_FAILURE );
        }

        // Service all the sockets with input pending.
        for( int i = 0; i < FD_SETSIZE; ++i )
        {
            if( FD_ISSET( i, &read_fd_set ) )
            {
                if( i == sock )
                {
                    // Connection request on original socket
                    size = sizeof( clientname );
                    int new = accept( sock, (struct sockaddr *)&clientname, &size );
                    if( new < 0 )
                    {
                        perror( "accept" );
                    }
                    else
                    {
#ifdef DEBUG
                        fprintf( stderr, "Server: connect from host %s, port %hd.\n", inet_ntoa( clientname.sin_addr ), ntohs( clientname.sin_port ) );
#endif
                    }
                    FD_SET( new, &active_fd_set );
                }
                else
                {
                    // Data arriving on an already-connected socket
                    if( read_from_client( i ) < 0 )
                    {
                        close( i );
                        FD_CLR( i, &active_fd_set );
                    }
                }
            }
        }
    }
}


