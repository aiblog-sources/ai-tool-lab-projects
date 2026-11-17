#define Source ((char *)0x6000)
#define Dest ((char *)0x6100)
void copy(void) {
  static char index;
  index = 0;
  do {
    Dest[index] = Source[index];
    index++;
  } while (index);
}
void copy_local(void) {
  char index = 0;
  do {
    Dest[index] = Source[index];
    index++;
  } while (index);
}
void copy_uchar(void) {
  unsigned char index = 0;
  do {
    Dest[index] = Source[index];
    index++;
  } while (index);
}
