
#ifndef ZPAQCommpress_H
#define ZPAQCommpress_H
#include "CompComm.hpp"
#include "libzpaq.h"

namespace ZPAQ
{

  class In : public libzpaq::Reader 
  {
  private:
    MyIOBase *S;
  public:
    In(MyIOBase *IO):S(IO){};
    int get() {return S->getbyte();}  // returns byte 0..255 or -1 at EOF
    int read(char* buf, int n) // read to buf[n], return no. read
    { return S->read(buf,n);}
  };

  class Out: public libzpaq::Writer 
  {
  private:
    MyIOBase *S;
  public:
    Out(MyIOBase *IO):S(IO){};
    void put(int c) {S->putbyte(c);}  // writes 1 byte 0..255
    void write(const char* buf, int n)  // write buf[n]
    { S->write(buf,n);}
  };

/*  int main() {
    libzpaq::compress(&in, &out, 2);  // level may be 1, 2, or 3
  }
*/
}
class ZPAQCoder : public CoderBase //Encode class
{
private:

    inline void Compress(MyIOBase *Source)
    {
        ZPAQ::In in;
        ZPAQ::Out out;
        LPQ8::Encoder En(&G,LPQ8::COMPRESS,Archive);
        for (unsigned long i = 0; i < Head.usize; i++)
        {
            En.compress(Source->getbyte());
        }
        En.flush();
    }
    inline void UnCompress(MyIOBase *Source)
    {
        G.init();
        LPQ8::Encoder De(&G,LPQ8::DECOMPRESS,Archive);
        De.deinit();
        for (unsigned long i = 0; i < Head.usize; i++)
        {
            Source->putbyte(De.decompress());
        }
    }
public:
    ZPAQCoder(bool m,MyIOBase *p=NULL):CoderBase(p){MathodN="LPQ8";Mode=m;}
    //Mode: true for encode,false for decode
};
#endif
