/*
 * This is bin2c program, which allows you to convert binary file to
 * C language array, for use as embedded resource, for instance you can
 * embed graphics or audio file directly into your program.
 * This is public domain software, use it on your own risk.
 * Contact Serge Fukanchik at fuxx@mail.ru  if you have any questions.
 *
 * Some modifications were made by Gwilym Kuiper (kuiper.gwilym@gmail.com)
 * I have decided not to change the licence.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef USE_BZ2
#include <bzlib.h>
#endif

int
main (int argc, char *argv[])
{
  char *buf;
  char *ident;
  unsigned int i, file_size, need_comma;
  FILE *f_input, *f_output;

#ifdef USE_BZ2
  int status;
  char *bz2_buf;
  unsigned int uncompressed_size, bz2_size;
#endif

  if (argc < 4) {
    fprintf (stderr, "Usage: %s binary_file output_file array_name\n", argv[0]);
    return -1;
  }

  f_input = fopen (argv[1], "rb");
  if (f_input == NULL) {
    fprintf (stderr, "%s: can't open %s for reading\n", argv[0], argv[1]);
    return -1;
  }
  // Get the file length
  fseek (f_input, 0, SEEK_END);
  file_size = ftell (f_input);
  fseek (f_input, 0, SEEK_SET);

  if ((buf = malloc (file_size)) == NULL) {
    fprintf (stderr, "Unable to malloc bin2c.c buffer\n");
    fclose (f_input);
    return -1;
  }

  fread (buf, file_size, 1, f_input);
  fclose (f_input);

#ifdef USE_BZ2
  // allocate for bz2.
  bz2_size = (file_size + file_size / 100 + 1) + 600; // as per the documentation

  if ((bz2_buf = malloc (bz2_size)) == NULL) {
    fprintf (stderr, "Unable to malloc bin2c.c buffer\n");
    free (buf);
    return -1;
  }
  // compress the data
  status =
    BZ2_bzBuffToBuffCompress (bz2_buf, &bz2_size, buf, file_size, 9, 1, 0);
  if (status != BZ_OK) {
    fprintf (stderr, "Failed to compress data: error %i\n", status);
    free (buf);
    free (bz2_buf);
    return -1;
  }
  // and be very lazy
  free (buf);
  uncompressed_size = file_size;
  file_size = bz2_size;
  buf = bz2_buf;
#endif

  f_output = fopen (argv[2], "w");
  if (f_output == NULL) {
    fprintf (stderr, "%s: can't open %s for writing\n", argv[0], argv[1]);
    free (buf);
    return -1;
  }

  ident = argv[3];
  need_comma = 0;

  fprintf (f_output, "const char %s[%u] = {", ident, file_size);
  for (i = 0; i < file_size; ++i) {
    if (need_comma)
      fprintf (f_output, ", ");
    else
      need_comma = 1;
    if ((i % 11) == 0)
      fprintf (f_output, "\n\t");
    fprintf (f_output, "0x%.2x", buf[i] & 0xff);
  }
  fprintf (f_output, "\n};\n\n");
  fprintf (f_output, "const int %s_length = %u;\n", ident, file_size);

#ifdef USE_BZ2
  fprintf (f_output, "const int %s_length_uncompressed = %u;\n", ident,
           uncompressed_size);
#endif

  fclose (f_output);
  free (buf);

  return 0;
}
