An associative array with change monitoring.

Key/value storage (std::map\< std::string, std::string \>) allowing to check which pairs are updated/added/deleted.

Useful for getting the minimal amount of operations needed to update remote representaion of data after local data has being modified by the higher-level logic.

The idea is the same as in [JS/graph_editor/buffered_observer.js](../../JS/graph_editor) .
