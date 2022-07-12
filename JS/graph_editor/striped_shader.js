'use strict';
define( [ "pixi", "rng" ], function( PIXI, rng ) {

function StripedShader( shaderManager ) {

  function load_shader_from_script( id ) {
    return document.getElementById( id ).innerHTML;
  }

  function load_shader_from_function( f ) {
    return f.toString().match(/[^]*\/\*\!([^]*)\!\*\/\}$/)[1];
  }

  function striped_vertex_shader() {/*!
    attribute vec2 aVertexPosition;
    attribute vec4 aColor;

    uniform mat3 translationMatrix;
    uniform mat3 projectionMatrix;

    uniform float alpha;
    uniform float flipY;
    uniform vec3 tint; 

    varying vec4 vColor;
    varying float Scale;

    void main(void){
       Scale = abs((projectionMatrix * translationMatrix * vec3(1., 0., 1.)).x);
       gl_Position = vec4((projectionMatrix * translationMatrix * vec3(aVertexPosition, 1.0)).xy, 0.0, 1.0);
       vColor = aColor * vec4(tint * alpha, alpha);
    }
  !*/}

  function striped_fragment_shader() {/*!
    precision mediump float;   
    varying float Scale;
    varying vec4 vColor;
    void main() {
      float packed_alpha = vColor[ 3 ];
      if ( packed_alpha > 1. ) {
        
        // unpack packed data
        float unpacked_aplha = mod( packed_alpha, 16. ) / 15.;
        gl_FragColor = vColor / packed_alpha * unpacked_aplha; // PIXI multiplies color components by packed alpha, we need to demultiply it and multiply again with correct unpacked value
        gl_FragColor[ 3 ] = unpacked_aplha;
        float DashLength = floor( mod( packed_alpha / 16., 64. ) );
        float TotalLength = DashLength + floor( mod( packed_alpha / 16. / 64., 64. ) );
        float Angle = floor( mod( packed_alpha / 16. / 64. / 64., 64. ) ) / 10.
          -0.043 // magic fractional part of PI to compensate roundoff
        ;
        float Slope = tan( Angle );
        float Slope2 = Slope+1./Slope;
        
        float b1 = gl_FragCoord.y - Slope * gl_FragCoord.x; 
        float x0 = -b1 / Slope2;   
        float y0 = Slope * x0 + b1; 
        float dx = gl_FragCoord.x - x0; 
        float dy = gl_FragCoord.y - y0; 
        float absolute_length = length( vec2( dx, dy ) );
        //absolute_length /= Scale;
        TotalLength *= Scale;
        DashLength *= Scale;
        if ( dy < 0. ) {  
          absolute_length = -absolute_length; 
        } 
        float current_period = mod(absolute_length, TotalLength); 
        float delta = current_period - DashLength;  
        //delta *= Scale;

        // antialiasing
        if (delta < 0. && delta > -1. ) {
          gl_FragColor *= -delta;
        } else
        if ( delta < -DashLength + 1. && delta > -DashLength ) {
          gl_FragColor *= delta + DashLength;
          //gl_FragColor = vec4(0.,0.,0.,1.);
        } else
        if ( delta >= 0. ) {  
          discard;  
        } 

      } else {
        gl_FragColor = vColor;
      }
    }
  !*/}
  
  PIXI.Shader.call(this,
        shaderManager,
        // vertex shader
        load_shader_from_function( striped_vertex_shader ),
        //load_shader_from_script( "striped_vertex_shader" ),
        // fragment shader
        load_shader_from_function( striped_fragment_shader ),
        //load_shader_from_script( "striped_fragment_shader" ),
        // custom uniforms
        {
            tint:   { type: '3f', value: [0, 0, 0] },
            alpha:  { type: '1f', value: 0 },
            translationMatrix: { type: 'mat3', value: new Float32Array(9) },
            projectionMatrix: { type: 'mat3', value: new Float32Array(9) }
        },
        // custom attributes
        {
            aVertexPosition:0,
            aColor:0
        }
    );
}

// The object is drawn in a striped space.
// Stripes are defined by the line equation.
// Line is drawn via current point, and normal to the stripes.
// The stripe mode (on/off) is defined by the distance from the current point to the crossing point of the normal line and Y-axis.

StripedShader.prototype = Object.create(PIXI.Shader.prototype);
StripedShader.prototype.constructor = StripedShader;
StripedShader.prototype.pack_alpha = function ( dash_length, space_length, angle, alpha ) {
  // mediump float is C-float, 23 precise bits or approx. 7 precise decimal digits
  // packed data by bits:
  // ..2.........1.........0
  // 21098765432109876543210
  //                    xxxx alpha * 15 (0.0-1.0)
  //              xxxxxx     dash length (0-63)
  //        xxxxxx           space length (0-63)
  //  xxxxxx                 angle * 10 (0.0-2pi=6.3)
  // x                       reserved, always 0
  // 21098765432109876543210
  // ..2.........1.........0
  //
  // angle is mirrored (2*PI-angle) since PIXI Y-axis direction is opposite to the WebGL
  if ( alpha === undefined ) {
    alpha = 1;
  }
  return ( ( Math.ceil( dash_length & 0x3f ) + ( Math.ceil( space_length & 0x3f ) << 6 ) + ( Math.ceil( ( (2*Math.PI-angle) % (2*Math.PI) ) * 10. ) << 12 ) ) << 4 ) + Math.ceil( alpha * 15 );
}

PIXI.ShaderManager.prototype.old_onContextChange = PIXI.ShaderManager.prototype.onContextChange;
PIXI.ShaderManager.prototype.onContextChange = function() {
  this.old_onContextChange();
  this.primitiveShader = new StripedShader(this);
  this.primitiveShader.syncUniforms();
}

return {
  PackAlpha: StripedShader.prototype.pack_alpha, // function ( alpha, dash_length, space_length, angle )
z:0};

});