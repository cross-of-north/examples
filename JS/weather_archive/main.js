/**
 * Main application UI controller
 */
class Application {

    constructor() {
        this.log = new Log();

        // base URL to load data from (if not cached)
        this.base_url = "https://raw.githubusercontent.com/cross-of-north/examples/main/JS/weather_archive/";
    }

    /**
     * Redraws the graph using raw DB data.
     * @returns {Promise<boolean>} - false if redraw operation is enqueued because another redraw operation is already in progress.
     * @constructor
     */
    async Redraw() {
        if ( this.data ) {

            if ( this.bInRedraw ) {
                // another redraw operation is already in progress
                // we will call Redraw() again on exit to update with new data
                this.bRedrawAgain = true;
                return false;
            }

            try {

                // redraw operation is in progress
                this.bInRedraw = true;

                this.log.DisplayStatus( "Rendering..." );

                // getting data stats here is needed only to get width of axis labels
                // stats calculation is offloaded to the web worker to keep the UI responsive
                const stats = await this.data_processor.call( "GetStats", [ this.data ] );

                // we need borders around graph to draw axis labels
                const borders = {
                    top: 0,
                    bottom: 15, // magic
                    right: 0,
                    // semi-magic max possible length of the value string
                    left: Math.max( this.canvas.MeasureText( stats.min_value ), this.canvas.MeasureText( stats.max_value ) ),
                };

                // calculating samples/points to draw DB data at the canvas of current size
                // calculation is offloaded to the web worker to keep the UI responsive
                const canvas_size = this.canvas.GetCanvasSize();
                await this.data_processor.call( "CreateVisualData", [ this.data, canvas_size.width, canvas_size.height, borders ] );

                // update local data copy after calculation
                this.visual_data = await this.data_processor.call( "GetVisualData" );

                // drawing Y-grid and labels
                this.canvas.SetPointStyle( Styles.grid_label_point );
                this.visual_data.y_grid && this.visual_data.y_grid.forEach( item => {
                    this.canvas.SetLineStyle( item.v === 0 ? Styles.grid_line_zero : Styles.grid_line );
                    this.canvas.RenderLine( borders.left, item.y, canvas_size.width, item.y );
                    this.canvas.RenderText( borders.left - this.canvas.MeasureText( item.v ) - 3, item.y + 4, item.v );
                } );

                // drawing X-grid and labels
                // leftmost (first) line is the graph border, it is styled like zero gridline
                this.canvas.SetLineStyle( Styles.grid_line_zero );
                this.visual_data.x_grid && this.visual_data.x_grid.forEach( item => {
                    this.canvas.RenderLine( item.x, 0, item.x, canvas_size.height - borders.bottom );
                    // all next lines are styled like a common grid
                    this.canvas.SetLineStyle( Styles.grid_line );
                    this.canvas.RenderText( item.x - this.canvas.MeasureText( item.t ) / 2, canvas_size.height, item.t );
                } );

                // drawing sample-to-sample connection lines
                this.canvas.SetLineStyle( Styles.sample_space_line );
                await this.canvas.RenderLines( this.visual_data.pixels, "samples" );

                // drawing sample points
                this.canvas.SetPointStyle( Styles.sample_point );
                await this.canvas.RenderPoints( this.visual_data.pixels, "samples" );

                // calculating simple MA
                // MA depth is display-based (1/10 of the display width) regardless the meaning of data values or the time range
                // calculation is offloaded to the web worker to keep the UI responsive
                const ma_depth = Math.trunc( this.visual_data.width * 0.1 );
                await this.data_processor.call( "FillMA", [ "avg", ma_depth ] );

                // update local data copy after calculation
                this.visual_data = await this.data_processor.call( "GetVisualData" );

                // drawing MA
                this.canvas.SetPointStyle( Styles.avg_ma_point );
                await this.canvas.RenderPoints( this.visual_data.pixels, "avg_ma" );

                // reset point-under-the-cursor data
                this.point_data_dom.innerText = "";

                // finished rendering
                this.log.DisplayStatus();

            } finally {

                this.bInRedraw = false;
                if ( this.bRedrawAgain ) {
                    // another redraw operation is needed
                    setTimeout( () => {
                        this.canvas.Reset();
                        this.Redraw();
                    }, 0 )
                    this.bRedrawAgain = false;
                }

            } // try

        }
        return true;
    }

    /**
     * Update external size of the DOM canvas element.
     * Top left corner is taken from the flow.
     * Bottom right corner is attached to the bottom right corner of window.
     * @param canvas_element
     * @private
     */
    UpdateDOMCanvasSize( canvas_element ) {
        const client_rect = canvas_element.getBoundingClientRect();
        const parent_styles = window.getComputedStyle( canvas_element.parentElement );
        canvas_element.style.width = ( window.innerWidth - client_rect.left - parseFloat( parent_styles.paddingRight ) ) + "px";
        canvas_element.style.height = ( window.innerHeight - client_rect.top - parseFloat( parent_styles.paddingBottom ) ) + "px";
    }

    /**
     * Update graph and overlay canvases after window resize.
     * @constructor
     */
    UpdateCanvasSize() {
        this.UpdateDOMCanvasSize( this.graph_dom );
        this.UpdateDOMCanvasSize( this.overlay_dom );

        // defer internal canvas size recalculation waiting for external DOM element size update to be applied
        setTimeout( () => {

            this.canvas.UpdateCanvasSize();
            this.overlay.UpdateCanvasSize();

            // redraw graph with the new canvas size
            this.Redraw().then();

        }, 0 );
    }

    /**
     * Load a full or limited dataset from the storage.
     * If data is not found in the storage (DB), then the storage-level logic tries to load full dataset from the network.
     * @param params - range filter
     * @returns {Promise<void>}
     * @constructor
     */
    async DoLoadData( params = undefined ) {

        // reset local data copy
        this.data = undefined;

        try {
            // load data from the storage (or from URL if this file is not cached yet)
            // task is offloaded to the web worker to keep the UI responsive
            this.data = await this.data_storage.call( "GetData", params );
        } catch ( e ) {
            // display failure reason (i.e. 404 or restricted browser with read-only IndexedDB)
            this.log.DisplayError( "Not opened: " + e );
        }

    }

    /**
     * Load a full dataset from the storage, then plot the data loaded.
     * If data is not found in the storage (DB), then the storage-level logic tries to load data from the URL given.
     * @param table_name
     * @returns {Promise<void>}
     * @constructor
     */
    async LoadData( table_name ) {

        // clear graph
        this.canvas.Reset();

        // close the current instance of storage (to upgrade IndexedDB, adding another table schema if needed)
        this.data_storage && await this.data_storage.call( "Close" );

        // same-thread storage for debugging
        //this.data_storage = new LocalRPC( { log: this.log } );

        // web-worker-based storage
        this.data_storage = new WebWorkerRPC( { log: this.log } );

        // init web worker with DataStorage as the main executor and with additional classes in the scope
        await this.data_storage.init( "DataStorage", [ "IndexedDB", "DataProcessor", "RemoteLog" ] );

        // pass parameters for DataStorage initialization
        await this.data_storage.call( "Init", [ {

            // base URL to load data from (if not cached)
            base_url: this.base_url,

            database_name: "weather_data", // arbitrary
            table_name: table_name, // name of the .json file

            // Data is split by months as per request:
            // ("A record for an individual month of meteorological measurements must be stored as a separate object/record in IndexedDB")
            schema: { keyPath: "month" },

            log: this.log,
        } ] );

        // load the full dataset
        await this.DoLoadData();

        if ( this.data ) {

            // warn user about possible problems with the dataset
            if ( !this.data.length ) {
                this.log.DisplayError( "Warning: empty dataset" );
            } else {
                if ( !this.data[ 0 ] || !this.data[ 0 ].month ) {
                    this.log.DisplayError( "Warning: invalid dataset" );
                }
            }

            // since we have obtained the full unfiltered dataset here, we can fill year selectors
            // year values collection is offloaded to the web worker to keep the UI responsive
            const years = await this.data_processor.call( "GetYears", [ this.data ] );
            [ this.from_year_dom, this.to_year_dom ].forEach( select => {
                while ( select.options.length ) {
                    select.options.remove( 0 );
                }
                years.forEach( year => {
                    select.add( new Option( year, year ) );
                } );
            } );

            // first year in the full dataset
            this.first_year = String( years[ 0 ] );
            // last year in the full dataset
            this.last_year = String( years[ years.length - 1 ] );

            // first year shown on the graph
            this.from_year = this.first_year;
            // last year shown on the graph
            this.to_year = this.last_year;

            // update values of DOM selects
            this.from_year_dom.value = this.from_year;
            this.to_year_dom.value = this.to_year;

            // hide Reset button
            this.UpdateResetYearsVisibility();

            // draw new data, do not wait for completion
            this.Redraw().then();
        }
    }

    /**
     * Load temperature table.
     * @constructor
     */
    LoadTemperature() {
        this.LoadData( "temperature.json" ).then();
    }

    /**
     * Load precipitation table.
     * @constructor
     */
    LoadPrecipitation() {
        this.LoadData( "precipitation.json" ).then();
    }

    /**
     * Show or hide Reset range button
     * @constructor
     */
    UpdateResetYearsVisibility() {
        // Reset button should be visible only if the current range is not full
        if ( this.from_year === this.first_year && this.to_year === this.last_year ) {
            this.reset_years_dom.style.visibility = "hidden";
        } else {
            this.reset_years_dom.style.visibility = "visible";
        }
    }

    /**
     * Reset time range shown to full.
     * @constructor
     */
    ResetYears() {
        // Reset range selected
        this.from_year_dom.value = this.first_year;
        this.to_year_dom.value = this.last_year;
        this.OnTimeRangeUpdated().then();
    }

    /**
     * Apply the time range chosen via the UI.
     * @returns {Promise<void>}
     * @constructor
     */
    async OnTimeRangeUpdated() {
        // only if range have changed
        if ( this.from_year !== this.from_year_dom.value || this.to_year !== this.to_year_dom.value ) {

            // switch year values if they are chosen in inverse order in the UI
            this.from_year = ( this.from_year_dom.value < this.to_year_dom.value ) ? this.from_year_dom.value : this.to_year_dom.value;
            this.to_year = ( this.from_year_dom.value < this.to_year_dom.value ) ? this.to_year_dom.value : this.from_year_dom.value;

            // load the dataset limited by the range chosen
            // table key is the ISO month, i.e. 1970-01
            await this.DoLoadData([ this.from_year + "-01", this.to_year + "-12" ] );

            if ( this.data ) {
                // clear graph
                this.canvas.Reset();
                // draw new data, do not wait for completion
                this.Redraw().then();
                // show Reset button if needed
                this.UpdateResetYearsVisibility();
            }
        }
    }

    /**
     * Returns optimal readable 3-digit floating number representation:
     * 1234.56789 => 1234
     * 234.56789 => 234
     * 34.56789 => 34.5
     * 4.56789 => 4.56
     * 0.56789 => 0.567
     * 0.06789 => 0.0678
     * ...
     * @param n
     * @returns {string|string}
     * @constructor
     */
    FormatNumber( n ) {
        return Number( n ) === 0 ? "0" : Number( n ).toFixed( Math.min( 3 - Math.log10( Math.abs( n ) ), 100 ) );
    }

    /**
     * Start dragging to select new range to drill down to.
     * @param event
     * @constructor
     */
    OnMouseDownOverCanvas( event ) {
        if ( this.mouse_down_counter === 0 ) {
            this.mouse_drag_from_x = this.canvas.FromScreenToCanvasCoordinates( { x: event.clientX } ).x;
        }
        this.mouse_down_counter++;
    }

    /**
     * Get the nearest left data point from the x'th plot pixel (including the data point at x).
     * @param x
     * @returns {*} - data point object or undefined
     * @constructor
     */
    GetNearestLeftDataPoint( x ) {
        let pixel;
        if ( this.visual_data ) {
            x = Math.min( Math.round( x ), this.visual_data.pixels.length - 1 );
            if ( x >= 0 && x < this.visual_data.pixels.length ) {
                let data_index = x;
                while ( !pixel && data_index >= 0 ) {
                    pixel = this.visual_data.pixels[ data_index ];
                    data_index--;
                }
            }
        }
        return pixel;
    }

    /**
     * End dragging to select new range.
     * Drill down to the range selected.
     * @param event
     * @constructor
     */
    OnMouseUpOverCanvas( event ) {
        this.mouse_down_counter--;
        if ( this.mouse_down_counter === 0 ) {

            // erase drag selection rectangle
            this.OnMouseOverCanvas( event );

            const current_x = this.canvas.FromScreenToCanvasCoordinates( { x: event.clientX } ).x;

            // set years in the correct order (from<=to) regardless of the drag direction
            let set_year = ( x, dom ) => {
                x = Math.round( x );
                const pixel = this.GetNearestLeftDataPoint( x );
                if ( pixel ) {
                    // min_t of the data point is the ISO date (i.e. 1970-01-01)
                    dom.value = pixel.min_t.substring( 0, 4 );
                }
            }
            set_year( Math.min( this.mouse_drag_from_x, current_x ), this.from_year_dom );
            set_year( Math.max( this.mouse_drag_from_x, current_x ), this.to_year_dom );

            // apply the new time range
            this.OnTimeRangeUpdated().then();
        }
    }

    /**
     * Draw the graph overlay (crosshair and range selection rectangle).
     * Display data about the data point under cursor.
     * @param event
     * @constructor
     */
    OnMouseOverCanvas( event ) {

        const point = this.canvas.FromScreenToCanvasCoordinates( { x: event.clientX, y: event.clientY } );

        // round values for crisp line drawing
        const x = Math.round( point.x );
        const y = Math.round( point.y );

        let point_data_text = "";
        const pixel = this.GetNearestLeftDataPoint( x );
        if ( pixel ) {

            // date range
            point_data_text += pixel.min_t;
            if ( pixel.max_t !== pixel.min_t ) {
                // multiple dates aggregation
                point_data_text += " - ";
                point_data_text += pixel.max_t;
            }

            if ( pixel.max_t === pixel.min_t ) {
                // single date
                point_data_text += ", value=";
                point_data_text += pixel.min_v;
            } else {
                // multiple dates aggregation stats
                point_data_text += ", min=";
                point_data_text += pixel.min_v;
                point_data_text += ", max=";
                point_data_text += pixel.max_v;
                point_data_text += ", avg=";
                point_data_text += this.FormatNumber( pixel.avg_v );
            }

            if ( pixel.avg_ma !== undefined ) {
                // underlying MA value
                point_data_text += ", MA=";
                point_data_text += this.FormatNumber( DataProcessor.prototype.YToValueIndirect( this.visual_data, pixel.avg_ma ) );
            }

            point_data_text += ", cursor=";
            const cursor_value = DataProcessor.prototype.YToValueIndirect( this.visual_data, y );
            point_data_text += this.FormatNumber( cursor_value );
        }

        this.point_data_dom.innerText = point_data_text;

        // draw new overlay
        this.overlay.Reset();

        const overlay_size = this.overlay.GetCanvasSize();

        // draw the crosshair
        this.overlay.SetLineStyle( Styles.crosshair_line );
        this.overlay.RenderLine( 0, y, overlay_size.width, y );
        this.overlay.RenderLine( x, 0, x, overlay_size.height );

        if ( this.mouse_down_counter > 0 ) {
            // draw the selection rectangle
            this.overlay.SetPointStyle( Styles.rubber_band_point );
            this.overlay.RenderRect( this.mouse_drag_from_x, 0, x, overlay_size.height );
        }
    }

    /**
     * Show that main UI thread is not blocked by updating text node every 100ms.
     * @constructor
     */
    RunUILivenessTest() {
        let liveness_element = document.getElementById( "liveness" );
        let chars = [ '_', '' ];
        let char_index = 0;
        setInterval( () => {
            if ( char_index >= chars.length ) {
                char_index = 0;
            }
            liveness_element.innerText = " " + chars[ char_index ];
            char_index++;
        }, 100 );
    }

    /**
     * Init application
     * @returns {Promise<void>}
     * @constructor
     */
    async Init() {

        this.RunUILivenessTest();

        // same-thread calculation engine for debugging
        //this.data_processor = new LocalRPC();

        // web-worker-based calculation engine
        this.data_processor = new WebWorkerRPC();

        // canvas DOM element for graph rendering
        this.graph_dom = document.getElementById( "graph" );
        // canvas class instance for graph rendering
        this.canvas = new CanvasRenderer( this.graph_dom );

        // canvas DOM element for overlay rendering
        this.overlay_dom = document.getElementById( "overlay" );
        // canvas DOM element for overlay rendering
        this.overlay = new CanvasRenderer( this.overlay_dom );

        // binding mouse events to the canvas with the topmost z-order
        this.graph_dom.addEventListener( "mousemove", this.OnMouseOverCanvas.bind( this ) );
        this.graph_dom.addEventListener( "mousedown", this.OnMouseDownOverCanvas.bind( this ) );
        this.graph_dom.addEventListener( "mouseup", this.OnMouseUpOverCanvas.bind( this ) );

        // mouse drag (button press) indicator
        this.mouse_down_counter = 0;

        // binding to window resize to update canvas size
        window.addEventListener( "resize", this.UpdateCanvasSize.bind( this ) );

        // initial canvas size update
        this.UpdateCanvasSize();

        // table load buttons
        document.getElementById( "temperature_button" ).addEventListener( "click", this.LoadTemperature.bind( this ) );
        document.getElementById( "precipitation_button" ).addEventListener( "click", this.LoadPrecipitation.bind( this ) );

        // first year DOM select
        this.from_year_dom = document.getElementById( "from_year" );
        this.from_year_dom.addEventListener( "change", this.OnTimeRangeUpdated.bind( this ) );

        // last year DOM select
        this.to_year_dom = document.getElementById( "to_year" );
        this.to_year_dom.addEventListener( "change", this.OnTimeRangeUpdated.bind( this ) );

        // data about point under cursor
        this.point_data_dom = document.getElementById( "point_data" );

        // Reset time range DOM button
        this.reset_years_dom = document.getElementById( "reset_years" );
        this.reset_years_dom.addEventListener( "click", this.ResetYears.bind( this ) );

        // init calculation engine web worker
        await this.data_processor.init( "DataProcessor" );

        // "By default, a user should see the archive data for the changes in the temperature"
        this.LoadTemperature();
    }

}

// bootstrap
let application;
document.addEventListener( "DOMContentLoaded", () => {
    application = new Application();
    application.Init().then();
} );
