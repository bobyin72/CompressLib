#ifndef LZWCommpress_H
#define LZWCommpress_H

#include "CompComm.hpp"

namespace LZW
{
  enum{BITS=14,                   /* Setting the number of bits to 12, 13*/
       HASHING_SHIFT=(BITS-8),    /* or 14 affects several constants.    */
       MAX_VALUE=(1 << BITS) - 1, /* Note that MS-DOS machines need to   */
       MAX_CODE=MAX_VALUE - 1,    /* compile their code in large model if*/
       TABLE_SIZE=18041 };                             /* 14 bits are selected.               */

  class Coder
  {
  private:
  int *code_value;                  /* This is the code value array        */
  unsigned int *prefix_code;        /* This array holds the prefix codes   */
  unsigned char *append_character;  /* This array holds the appended chars */
  unsigned char decode_stack[4000]; /* This array holds the decoded string */
  MyIOBase *Arc;
  int input_bit_count;
  unsigned long input_bit_buffer;
  int output_bit_count;
  unsigned long output_bit_buffer;

  int find_match(unsigned int hash_prefix,unsigned char hash_character)
  {
  /*
  ** This is the hashing routine.  It tries to find a match for the prefix+char
  ** string in the string table.  If it finds it, the index is returned.  If
  ** the string is not found, the first available index in the string table is
  ** returned instead.
  */
  int index= (hash_character << HASHING_SHIFT) ^ hash_prefix;
  int offset=(index == 0)?1:(TABLE_SIZE - index);

    while (1)
    {
      if (code_value[index] == -1)
        return(index);
      if (prefix_code[index] == hash_prefix && 
          append_character[index] == hash_character)
        return(index);
      index -= offset;
      if (index < 0)
        index += TABLE_SIZE;
    }
  }
  void output_code(unsigned int code)
  {
    output_bit_buffer |= (unsigned long) code << (32-BITS-output_bit_count);
    output_bit_count += BITS;
    while (output_bit_count >= 8)
    {
      Arc->putbyte(output_bit_buffer >> 24);
      output_bit_buffer <<= 8;
      output_bit_count -= 8;
    }
  }
  unsigned int input_code()
  {
    unsigned int return_value;

    while (input_bit_count <= 24)
    {
      input_bit_buffer |= 
          (unsigned long) Arc->getbyte() << (24-input_bit_count);
      input_bit_count += 8;
    }
    return_value=input_bit_buffer >> (32-BITS);
    input_bit_buffer <<= BITS;
    input_bit_count -= BITS;
    return(return_value);
  }
  unsigned char *decode_string(unsigned char *buffer,unsigned int code)
  {/*
  ** This routine simply decodes a string from the string table, storing
  ** it in a buffer.  The buffer can then be output in reverse order by
  ** the expansion program.
  */
    int i=0;
    while (code > 255)
    {
      *buffer++ = append_character[code];
      code=prefix_code[code];
      if (i++>=MAX_CODE)
      {
        printf("Fatal error during code expansion.\n");
        exit(-3);
      }
    }
    *buffer=code;
    return(buffer);
  }
  public:
  Coder()
  {
    code_value=new int[TABLE_SIZE];
    prefix_code=new unsigned int [TABLE_SIZE];
    append_character=new unsigned char[TABLE_SIZE];
  }
  void init(MyIOBase *A)
  {
    for (int i=0;i<TABLE_SIZE;i++)  /* Clear out the string table before starting */
      code_value[i]=-1;
    memset(prefix_code,0,TABLE_SIZE*sizeof(unsigned int));
    memset(append_character,0,TABLE_SIZE*sizeof(unsigned char));
    input_bit_count=0;
    input_bit_buffer=0L;
    output_bit_count=0;
    output_bit_buffer=0L;
    Arc=A;
  }
  ~Coder(){delete []code_value; delete []prefix_code; delete []append_character;}
  void compress(MyIOBase *input)
  {
    unsigned int next_code=256; /* Next code is the next available string code*/
    unsigned char character;
    unsigned int string_code;
    unsigned int index;
    int c;


    string_code=input->getbyte();    /* Get the first code                         */
  /*
  ** This is the main loop where it all happens.  This loop runs util all of
  ** the input has been exhausted.  Note that it stops adding codes to the
  ** table after all of the possible codes have been defined.
  */
    while ((c=input->getbyte()) != EOF)
    {
      character=c;
      index=find_match(string_code,character);/* See if the string is in */
      if (code_value[index] != -1)            /* the table.  If it is,   */
        string_code=code_value[index];        /* get the code value.  If */
      else                                    /* the string is not in the*/
      {                                       /* table, try to add it.   */
        if (next_code <= MAX_CODE)
        {
          code_value[index]=next_code++;
          prefix_code[index]=string_code;
          append_character[index]=character;
        }
        output_code(string_code);  /* When a string is found  */
        string_code=character;            /* that is not in the table*/
      }                                   /* I output the last string*/
    }                                     /* after adding the new one*/
  /*
  ** End of the main loop.
  */
    output_code(string_code); /* Output the last code               */
    output_code(MAX_VALUE);   /* Output the end of buffer code      */
    output_code(0);           /* This code flushes the output buffer*/
  }
  
  void expand(MyIOBase *output)
  {
  unsigned int next_code=256;
  unsigned int new_code;
  unsigned int old_code;
  int character;
  unsigned char *string;

    old_code=input_code();  /* Read in the first code, initialize the */
    character=old_code;          /* character variable, and send the first */
    output->putbyte(old_code);       /* code to the output file                */
  /*
  **  This is the main expansion loop.  It reads in characters from the LZW file
  **  until it sees the special code used to inidicate the end of the data.
  */
    while ((new_code=input_code()) != (MAX_VALUE))
    {
  /*
  ** This code checks for the special STRING+CHARACTER+STRING+CHARACTER+STRING
  ** case which generates an undefined code.  It handles it by decoding
  ** the last code, and adding a single character to the end of the decode string.
  */
      if (new_code>=next_code)
      {
        *decode_stack=character;
        string=decode_string(decode_stack+1,old_code);
      }
  /*
  ** Otherwise we do a straight decode of the new code.
  */
      else
        string=decode_string(decode_stack,new_code);
  /*
  ** Now we output the decoded string in reverse order.
  */
      character=*string;
      while (string >= decode_stack)
        output->putbyte(*string--);
  /*
  ** Finally, if possible, add a new code to the string table.
  */
      if (next_code <= MAX_CODE)
      {
        prefix_code[next_code]=old_code;
        append_character[next_code]=character;
        next_code++;
      }
      old_code=new_code;
    }
  }
  };  
}//namespace
class LZWCoder : public CoderBase //Encode class
{
private:
    LZW::Coder C;
    inline void Compress(MyIOBase *Source)
    {
        C.init(Archive);
        C.compress(Source);
    }
    inline void UnCompress(MyIOBase *Source)
    {
        C.init(Archive);
        C.expand(Source);
    }
public:
    LZWCoder(bool m,MyIOBase *p=NULL):CoderBase(p)
    {
      MathodN=" LZW";
      Mode=m;
    }
    //Mode: true for encode,false for decode
};
#endif
