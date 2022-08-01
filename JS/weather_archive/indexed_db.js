/**
 * IndexedDB promise-based helper.
 * Used by DataStorage class in the web worker context.
 */
class IndexedDB {

    constructor() {
    }

    /**
     * Initialization
     * @param options
     * @constructor
     */
    Init( options ) {
        this.options = { ...options };
    }

    /**
     * @returns {boolean} true if DB / table already opened
     * @constructor
     */
    IsOpened() {
        return !!this.db;
    }

    /**
     * Creates new table (object store).
     * Called from within upgradeneeded event handler.
     * @param db
     * @param table_name
     * @param schema
     * @returns {Promise<unknown>}
     * @constructor
     */
    CreateTable( db, table_name, schema ) {
        const object_store = db.createObjectStore( table_name, schema );
        return new Promise( ( resolve, reject ) => {
            object_store.transaction.onerror = event => {
                reject( event );
            }
            object_store.transaction.onabort = event => {
                reject( event );
            }
            object_store.transaction.oncomplete = () => {
                resolve();
            }
        } );
    }

    /**
     * Opens DB connection.
     * If the table needed not found, recreates connection in version upgrade mode and creates table.
     * @param db_name
     * @param table_name
     * @param schema
     * @param version
     * @returns {Promise<void>|Promise<unknown>}
     * @constructor
     */
    OpenTable( db_name, table_name, schema, version ) {

        return this.TableExists( table_name ) ? Promise.resolve() : new Promise( ( resolve, reject ) => {

            // dummy upgrade to wait for
            let upgrade_operation = Promise.resolve();

            const open_request = indexedDB.open( db_name, version );

            open_request.onupgradeneeded = event => {
                // real upgrade to wait for
                upgrade_operation = this.CreateTable( event.target.result, table_name, schema );
            }

            open_request.onerror = event => {
                this.options.log.DisplayError( "Error opening cache for " + table_name + " from IndexedDB : " + event.target.error );
                reject( event );
            }

            open_request.onblocked = event => {
                this.options.log.DisplayError( "Blocked opening cache for " + table_name + " from IndexedDB" );
                reject( event );
            }

            open_request.onsuccess = event => {
                this.db = event.target.result;
                upgrade_operation.then( () => {

                    if ( version === undefined && !this.TableExists( table_name ) ) {

                        // table is not found
                        // reopening DB in upgrade mode to add new table schema
                        const new_version = this.db.version + 1;
                        this.db.close();
                        this.db = undefined;
                        this.OpenTable( db_name, table_name, schema, new_version ).then( db => {
                            // table is created
                            this.db = db;
                            resolve( this.db );
                        } ).catch( e => {
                            reject( e );
                        } );

                    } else {
                        // table is found, upgrade is not needed
                        resolve( this.db );
                    }

                } );
            }
        } );
    }

    /**
     * @param table_name
     * @returns {boolean} true if table exists, false if not open or table doesn't exist
     * @constructor
     */
    TableExists( table_name ) {
        let bResult = this.IsOpened();
        if ( bResult ) try {
            // throws if there is no table
            const transaction = this.db.transaction( table_name, "readonly" );
            const object_store = transaction.objectStore( table_name );
            object_store.count();
        } catch ( e ) {
            bResult = false;
        }
        return bResult;
    }

    /**
     * Call the specified method of objectStore
     * @param table_name
     * @param query_name - objectStore method name
     * @param key_range - params to pass to the method
     * @returns {Promise<unknown>}
     * @constructor
     */
    Query( table_name, query_name, key_range ) {
        return new Promise( ( resolve, reject ) => {
            try {
                const transaction = this.db.transaction( table_name, "readonly" );
                const object_store = transaction.objectStore( table_name );
                const count_request = object_store[ query_name ]( key_range );
                count_request.onsuccess = event => {
                    resolve( event.target.result );
                };
                count_request.onerror = event => {
                    reject( event );
                };
            } catch ( e ) {
                reject( e );
            }
        } );
    }

    /**
     * Returns table row count.
     * @param table_name
     * @returns {Promise<*>}
     * @constructor
     */
    GetRowCount( table_name ) {
        return this.Query( table_name, "count" );
    }

    /**
     * Inserts rows into the table.
     * @param table_name
     * @param data
     * @param before_add - callback to call before adding a row
     * @returns {Promise<unknown>}
     * @constructor
     */
    InsertRows( table_name, data, before_add ) {
        return new Promise( ( resolve, reject ) => {
            try {
                const transaction = this.db.transaction( table_name, "readwrite" );
                transaction.oncomplete = () => {
                    resolve();
                };
                transaction.onerror = () => {
                    reject();
                };
                const object_store = transaction.objectStore( table_name );
                data.forEach( ( item ) => {
                    before_add && before_add( item );
                    object_store.add( item );
                } );
            } catch ( e ) {
                this.options.log.DisplayError( "Error inserting " + table_name + " in IndexedDB: " + e );
                reject( e );
            }
        } );
    }

    /**
     * Returns table rows with the optional primary key filtering.
     * @param table_name
     * @param key_from
     * @param key_to
     * @returns {Promise<*>}
     * @constructor
     */
    Fetch( table_name, key_from, key_to ) {
        let key_range;
        if ( key_from !== undefined && key_to !== undefined ) {
            key_range = IDBKeyRange.bound( key_from, key_to );
        } else if ( key_from !== undefined ) {
            key_range = IDBKeyRange.lowerBound( key_from );
        } else if ( key_to !== undefined ) {
            key_range = IDBKeyRange.upperBound( key_to );
        }
        return this.Query( table_name, "getAll", key_range );
    }

    /**
     * Closes DB handle.
     * @constructor
     */
    Close() {
        this.db.close();
        this.db = undefined;
    }
}
