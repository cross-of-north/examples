import splunk.Intersplunk
import intersplunk_streaming,re,sys,ip_cached_lookup

ipre = re.compile("^(\s)*\d+\.\d+\.\d+\.\d+(\s)*$")

error = ""

bStrictMode = False
if ( len( sys.argv ) > 1 and sys.argv[ 1 ] == "strict" ) :
  sys.argv.pop( 1 )
  bStrictMode = True
#if

def processor(result) :
    bSuccess = False
    try:
        for k,v in result.items():
            if ( k != "_time" and ( len( sys.argv ) < 2 or k in sys.argv ) and not ip_cached_lookup.NO_RDNS_SUFFIX in k ) :
                if ( ipre.match( v ) != None ) :
                    s = ip_cached_lookup.rlookup( v )
                    if ( ( s == None or s == ip_cached_lookup.INVALID_HOST_NAME ) and not bStrictMode ) :
                        s = ip_cached_lookup.block_name( v )
                    #if
                    if ( s != None and s != ip_cached_lookup.INVALID_HOST_NAME ) :
                        result[ k ] = s
                        result[ ip_cached_lookup.NO_RDNS_SUFFIX ] = v
                    #if
                #if
            #if
        #for
        bSuccess = True
    except Exception, e:
        global error
        error += "Exception! %s " % (e,)
    #try
    return bSuccess
#def

intersplunk_streaming.processResults( processor, [ ip_cached_lookup.NO_RDNS_SUFFIX ] )

try:
    ip_cached_lookup.commit()
except Exception, e:
    error += "Exception! %s " % (e,)
#try

if ( len( error ) > 0 ) :
    splunk.Intersplunk.generateErrorResults( error )
#if
