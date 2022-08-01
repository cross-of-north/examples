/**
 * Calculation and data preprocessing engine.
 * Used in the web worker context.
 */
class DataProcessor {

    constructor() {
    }

    /**
     * Calculate and store Simple Moving Average using internal data calculation structure.
     * @param key_name - name of the key to calculate MA on. The MA data is named "key_name + "_ma"
     * @param depth
     * @param bCutLeftEdge
     * @constructor
     */
    FillMA( key_name, depth, bCutLeftEdge = true ) {
        const new_key_name = key_name + "_ma";
        const min_index = bCutLeftEdge ? ( depth - 1 ) : 0;
        this.visual_data.pixels.forEach( ( item, index ) => {
            if ( index < min_index ) {
                return;
            }
            let sample_count = depth;
            let i = sample_count - 1;
            let sum = 0;
            while ( i >= 0 ) {
                const prev_item = this.visual_data.pixels[ index - i ];
                if ( prev_item === undefined ) {
                    sample_count--;
                } else {
                    const value = prev_item[ key_name ];
                    if ( value === undefined ) {
                        sample_count--;
                    } else {
                        sum += value;
                    }
                }
                i--;
            }
            if ( sample_count ) {
                item[ new_key_name ] = sum / sample_count;
            } else {
                item[ new_key_name ] = item[ key_name ];
            }
        } );
    }

    /**
     * Not used
     * @param key_name
     * @param depth
     * @param bCutLeftEdge
     * @constructor
     */
    FillWMA( key_name, depth, bCutLeftEdge = true ) {
        const new_key_name = key_name + "_wma";
        const min_index = bCutLeftEdge ? ( depth - 1 ) : 0;
        this.visual_data.pixels.forEach( ( item, index ) => {
            if ( index < min_index ) {
                return;
            }
            let sample_count = depth;
            let i = sample_count - 1;
            let sum = 0;
            while ( i >= 0 ) {
                const prev_item = this.visual_data.pixels[ index - i ];
                if ( prev_item === undefined ) {
                    sample_count--;
                } else {
                    const value = prev_item[ key_name ];
                    if ( value === undefined ) {
                        sample_count--;
                    } else {
                        sum += value * ( sample_count - i );
                    }
                }
                i--;
            }
            if ( sample_count ) {
                item[ new_key_name ] = sum / ( sample_count * ( sample_count + 1 ) / 2 );
            } else {
                item[ new_key_name ] = item[ key_name ];
            }
        } );
    }

    /**
     * Not used
     * @param key_name
     * @param depth
     * @param bCutLeftEdge
     * @constructor
     */
    FillEMA( key_name, depth, bCutLeftEdge = true ) {
        const new_key_name = key_name + "_ema";
        const min_index = bCutLeftEdge ? ( depth - 1 ) : 0;
        //k=2/(N+1)
        const k = 2 / ( depth + 1 );
        this.visual_data.pixels.forEach( ( item, index ) => {
            if ( index < min_index ) {
                return;
            }
            const value = item[ key_name ];
            if ( value !== undefined ) {
                const prev_item = this.visual_data.pixels[ index - 1 ];
                if ( prev_item === undefined ) {
                    item[ new_key_name ] = value;
                } else {
                    const prev_ema = prev_item[ new_key_name ];
                    if ( prev_ema === undefined ) {
                        item[ new_key_name ] = value;
                    } else {
                        //EMA=Price(t)*k+EMA(y)*(1−k)
                        item[ new_key_name ] = value * k + prev_ema * ( 1 - k );
                    }
                }
            }
        } );
    }

    /**
     * Converts Date object to the number of days since Epoch (negative if before Epoch).
     * @param date
     * @returns {number}
     * @private
     */
    _DateToDayNumber( date ) {
        return date.valueOf() / ( 24 * 60 * 60 * 1000 );
    }

    /**
     * Returns YYYY-MM part of ISO date string.
     * @param iso_s
     * @returns {string}
     * @private
     */
    _ISOToMonthString( iso_s ) {
        return iso_s.substring( 0, 7 );
    }

    /**
     * Converts Date object to ISO date string and returns YYYY-MM part.
     * @param date
     * @returns {string}
     * @private
     */
    _DateToMonthString( date ) {
        return this._ISOToMonthString( date.toISOString() );
    }

    /**
     * Converts ISO date string to the number of days since Epoch (negative if before Epoch).
     * @param s
     * @returns {number}
     * @constructor
     */
    DateStringToDayNumber( s ) {
        return this._DateToDayNumber( new Date( s ) );
    }

    /**
     * Converts possibly non-standard ISO date string to YYYY-MM part of ISO date string via String-Date-String conversion.
     * @param s
     * @returns {string}
     * @constructor
     */
    DateStringToMonthString( s ) {
        return this._DateToMonthString( new Date( s ) );
    }

    /**
     * Returns aggregate parameters of data.
     * @param data
     * @returns {{}}
     * @constructor
     */
    GetStats( data ) {
        let stats = {};
        if ( data ) {
            // earliest date of the dataset (ISO string)
            stats.first_date = "9999-99-99";
            // latest date of the dataset (ISO string)
            stats.last_date = "0000-00-00";
            data.forEach( month => {
                month.days && month.days.forEach( item => {
                    if ( item.t < stats.first_date ) {
                        stats.first_date = item.t;
                    }
                    if ( item.t > stats.last_date ) {
                        stats.last_date = item.t;
                    }
                    if ( stats.max_value === undefined ) {
                        // max value of the dataset
                        stats.max_value = item.v;
                    } else if ( item.v > stats.max_value ) {
                        stats.max_value = item.v;
                    }
                    if ( stats.min_value === undefined ) {
                        // min value of the dataset
                        stats.min_value = item.v;
                    } else if ( item.v < stats.min_value ) {
                        stats.min_value = item.v;
                    }
                } );
            } );
            // distance from Epoch of the earliest date of the dataset in days
            stats.first_day_number = this.DateStringToDayNumber( stats.first_date );
            // distance from Epoch of the latest date of the dataset in days
            stats.last_day_number = this.DateStringToDayNumber( stats.last_date );
            stats.first_year = stats.first_date.substring( 0, 4 );
            stats.last_year = stats.last_date.substring( 0, 4 );
        }
        return stats;
    }

    /**
     * Converts the sample data value to the Y coordinate of the current graph using internal data calculation structure.
     * @param v
     * @returns {number}
     * @constructor
     */
    ValueToY( v ) {
        return Math.round( this.visual_data.height - this.visual_data.border_bottom - ( ( v - this.visual_data.min_value ) / this.visual_data.values_per_pixel ) );
    }

    /**
     * Converts the Y coordinate of the current graph to sample data value using externally passed calculation data.
     * Used outside DataProcessor instance.
     * @param visual_data
     * @param y
     * @returns {*}
     * @constructor
     */
    YToValueIndirect( visual_data, y ) {
        return ( visual_data.height - visual_data.border_bottom - y ) * visual_data.values_per_pixel + visual_data.min_value;
    }

    /**
     *  Converts the Y coordinate of the current graph to sample data value using internal data calculation structure.
     * @param y
     * @returns {*}
     * @constructor
     */
    YToValue( y ) {
        return this.YToValueIndirect( this.visual_data, y );
    }

    /**
     * Creates internal data calculation structure.
     * @param data - DB data from storage
     * @param width - total width of the canvas
     * @param height - total height of the canvas
     * @param borders - {top,bottom,left,right} borders to be left free of graph points
     * @constructor
     */
    CreateVisualData( data, width, height, borders ) {
        let stats = this.GetStats( data );
        //console.log(stats);

        // internal data calculation structure
        this.visual_data = {
            ...stats,
            pixels: [],
            width: width,
            height: height,
            border_top: borders.top,
            border_bottom: borders.bottom,
            border_left: borders.left,
            border_right: borders.right,
        };

        if ( data ) {

            // graph width in pixels
            this.visual_data.graph_width = this.visual_data.width - this.visual_data.border_left - this.visual_data.border_right;
            // graph height in pixels
            this.visual_data.graph_height = this.visual_data.height - this.visual_data.border_top - this.visual_data.border_bottom;

            // count of days per graph pixel
            this.visual_data.days_per_pixel = ( this.visual_data.last_day_number - this.visual_data.first_day_number ) / ( this.visual_data.graph_width );
            // count of Y-data-units per graph pixel
            this.visual_data.values_per_pixel = ( this.visual_data.max_value - this.visual_data.min_value ) / ( this.visual_data.graph_height );

            // current graph x coordinate to collect data for
            let current_x = 0;

            // sum of data values to calculate average
            let sum_v = 0;
            // max data value
            let max_v;
            // min data value
            let min_v;
            // latest date (ISO string)
            let max_t;
            // earliest date (ISO string)
            let min_t;
            // array of all values at this graph x coordinate
            let samples = [];

            // zero data value Y
            this.visual_data.zero_y = this.ValueToY( 0 );

            // store collected data
            let collect = () => {
                let avg_v = sum_v / samples.length;
                // internal data are indexed by canvas (not graph) coordinates
                this.visual_data.pixels[ Math.trunc( this.visual_data.border_left + current_x ) ] = {
                    max_t: max_t,
                    min_t: min_t,
                    max_v: max_v,
                    min_v: min_v,
                    avg_v: avg_v,
                    max: this.ValueToY( max_v ),
                    min: this.ValueToY( min_v ),
                    avg: this.ValueToY( avg_v ),
                    samples: samples,
                };
            }

            // Data is stored using one-month blocks:
            // "A record for an individual month of meteorological measurements must be stored as a separate object/record in IndexedDB."
            data.forEach( month => {
                month.days && month.days.forEach( item => {

                    // item.t - date (ISO string) - implicit sort order
                    // item.tn - count of days since Epoch
                    // item.v - data value

                    // graph x coordinate of the current data item
                    let x = ( item.tn - this.visual_data.first_day_number ) / this.visual_data.days_per_pixel;
                    // graph y coordinate of the current data item
                    let y = this.ValueToY( item.v );

                    // previous coordinate collection has ended
                    if ( x > current_x ) {
                        collect();
                        current_x = Math.trunc( x ) + 1;
                        sum_v = 0;
                        max_v = undefined;
                        min_v = undefined;
                        max_t = undefined;
                        min_t = undefined;
                        samples = [];
                    }

                    // collect data for the current item
                    samples.push( y );
                    sum_v += item.v;
                    if ( max_v === undefined ) {
                        max_v = item.v;
                    } else if ( item.v > max_v ) {
                        max_v = item.v;
                    }
                    if ( min_v === undefined ) {
                        min_v = item.v;
                    } else if ( item.v < min_v ) {
                        min_v = item.v;
                    }
                    if ( max_t === undefined ) {
                        max_t = item.t;
                    } else if ( item.t > max_t ) {
                        max_t = item.t;
                    }
                    if ( min_t === undefined ) {
                        min_t = item.t;
                    } else if ( item.t < min_t ) {
                        min_t = item.t;
                    }

                } );
            } );

            // collect last graph point data
            if ( samples.length ) {
                collect();
            }

            // calculate optimal Y-grid
            {
                // range of data values
                this.visual_data.value_height = this.visual_data.max_value - this.visual_data.min_value;
                // log10 scale of data values
                this.visual_data.value_scale = Math.round( Math.log10( this.visual_data.value_height ) ) - 1;
                // Y-grid lines data
                this.visual_data.y_grid = [];
                // autoscaled grid step (pow10-divisible)
                const grid_increment = Math.pow( 10, this.visual_data.value_scale );

                // lowest grid line coordinate (pow10-divisible)
                let grid_value = Math.trunc( this.visual_data.min_value / grid_increment ) * grid_increment;

                while ( grid_value < this.visual_data.max_value ) {
                    this.visual_data.y_grid.push( {
                        // label
                        v: grid_value,
                        // canvas Y-coord
                        y: this.ValueToY( grid_value )
                    } );
                    grid_value += grid_increment;
                }
            }

            // calculate optimal X-grid
            {
                const first_year = parseInt( this.visual_data.first_year );
                const last_year = parseInt( this.visual_data.last_year );
                // range of year numbers
                this.visual_data.t_width = last_year - first_year + 1;
                // log10 scale of year numbers
                this.visual_data.t_scale = Math.max( 0, Math.round( Math.log10( this.visual_data.t_width ) ) - 1 );
                // X-grid lines data
                this.visual_data.x_grid = [];
                // autoscaled grid step (pow10-divisible)
                const grid_increment = Math.pow( 10, this.visual_data.t_scale );

                // proposed leftmost grid line coordinate (pow10-divisible)
                let grid_value = Math.trunc( first_year / grid_increment ) * grid_increment;

                while ( grid_value <= last_year ) {
                    // real leftmost grid line coordinate is always the first year number
                    const t = Math.max( grid_value, first_year );
                    const day_number = this.DateStringToDayNumber( String( t ) + "-01-01" );
                    this.visual_data.x_grid.push( {
                        // label
                        t: t,
                        tn: day_number,
                        // canvas X-coord
                        x: ( day_number - this.visual_data.first_day_number ) / this.visual_data.days_per_pixel + this.visual_data.border_left,
                    } );
                    grid_value += grid_increment;
                }
            }
        }
    }

    /**
     * Converts file data (flat array of day records) to DB data (array of arrays of day records grouped by month).
     * "A record for an individual month of meteorological measurements must be stored as a separate object/record in IndexedDB."
     * @param data
     * @returns {*[]}
     * @constructor
     */
    PreprocessRawData( data ) {
        let data_by_month = {};
        data.forEach( item => {
            const date = new Date( item.t );
            const iso_s = date.toISOString();
            item.t = iso_s.substring( 0, 10 );
            item.tn = this._DateToDayNumber( date );
            const month = this._ISOToMonthString( iso_s );
            if ( !data_by_month[ month ] ) {
                data_by_month[ month ] = [];
            }
            data_by_month[ month ].push( item );
        } );
        let output_data = [];
        for ( let key in data_by_month ) {
            output_data.push( { month: key, days: data_by_month[ key ] } );
        }
        return output_data;
    }

    /**
     * Returns all year numbers contained in the dataset time range (including those omitted in the dataset).
     * @param data
     * @returns {*[]}
     * @constructor
     */
    GetYears( data ) {
        let stats = this.GetStats( data );
        const first_year = parseInt( stats.first_date.substring( 0, 4 ) );
        const last_year = parseInt( stats.last_date.substring( 0, 4 ) );
        let years = [];
        for ( let i = first_year; i <= last_year; i++ ) {
            years.push( i.toString() );
        }
        return years;
    }

    /**
     * Returns internal data calculation structure for external usage.
     * @returns {*}
     * @constructor
     */
    GetVisualData() {
        return this.visual_data;
    }

}
