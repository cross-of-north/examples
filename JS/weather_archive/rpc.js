/**
 * Web worker wrapper.
 */
class WebWorkerRPC {

    constructor( options ) {
        this.options = { ...options };
        // unique query id generator
        this.query_counter = 0;
        // storage of query promises
        this.query_promises = {};
    }

    /**
     * Main web worker function.
     * Transferred to the worker context as a text and called there.
     * Never called directly here in UI thread.
     */
    worker_function() {
        // worker thread message processor
        onmessage = async function ( event ) {
            let result;
            let error_text = undefined;
            try {
                // call method in the command receiver (main class instance) with or without parameters
                result = event.data.params === undefined ? await o[ event.data.function_name ]() : await o[ event.data.function_name ]( ...event.data.params );
            } catch ( e ) {
                error_text = e.toString();
            }
            // return successful result or error to the main thread
            postMessage( error_text === undefined ? { id: event.data.id, result: result } : {
                id: event.data.id,
                error: true,
                error_text: error_text
            } );
        }
    }

    /**
     * Initializes web worker using locally defined main class and additional classes.
     * Main class becomes the web worker message processor.
     * @param class_name
     * @param additional_class_names
     * @returns {Promise<void>}
     */
    async init( class_name, additional_class_names ) {

        // translating every class body into the text
        let script_text = "";
        if ( !Array.isArray( additional_class_names ) ) {
            additional_class_names = [];
        }
        additional_class_names.push( class_name );
        additional_class_names.forEach( class_name => {
            script_text += eval( "(" + class_name + ".toString())" );
        } );

        // main class is instantiated as a worker command receiver
        script_text += ";o=new " + class_name + "();";

        // translating main web worker function into the text
        let worker_function_text = this.worker_function.toString();
        // ensuring that function declaration text is correct
        // Safari toString() returns "XXX(){..."
        // Chrome/Firefox toString() returns "function XXX(){..."
        if ( worker_function_text.indexOf( "function" ) !== 0 ) {
            worker_function_text = "function " + worker_function_text;
        }
        script_text += "(" + worker_function_text + ")()";

        // starting blob-text-based web worker
        this.worker = new Worker( URL.createObjectURL( new Blob( [ script_text ], { type: 'text/javascript' } ) ) );

        // UI thread message processor
        this.worker.onmessage = function ( event ) {
            if ( event.data.id === "log" ) {
                // log data from RemoteLog
                this.options.log && this.options.log[ event.data.name ]( event.data.text );
            } else {
                // call() result message
                const promise = this.query_promises[ event.data.id ];
                if ( promise ) {
                    // finish and delete relevant promise
                    if ( event.data.error ) {
                        promise.reject( event.data.error_text );
                    } else {
                        promise.resolve( event.data.result );
                    }
                    delete this.query_promises[ event.data.id ];
                }
            }
        }.bind( this );

    }

    /**
     * Returns result of the method call on the web worker main class.
     * @param method_name
     * @param params
     * @returns {Promise<unknown>}
     */
    async call( method_name, params ) {
        return new Promise( ( resolve, reject ) => {
            // save this promise to finalize it after receiving the reply message
            this.query_promises[ this.query_counter ] = {
                resolve: resolve,
                reject: reject,
            };
            // pass command to call method to the worker
            this.worker.postMessage( { id: this.query_counter, function_name: method_name, params: params } );
            this.query_counter++;
        } );
    }
}

/**
 * Dummy RPC to debug using locally instantiated classes instead of remote classes.
 */
class LocalRPC {

    constructor() {
    }

    async init( class_name ) {
        this.o = eval( "(new " + class_name + "())" );
    }

    async call( function_name, params ) {
        return params === undefined ? this.o[ function_name ]() : this.o[ function_name ]( ...params );
    }
}
