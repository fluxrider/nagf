uniform mat4 my_model;
uniform mat4 my_projection;

attribute vec2 my_position;

void main() {
  gl_Position = my_projection * my_model * vec4(my_position, 0.0, 1.0);
}
