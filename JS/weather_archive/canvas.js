/**
 * HTML canvas helper
 */
class CanvasRenderer {

    constructor( element ) {
        this.canvas = element;
        this.Reset();
        this.SetPointStyle( {
            width: 1,
            height: 1,
        } );

        // visible but not usable one-pixel spacing
        this.x_incomplete_pixel = 1;
        this.y_incomplete_pixel = 1;
    }

    /**
     * Yield execution to the UI thread tasks during long drawing operations.
     * @param ms
     * @returns {Promise<unknown>}
     * @constructor
     */
    async Yield( ms ) {
        return new Promise( ( resolve ) => {
            setTimeout( () => resolve(), ms );
        } );
    }

    /**
     * Set line style for subsequent RenderLine(), RenderLines() and RenderText() calls.
     * @param line_style
     * @constructor
     */
    SetLineStyle( line_style ) {
        this.context.lineWidth = ( line_style.width === undefined ) ? 1 : line_style.width;
        this.context.strokeStyle = ( line_style.color === undefined ) ? "#000" : line_style.color;
        this.context.setLineDash( ( line_style.pattern === undefined ) ? [] : line_style.pattern );
    }

    /**
     * Draw a single line.
     * @param x1
     * @param y1
     * @param x2
     * @param y2
     * @constructor
     */
    RenderLine( x1, y1, x2, y2 ) {
        this.context.beginPath();
        // coordinates are processed to draw crisp line
        this.context.moveTo( Math.round( x1 ) + .5, Math.round( y1 ) + .5 );
        this.context.lineTo( Math.round( x2 ) + .5, Math.round( y2 ) + .5 );
        this.context.stroke();
    }

    /**
     * Draw objects of the dataset.
     * Yields execution after every 50 ms.
     * @param pixels - dataset structure
     * @param field_name - name of the field containing Y-values to draw
     * @param renderer - rendering function(x,y)
     * @returns {Promise<void>}
     * @constructor
     */
    async RenderObjects( pixels, field_name, renderer ) {

        // yielding mechanics
        let prev_yield_time = performance.now();
        let should_yield = () => {
            return performance.now() - prev_yield_time > 50;
        };
        let do_yield = async () => {
            prev_yield_time = performance.now();
            await this.Yield( 0 );
        };

        let index = 0;
        while ( index < pixels.length ) {
            const item = pixels[ index ];
            if ( item ) {
                const field_value = item[ field_name ];
                if ( field_value !== undefined ) {
                    if ( Array.isArray( field_value ) ) {
                        let value_index = 0;
                        while ( value_index < field_value.length ) {
                            // array of values (multiple samples are rendered at one pixel)
                            renderer( index, Math.round( field_value[ value_index ] ) )
                            value_index++;
                            if ( !( value_index % 100 ) && should_yield() ) await do_yield();
                        }
                    } else {
                        // single sample value
                        renderer( index, Math.round( field_value ) );
                    }
                    if ( !( index % 100 ) && should_yield() ) await do_yield();
                }
            }
            index++;
        }
    }

    /**
     * Draw a sequence of lines between points of the dataset.
     * Yields execution after every yield_after number of lines drawn.
     * @param pixels - dataset structure
     * @param field_name - name of the field containing points to draw lines between
     * @returns {Promise<void>}
     * @constructor
     */
    async RenderLines( pixels, field_name ) {
        this.context.beginPath();
        let bFirstPoint = true;
        await this.RenderObjects( pixels, field_name, ( x, y ) => {
            y = Math.round( y ) + .5;
            x = Math.round( x ) + .5;
            if ( bFirstPoint ) {
                bFirstPoint = false;
                this.context.moveTo( x, y );
            } else {
                this.context.lineTo( x, y );
            }
        } );
        this.context.stroke();
    }

    /**
     * Set point/fill style for subsequent RenderPoint(), RenderPoints() and RenderRect() operations.
     * @param point_style
     * @constructor
     */
    SetPointStyle( point_style ) {
        this.point_style = {
            ...this.point_style,
            ...point_style,
        };

        // points should be drawn centered at the coordinates specified
        this.point_style.x_offset = -Math.trunc( this.point_style.width / 2 );
        this.point_style.y_offset = -Math.trunc( this.point_style.height / 2 );

        this.context.fillStyle = ( point_style.color === undefined ) ? "#000" : point_style.color;
    }

    /**
     * Draw a single point.
     * @param x
     * @param y
     * @constructor
     */
    RenderPoint( x, y ) {
        // points should be drawn centered at the coordinates specified
        this.context.fillRect( x + this.point_style.x_offset, y + this.point_style.y_offset, this.point_style.width, this.point_style.height );
    }

    /**
     * Draw a filled rectangle.
     * @param x1
     * @param y1
     * @param x2
     * @param y2
     * @constructor
     */
    RenderRect( x1, y1, x2, y2 ) {
        this.context.fillRect( x1, y1, x2 - x1, y2 - y1 );
    }

    /**
     * Render a text string.
     * @param x
     * @param y
     * @param s
     * @constructor
     */
    RenderText( x, y, s ) {
        this.context.fillText( s, x, y );
    }

    /**
     * Returns the width of the string in pixels.
     * @param s
     * @returns {number}
     * @constructor
     */
    MeasureText( s ) {
        return this.context.measureText( s ).width;
    }

    /**
     * Draw a sequence of points.
     * Yields execution after every yield_after number of points drawn.
     * @param pixels - dataset structure
     * @param field_name - name of the field containing points to draw lines between
     * @returns {Promise<void>}
     * @constructor
     */
    async RenderPoints( pixels, field_name ) {
        await this.RenderObjects( pixels, field_name, this.RenderPoint.bind( this ) );
    }

    /**
     * Returns internal canvas size.
     * @returns {{width: number, height: number}}
     * @constructor
     */
    GetCanvasSize() {
        return {
            width: this.canvas.width - this.x_incomplete_pixel,
            height: this.canvas.height - this.y_incomplete_pixel,
        }
    }

    /**
     * Update internal canvas size after the external DOM canvas element size is changed.
     * @constructor
     */
    UpdateCanvasSize() {
        //https://stackoverflow.com/questions/19142993/how-draw-in-high-resolution-to-canvas-on-chrome-and-why-if-devicepixelratio/35244519#35244519
        const client_rect = this.canvas.getBoundingClientRect();
        const styles = getComputedStyle( this.canvas );
        // this way pixel crispness is preserved
        this.canvas.width = Math.round( devicePixelRatio * ( client_rect.right - parseFloat( styles.borderRightWidth ) ) ) - Math.round( devicePixelRatio * ( client_rect.left + parseFloat( styles.borderLeftWidth ) ) );
        this.canvas.height = Math.round( devicePixelRatio * ( client_rect.bottom - parseFloat( styles.borderBottomWidth ) ) ) - Math.round( devicePixelRatio * ( client_rect.top + parseFloat( styles.borderTopWidth ) ) );
    }

    /**
     * Translate coordinates from window to internal canvas.
     * @param point {{x: number, y: number}}
     * @returns {{x: number, y: number}}
     * @constructor
     */
    FromScreenToCanvasCoordinates( point ) {
        const rect = this.canvas.getBoundingClientRect();
        const styles = getComputedStyle( this.canvas );
        const border_width = {
            left: parseFloat( styles.borderLeftWidth ),
            right: parseFloat( styles.borderRightWidth ),
            top: parseFloat( styles.borderTopWidth ),
            bottom: parseFloat( styles.borderBottomWidth ),
        };
        return {
            x: ( ( point.x === undefined ? 0 : point.x ) - rect.left - border_width.left ) / ( rect.width - border_width.left - border_width.right ) * ( this.canvas.width - this.x_incomplete_pixel ),
            y: ( ( point.y === undefined ? 0 : point.y ) - rect.top - border_width.top ) / ( rect.height - border_width.top - border_width.bottom ) * ( this.canvas.height - this.y_incomplete_pixel ),
        };
    }

    /**
     * Clear canvas.
     * @constructor
     */
    Reset() {
        if ( this.context ) {
            // https://www.measurethat.net/Benchmarks/Show/14723/2/canvas-clearing-performance-v3
            // https://stackoverflow.com/questions/2142535/how-to-clear-the-canvas-for-redrawing/6722031#6722031
            this.context.beginPath();
            // Store the current transformation matrix
            this.context.save();
            // Use the identity matrix while clearing the canvas
            this.context.setTransform( 1, 0, 0, 1, 0, 0 );
            this.context.clearRect( 0, 0, this.canvas.width, this.canvas.height );
            // Restore the transform
            this.context.restore();
        }
        this.context = this.canvas.getContext( "2d" );
        this.context.imageSmoothingEnabled = false;
    }

}
