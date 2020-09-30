precision mediump float;

varying vec2 v_texCoord;
uniform vec2 texSize;
uniform sampler2D texture;
void main()
{
 vec2 texCoord=vec2(v_texCoord.x*texSize.x, v_texCoord.y*texSize.y);
 float oe=floor(mod(texCoord.x, 2.0));

 vec2 tc = vec2(v_texCoord.x, v_texCoord.y*3.0/2.0);
 vec4 yuyv=texture2D(texture, tc);
 float y=yuyv.p*oe+yuyv.s*(1.0-oe);
 y = float(v_texCoord.y<4.0/6.0) * y;

 float yu = (v_texCoord.y*3.0/2.0 - 1.0)*4.0 + float(v_texCoord.x>=0.5)*3.0/texCoord.y;
 tc = vec2(mod(v_texCoord.x*2.0, 1.0), yu);
 yuyv=texture2D(texture, tc);
 float u = yuyv.t;
 u = float(v_texCoord.y>=4.0/6.0 && v_texCoord.y<5.0/6.0) * u;

 y += u;

 float yv = (v_texCoord.y*3.0/2.0 - 5.0/4.0)*4.0 + float(v_texCoord.x>=0.5)*3.0/texCoord.y;
 tc = vec2(mod(v_texCoord.x*2.0, 1.0), yv);
 yuyv=texture2D(texture, tc);
 float v = yuyv.q;
 v = float(v_texCoord.y>=5.0/6.0) * v;

 y += v;

 gl_FragColor=vec4(y, y, y, 1.0);
}
