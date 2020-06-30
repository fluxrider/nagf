uniform sampler2D my_sampler;
void main() {
  gl_FragColor = texture2D(my_sampler, gl_TexCoord[0].xy);
}
