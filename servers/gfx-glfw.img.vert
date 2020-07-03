uniform mat4 my_model;
uniform mat4 my_projection;

attribute vec2 my_position;
attribute vec2 my_tex_uv;

void main() {
  gl_TexCoord[0].xy = my_tex_uv;
  gl_Position = my_projection * my_model * vec4(my_position, 0.0, 1.0);
}
