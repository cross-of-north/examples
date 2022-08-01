/**
 * File URL / Indexed DB data source.
 * Used in the web worker context.
 */
class DataStorage {

    constructor() {
    }

    /**
     * Initialization
     * @param options
     * @constructor
     */
    Init( options ) {
        this.options = { ...options };
        if ( this.options.log && !this.options.log.DisplayStatus ) {
            // worker context
            this.options.log = new RemoteLog();
        }
        this.db = new IndexedDB();
        this.db.Init( this.options );
    }

    /**
     * Loads data from URL when data is not cached in IndexedDB yet
     * @returns {Promise<any>}
     * @constructor
     */
    async GetFileContents() {
        this.options.log.DisplayStatus( "Loading file..." );
        let url = this.options.base_url + this.options.table_name;
        const response = await fetch( url )
        if ( response.ok ) {
            return await response.json();
        } else {
            throw String( url + " - invalid server response: " + response.status );
        }
    }

    /**
     * Opens/creates IndexedDB and table needed.
     * If table is empty, tries to load data into the IndexedDB from URL.
     * @returns {Promise<void>}
     * @constructor
     */
    async Open() {
        this.options.log.DisplayStatus( "Checking cache..." );
        await this.db.OpenTable( this.options.database_name, this.options.table_name, this.options.schema, this.options.version );
        const row_count = await this.db.GetRowCount( this.options.table_name );
        if ( !row_count ) {
            const data_read = await this.GetFileContents();
            this.options.log.DisplayStatus( "Caching..." );
            // Conversion is needed because of the requirement:
            // "A record for an individual month of meteorological measurements must be stored as a separate object/record in IndexedDB."
            const rows = ( new DataProcessor() ).PreprocessRawData( data_read );
            await this.db.InsertRows( this.options.table_name, rows, this.options.on_new_item );
        }
        this.options.log.DisplaySuccess( "Opened." );
    }

    /**
     * Returns dataset (loading data from IndexedDB/URL if needed)
     * @param key_from - optional lower inclusive bound (ISO month string, i.e. 1970-01)
     * @param key_to - optional upper inclusive bound (ISO month string, i.e. 1970-01)
     * @returns {Promise<unknown>}
     * @constructor
     */
    async GetData( key_from, key_to ) {
        await this.Open();
        return await this.db.Fetch( this.options.table_name, key_from, key_to );
    }

    /**
     * Close underlying IndexedDB handle (to possibly re-open it with upgrade to store a new table).
     * @constructor
     */
    Close() {
        this.db.Close();
    }
}
