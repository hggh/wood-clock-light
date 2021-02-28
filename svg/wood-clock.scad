difference() {
  cube(size = [264,86,13]);
  for (e = [ 0 : 4 ]) {
    for (i = [0 : 16]) {
      translate([(20 + (i * 14)),17 + (e *13),0]) {
        cylinder (h = 12.2, r=4, center=false);
      }
    }
  }
}

