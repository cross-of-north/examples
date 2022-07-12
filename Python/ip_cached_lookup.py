import sys
import socket
import time
import sqlite3
import whois
import re

NO_RDNS_SUFFIX = "_no_rdns"
INVALID_HOST_NAME = "-";
#print sqlite3.__file__
conn = sqlite3.connect('./../../../var/ip_lookup.db')
conn.text_factory = str #to save national strings
c = conn.cursor()

def create_database() :
    try :
        c.execute( "DROP TABLE ip_name" )
        c.execute( "DROP TABLE ip_block" )
        c.execute( "DROP TABLE whois_load" )
        c.execute( "DROP TABLE stat" )
    except Exception as e :
        pass
    #try

    #reverse DNS cache
    c.execute( "CREATE TABLE ip_name (ip text, name text, valid bool, ts timestamp)" )
    c.execute( "CREATE INDEX ix_ip ON ip_name (ip)" )
    c.execute( "CREATE INDEX ix_name ON ip_name (name)" )

    #whois cache
    c.execute( "CREATE TABLE ip_block (from_value int, to_value int, range int, name text, server text, redirect text, valid bool, hit_count int, ts timestamp)" )
    c.execute( "CREATE INDEX ix_block ON ip_block (from_value, to_value)" )
    c.execute( "CREATE INDEX ix_block_ts ON ip_block (from_value, to_value, ts)" )

    #whois ban protection
    c.execute( "CREATE TABLE whois_load ( server text, counter int, ts timestamp )" )
    c.execute( "CREATE INDEX ix_whois_load_by_ts ON whois_load ( ts )" )
    c.execute( "CREATE INDEX ix_whois_load ON whois_load ( server, ts )" )

    #usage statistics
    c.execute( "CREATE TABLE stat (ip_hit integer, block_hit integer, ip_count integer, block_count integer)" )
    c.execute( "INSERT INTO stat (ip_hit, block_hit, ip_count, block_count) VALUES (0,0,0,0)" )

    conn.commit()

try :
    c.execute( "SELECT * FROM stat" )
except Exception as e :
    create_database()
#try

ip_count_inc = 0
ip_hit_inc = 0

current_time = time.time()

rlookup_cache = dict()

# Given an ip, return the host
def rlookup( ip ) :
    global current_time
    global rlookup_cache

    hostname = ""
    
    try :

        #update ip query count
        global ip_count_inc
        ip_count_inc += 1

        if ( ip in rlookup_cache ) :

            hostname = rlookup_cache[ ip ]

        else :

            t = ( ip, )
            for name,ts in c.execute( 'SELECT name,ts FROM ip_name WHERE ip=?', t ) :
                # 1 day storage for invalid and 30 days for valid lookups
                if ( ts < current_time - 60 * 60 * 24 * ( 1 if ( name == INVALID_HOST_NAME ) else 30 ) ) :
                    #obsolete record
                    t = ( ip, )
                    c.execute( 'DELETE FROM ip_name WHERE ip=?', t )
                else :
                    hostname = name
                    break
                #if
            #for
        
        #if

        if ( len( hostname ) == 0 ) :
            
            #cache miss
            try :
                hostname, aliaslist, ipaddrlist = socket.gethostbyaddr(ip)
            except socket.error as ( error_code, error_string ) :
                hostname = INVALID_HOST_NAME;
            #try

            if ( len( hostname ) != 0 ) :
                #save into cache
                t = ( ip, )
                c.execute( 'DELETE FROM ip_name WHERE ip=?', t )
                t = ( ip, hostname, hostname != INVALID_HOST_NAME, int( current_time ), )
                c.execute( "INSERT INTO ip_name (ip,name,valid,ts) VALUES (?,?,?,?)", t )
            #if
        else :
            #cache hit
            global ip_hit_inc
            ip_hit_inc += 1
        #if

    except Exception as e :
        #splunk.Intersplunk.generateErrorResults("Exception! %s" % (e,))
        pass
    #try

    if ( not ip in rlookup_cache ) :
        rlookup_cache[ ip ] = hostname
    #if

    return hostname
#def

whois_name_parser = re.compile( "^(?:(?:descr)|(?:netname)|(?:country)|(?:route)|(?:cidr)|(?:owner)|(?:ownerid)):\s*(.*)$", re.IGNORECASE | re.MULTILINE );
whois_address_parser = re.compile( "^(?:(?:inetnum)|(?:netrange)):\s*([0-9.]+)\s*(?:/|-)\s*([0-9.]+)\s*$", re.IGNORECASE | re.MULTILINE );

INVALID_BLOCK_NAME = "-";

NO_REDIRECT = ""
NO_SERVER = ""

# ip to number
def local_aton( ip ) :
    octets = ip.split( "." );
    return (
        ( int( octets[ 0 ] if ( len( octets ) > 0 ) else 0 ) << ( 8 * 3 ) ) + 
        ( int( octets[ 1 ] if ( len( octets ) > 1 ) else 0 ) << ( 8 * 2 ) ) + 
        ( int( octets[ 2 ] if ( len( octets ) > 2 ) else 0 ) << ( 8 * 1 ) ) + 
        ( int( octets[ 3 ] if ( len( octets ) > 3 ) else 0 ) << ( 8 * 0 ) )
    )
#def

class CachedNICClientException( Exception ) :
    pass
#class

block_history = list()

class CachedNICClient(whois.NICClient) :
    
    #whois after query callback
    def on_whois_server_found( self, query, hostname, flags, nhost, response ) :
        save_block( local_aton( query ), response, hostname, NO_REDIRECT if ( nhost == None ) else nhost )
        return super( CachedNICClient, self ).on_whois_server_found( query, hostname, flags, nhost, response )
        
    #whois before query callback
    def on_whois_server_search( self, query, hostname, flags ) :
        ts_now = int( time.time() )
        ts_now_minutes = int( time.time() / 60 ) * 60
        t = ( ts_now - 60 * 60 * 24 * 2, )
        c.execute( "DELETE FROM whois_load WHERE ts<?", t )
        if ( hostname in whois.NICClient.whois_limits ) :
            WHOIS_OVERLOAD_DELTA =  60 * 60 * 24 * 1 - 60 * 5
            t = ( hostname, ts_now - WHOIS_OVERLOAD_DELTA )
            ( ( sum, ), ) = c.execute( "SELECT SUM( counter ) FROM whois_load WHERE server=? AND ts>=?", t )
            if ( sum >= whois.NICClient.whois_limits[ hostname ] ) :
                ( ( nearest_time, ), ) = c.execute( "SELECT ts FROM whois_load WHERE server=? AND ts>=? AND counter > 0 ORDER BY ts ASC LIMIT 1", t )
                raise CachedNICClientException( hostname + " - no access until " + time.strftime( "%H:%M", time.localtime( nearest_time + WHOIS_OVERLOAD_DELTA ) ) )
            #if
        #if
        t = ( ts_now, hostname )
        bUpdated = False
        t = ( ts_now_minutes, hostname )
        for counter,ts in c.execute( "SELECT counter,ts FROM whois_load WHERE ts=? AND server=?", t ) :
            bUpdated = True
            c.execute( "UPDATE whois_load SET counter = counter + 1 WHERE ts=? AND server=?", t )
            conn.commit()
            break
        #for
        if ( not bUpdated ) :
            c.execute( "INSERT INTO whois_load (ts,server,counter) VALUES (?,?,1)", t )
            conn.commit()
        #if
        block_name,redirect_to = search_block( local_aton( query ), hostname )
        if ( block_name != None ) :
            global block_history
            block_history.append( block_name )
        #if
        return redirect_to

#class

def save_block( ip_as_int, whois_data, server, redirect ) :
    try :
        #print whois_data
        block_name = "|".join( whois_name_parser.findall( whois_data )[ ::-1 ] ).replace( '\r', '' ).replace( '\n', '' )
        address_range = whois_address_parser.findall( whois_data )[ -1 ]
        from_value = local_aton( address_range[ 0 ].replace( '\r', '' ).replace( '\n', '' ) )
        to_value = address_range[ 1 ].replace( '\r', '' ).replace( '\n', '' )
        if ( len( to_value ) < 4 ) :
            #netmask
            to_value = from_value + ( 1 << ( 32 - int( to_value ) ) ) - 1
        else :
            #range
            to_value = local_aton( to_value )
        #if
        if ( ip_as_int < from_value or ip_as_int > to_value ) :
            raise Exception()
        #if
    except Exception as e :
        from_value = ip_as_int
        to_value = ip_as_int
        block_name = INVALID_BLOCK_NAME
    #try

    global block_history
    block_history.append( block_name )

    t = ( from_value, to_value )
    if ( server != NO_SERVER ) :
        t += ( server, )
    #if
    
    c.execute( 
        "DELETE FROM ip_block WHERE from_value=? AND to_value=?" + 
        ( 
            "" 
            if ( redirect == NO_REDIRECT ) else 
            " AND redirect IS NOT NULL AND LENGTH(redirect)>0" 
        ) +
        ( 
            "" 
            if ( server == NO_SERVER ) else 
            " AND server IS NOT NULL AND server=?" 
        )
    , t )

    t = ( 
        from_value, to_value, to_value - from_value, block_name, server, redirect,
        block_name != INVALID_BLOCK_NAME, 0, int( time.time() ) 
    )
    c.execute( "INSERT INTO ip_block (from_value,to_value,range,name,server,redirect,valid,hit_count,ts) VALUES (?,?,?,?,?,?,?,?,?)", t )
    conn.commit()
#def

def search_block( ip_as_int, server ) :

    block_name = None
    redirect_to = None

    try :
        t = ( ip_as_int, ip_as_int )
        if ( server != NO_SERVER ) :
            t += ( server, )
        #if
        for from_value,to_value,name,redirect,ts in c.execute( 
            "SELECT from_value,to_value,name,redirect,ts FROM ip_block WHERE from_value<=? AND to_value>=?" + 
            ( 
                " AND ( redirect IS NULL OR LENGTH( redirect ) == 0 )" 
                if ( server == NO_SERVER ) else 
                " AND server=?" 
            ) + 
            " ORDER BY range DESC"
        , t ) :
            if ( ts < time.time() - 60 * 60 * 24 * ( 1 if ( name == INVALID_BLOCK_NAME ) else 160 ) ) :
                t = ( from_value, to_value, ts )
                c.execute( 'DELETE FROM ip_block WHERE from_value=? AND to_value=? AND ts=?', t )
            else :
                if ( len( redirect ) == 0 ) :
                    t = ( from_value, to_value, ts )
                    c.execute( "UPDATE ip_block SET hit_count = hit_count + 1 WHERE from_value=? AND to_value=? AND ts=?", t )
                #if
                block_name = name
                redirect_to = redirect
                break
            #if
        #for
    except Exception as e :
        block_name = ""
    #try

    return ( block_name, redirect_to )
#def

def block_name( ip ) :
    block_name = None
    try :

        c.execute( "UPDATE stat SET block_count = block_count + 1" )
        ip_as_int = local_aton( ip )
        block_name,redirect_to = search_block( ip_as_int, NO_SERVER )

        if ( block_name == None ) :
            try :
                global block_history
                block_history = list()
                nic_client = CachedNICClient()
                nic_client.whois_lookup( None, ip, 0 )
                block_name,redirect_to = search_block( ip_as_int, NO_SERVER )
            except CachedNICClientException as e :
                block_history.append( e[ 0 ] )
                block_name = "|".join( block_history )
            #try
        else :
            c.execute( "UPDATE stat SET block_hit = block_hit + 1" )
        #if

    except Exception as e :
        block_name = ""
    #try

    conn.commit()
    return block_name.encode('string_escape');
#def

def commit() :
    global ip_count_inc
    if ( ip_count_inc > 0 ) :
        t = ( ip_count_inc, )
        c.execute( "UPDATE stat SET ip_count = ip_count + ?", t );
    #if
    global ip_hit_inc
    if ( ip_hit_inc > 0 ) :
        t = ( ip_hit_inc, )
        c.execute( "UPDATE stat SET ip_hit = ip_hit + ?", t )
    #if
    conn.commit()
#def
