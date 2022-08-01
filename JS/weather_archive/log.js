/**
 * A way to display program status information to the user.
 */
class Log {

    /**
     * Shows or hides informational panel with a permanent (until next DisplayStatus/DisplaySuccess call) message.
     * @param s - if empty - hide panel
     * @constructor
     */
    DisplayStatus( s ) {
        clearTimeout( this.display_status_timeout_handler );
        const block = document.getElementById( "status_message" );
        if ( s && s.length ) {
            block.style.display = "block";
            block.innerText = s;
        } else {
            block.style.display = "none";
            block.innerText = "";
        }
    }

    /**
     * Shows informational panel with temporary message.
     * The panel is hidden automatically in 5 seconds.
     * @param s
     * @constructor
     */
    DisplaySuccess( s ) {
        this.DisplayStatus( s );
        this.display_status_timeout_handler = setTimeout( () => {
            const block = document.getElementById( "status_message" );
            block.innerText = "";
            block.style.display = "none";
        }, 5000 )
    }

    /**
     * Shows red error panel with temporary error message.
     * The panel is hidden automatically in 10 seconds.
     * @param s
     * @constructor
     */
    DisplayError( s ) {
        clearTimeout( this.display_error_timeout_handler );
        this.DisplayStatus();
        const block = document.getElementById( "error_message" );
        block.style.display = "block";
        if ( block.innerText.length ) {
            block.innerText += "\r\n";
        }
        block.innerText += s;
        this.display_error_timeout_handler = setTimeout( () => {
            block.innerText = "";
            block.style.display = "none";
        }, 10000 )
    }

}

/**
 * Remote log sink to bridge Log commands from the web worker to the main UI thread.
 */
class RemoteLog {

    DisplayStatus( s ) {
        postMessage( { id: "log", name: "DisplayStatus", text: s } );
    }

    DisplaySuccess( s ) {
        postMessage( { id: "log", name: "DisplaySuccess", text: s } );
    }

    DisplayError( s ) {
        postMessage( { id: "log", name: "DisplayError", text: s } );
    }

}
