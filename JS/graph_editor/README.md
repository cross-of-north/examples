Graph editor parts.

- [buffered_observer.js](buffered_observer.js)

  An observer for JS objects recording history of changes.

  Useful for getting the minimal amount of operations needed to update remote representaion of data after local data has being modified by the higher-level logic.

  The idea is the same as in [C++/string_map_with_changes](../C++/string_map_with_changes) .
  
- [selection_rectangle.js](selection_rectangle.js)

  Selection rectangle logic (with additive/subtractive mode support).
  
- [striped_shader.js](striped_shader.js)

  The fast way to render dashed lines for [PixiJS](https://github.com/pixijs/pixijs) by using shaders.
