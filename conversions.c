#include <stdio.h>
#include <string.h>
#include <stdlib.h>

char *convert_str(unsigned char *in_string, int string_length)
{
   char *out_string;

   out_string = (char *)calloc(string_length+1,sizeof(char));
   return(memcpy(out_string, in_string, string_length));
   
}

float convert_r4(unsigned char *in)
{
   union {
      float r4;
      unsigned char ch[4];
   } output;

   output.ch[1] = *(in++);
   output.ch[0] = *(in++);
   output.ch[3] = *(in++);
   output.ch[2] = *in;
   return(output.r4/4);
}

int convert_i4(unsigned char *in)
{
   union {
      int i4;
      unsigned char ch[4];
   } output;

   output.ch[3] = *(in++);
   output.ch[2] = *(in++);
   output.ch[1] = *(in++);
   output.ch[0] = *in;
   return(output.i4);
}

short convert_i2(unsigned char *in)
{
   union {
      short i2;
      unsigned char ch[2];
   } output;

   output.ch[1] = *(in++);
   output.ch[0] = *in;
   return(output.i2);
}

