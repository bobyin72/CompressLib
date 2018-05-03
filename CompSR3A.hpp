#ifndef SR3ACompress_H
#define SR3ACompress_H
//This is slow but best compress ratio class
//based on SR3A - File archiver and compressor.
//    (C) 2003, Matt Mahoney, mmahoney@cs.fit.edu
//resutruct by  bobyin 2008

#include <assert.h>

namespace SR3A
{
    #include "CompComm.hpp"
    using namespace std;
    // ----------------------------------------------------------------------------
    // A StateMap maps a secondary context to a probability.  Methods:
    //
    // Statemap sm(n) creates a StateMap with n contexts using 4*n bytes memory.
    // sm.p(cxt) predicts next bit (0..4K-1) in context cxt (0..n-1).
    // sm.update(cxt, y) updates model for actual bit y (0..1) in cxt (0..n-1).

    class StateMap
    {
    protected:
      const int N;  // Number of contexts
      U32 *t;       // cxt -> prediction in high 25 bits, count in low 7 bits
      int dt[128];  // i -> 1K/(i+2)
    public:
      StateMap(int n): N(n)
      {
        t=new U32[N];
        for (int i=0; i<N; ++i)
            t[i]=1<<31;
        for (int i=0; i<128; ++i)
            dt[i]=512/(i+2);
      }
      ~StateMap(){delete []t;}
    // predict next bit in context cxt
      int p(int cxt)
      {
        assert(cxt>=0 && cxt<N);
        return t[cxt]>>20;
      }

      // update model in context cxt for actual bit y
      void update(int cxt, int y)
      {
        assert(cxt>=0 && cxt<N);
        assert(y==0 || y==1);
        int n=t[cxt]&127, p=t[cxt]>>9;  // count, prediction
        if (n<127) ++t[cxt];
        t[cxt]+=((y<<23)-p)*dt[n]&0xffffff80;
      }
    };

    // ----------------------------------------------------------------------------
    // An Encoder does arithmetic encoding in n contexts.  Methods:
    // Encoder(f, n) creates encoder for compression to archive f, which
    //     must be open past any header for writing in binary mode.
    // code(cxt, y) compresses bit y (0 or 1) to file f
    //   modeled in context cxt (0..n-1)
    // flush() should be called exactly once after compression is done and
    //     before closing f.

    class Encoder
    {
    private:
      const int N;    // number of contexts
      MyIOBase *archive;  // Compressed data file
      U32 x1, x2;     // Range, initially [0, 1), scaled by 2^32
      StateMap sm;    // cxt -> p
    public:
      Encoder(MyIOBase *a, int n=2048*258):N(n),archive(a),
       x1(0), x2(0xffffffff), sm(n){};
      void code(int cxt, int y)  // compress bit y
      {
        assert(y==0 || y==1);
        assert(cxt>=0 && cxt<N);
        int p=sm.p(cxt);
        assert(p>=0 && p<4096);
        U32 xmid=x1 + (x2-x1>>12)*p;
        assert(xmid>=x1 && xmid<x2);
        y ? (x2=xmid) : (x1=xmid+1);
        sm.update(cxt, y);
        while (((x1^x2)&0xff000000)==0) {  // pass equal leading bytes of range
            archive->putbyte(x2>>24);
            x1<<=8;
            x2=(x2<<8)+255;
        }
      }
      void flush()
      { // call this when compression is finished
        archive->putbyte(x1>>24);  // Flush first unequal byte of range
      }
      // Encode a byte c in context cxt to encoder e
      void encode(int cxt, int c)
      {
        // code high 4 bits in contexts cxt+1..15
        int b=(c>>4)+16;
        code(cxt+1     , b>>3&1);
        code(cxt+(b>>3), b>>2&1);
        code(cxt+(b>>2), b>>1&1);
        code(cxt+(b>>1), b   &1);

        // code low 4 bits in one of 16 blocks of 15 cxts (to reduce cache misses)
        cxt+=15*(b-15);
        b=c&15|16;
        code(cxt+1     , b>>3&1);
        code(cxt+(b>>3), b>>2&1);
        code(cxt+(b>>2), b>>1&1);
        code(cxt+(b>>1), b   &1);
      }
    };


    // ----------------------------------------------------------------------------
    // A Decoder does arithmetic decoding in n contexts.  Methods:
    // Decoder(f, n) creates decoder for decompression from archive f,
    //     which must be open past any header for reading in binary mode.
    // code(cxt) returns the next decompressed bit from file f
    //   with context cxt in 0..n-1.

    class Decoder {
    private:
      const int N;     // number of contexts
      MyIOBase* archive;   // Compressed data file
      U32 x1, x2;      // Range, initially [0, 1), scaled by 2^32
      U32 x;           // Decompress mode: last 4 input bytes of archive
      StateMap sm;     // cxt -> p
    public:
      Decoder(MyIOBase* f, int n=(1024+1024)*258):N(n), archive(f), x1(0), x2(0xffffffff), x(0), sm(n){}
      void deinit()
      {
        for (int i=0; i<4; ++i)
          x=(x<<8)+(archive->getbyte()&255);
      }
      // Return decompressed bit (0..1) in context cxt (0..n-1)
      inline int code(int cxt)
      {
        assert(cxt>=0 && cxt<N);
        int p=sm.p(cxt);
        assert(p>=0 && p<4096);
        U32 xmid=x1 + (x2-x1>>12)*p;
        assert(xmid>=x1 && xmid<x2);
        int y=x<=xmid;
        y ? (x2=xmid) : (x1=xmid+1);
        sm.update(cxt, y);
        while (((x1^x2)&0xff000000)==0) {  // pass equal leading bytes of range
          x1<<=8;
          x2=(x2<<8)+255;
          x=(x<<8)+(archive->getbyte()&255);  // EOF is OK
        }
        return y;
      }

      // Decode one byte
      int decode(int cxt)
      {
        int hi=1, lo=1;  // high and low nibbles
        hi+=hi+code(cxt+hi);
        hi+=hi+code(cxt+hi);
        hi+=hi+code(cxt+hi);
        hi+=hi+code(cxt+hi);
        cxt+=15*(hi-15);
        lo+=lo+code(cxt+lo);
        lo+=lo+code(cxt+lo);
        lo+=lo+code(cxt+lo);
        lo+=lo+code(cxt+lo);
        return hi-16<<4|(lo-16);
      }
    };

    // ----------------------------------------------------------------------------
    // Decompress from in to out.  in should be positioned past the header.
}//namespace

class SR3ACoder : public CoderBase //Encode class
{
private:

    inline void Compress(MyIOBase *Source)
    {
        SR3A::Encoder e(Archive);
        const int cshft = 20; // Bit shift for context
        const int hmask =0xffffff;  // Hash mask
        const int hmult = 5 << 5;       // Multiplier for hashes
        const int maxc = 1;       // Number of channels
        int channel = 0;                          // Channel for WAV files
        U32 *t4;  // context -> last 3 bytes in bits 0..23, count in 24..29
        int c1=0; // previous byte
        U32 hc[16];                     // Hash of last 4 bytes by channel
        int cxt=0;  // context
        U32 r=0;

        t4=new U32[0x1000000];
        memset(t4, 0, sizeof(U32)*0x1000000);
        memset(hc, 0, sizeof(hc));

        for (unsigned long i = 0; i < Head.usize; i++)
        {
          const U32 h = hc[channel];
          r=t4[h];  // last byte count, last 3 bytes in this context

          if (r>=0x4000000) cxt=1024+(r>>cshft);
            else cxt=c1|r>>16&0x3f00;
          cxt*=258;

          int c=Source->getbyte();
          int comp3=c*0x10101^r;  // bytes 0, 1, 2 are 0 if equal
          if (!(comp3&0xff))
          {  // match first?
            e.code(cxt, 0);
            if (r<0x3f000000) t4[h]+=0x1000000;  // increment count
          }
          else if (!(comp3&0xff00)) {  // match second?
            e.code(cxt, 1);
            e.code(cxt+1, 1);
            e.code(cxt+2, 0);
            t4[h]=r&0xff0000|r<<8&0xff00|c|0x1000000;
          }
          else if (!(comp3&0xff0000)) {  // match third?
            e.code(cxt, 1);
            e.code(cxt+1, 1);
            e.code(cxt+2, 1);
            t4[h]=r<<8&0xffff00|c|0x1000000;
          }
          else {  // literal?
            e.code(cxt, 1);
            e.code(cxt+1, 0);
            e.encode(cxt+2, c);
            t4[h]=r<<8&0xffff00|c;
          }
          c1=c;
          hc[channel]=(h*hmult+c+1)&hmask;
          if (++channel>=maxc) channel=0;

        }
        e.code(cxt, 1);
        e.code(cxt+1, 0);
        e.encode(cxt+2, r&0xff);
        e.flush();
        delete []t4;
    }
    inline void UnCompress(MyIOBase *Source)
    {
        SR3A::Decoder e(Archive);
        const int cshft = 20; // Bit shift for context
        const int hmask =0xffffff;  // Hash mask
        const int hmult = 5<<5;       // Multiplier for hashes
        const int maxc = 1;       // Number of channels
        U32 *t4;  // context -> last 3 bytes in bits 0..23, count in 24..29
        int channel = 0;                          // Channel for WAV files
        int c1=0; // previous byte
        U32 hc[16];                     // Hash of last 4 bytes by channel
        int cxt;

        t4=new U32[0x1000000];
        memset(hc, 0, sizeof(hc));
        memset(t4, 0, sizeof(U32)*0x1000000);
        e.deinit();
        for (unsigned long i = 0; i < Head.usize; i++)
        {
            const U32 h = hc[channel];
            const U32 r=t4[h];
            if (r>=0x4000000) cxt=1024+(r>>cshft);
                else cxt=c1|r>>16&0x3f00;
            cxt*=258;

            // Decompress: 0=p[1], 110=p[2], 111=p[3], 10xxxxxxxx=literal.
            // EOF is marked by p[1] coded as a literal.
            if (e.code(cxt))
            {
                if (e.code(cxt+1))
                {
                    if (e.code(cxt+2))
                    {  // match third?
                        c1=r>>16&0xff;
                        t4[h]=r<<8&0xffff00|c1|0x1000000;
                    }
                    else
                    {  // match second?
                        c1=r>>8&0xff;
                        t4[h]=r&0xff0000|r<<8&0xff00|c1|0x1000000;
                    }
                }
                else
                {  // literal?
                    c1=e.decode(cxt+2);
                    if (c1==int(r&0xff)) break;  // EOF?
                    t4[h]=r<<8&0xffff00|c1;
                }
            }
            else
            {  // match first?
                c1=r&0xff;
                if (r<0x3f000000) t4[h]+=0x1000000;  // increment count
            }
            Source->putbyte(c1);
            hc[channel]=(h*hmult+c1+1)&hmask;
            if (++channel >= maxc) channel=0;
        }
    }
public:
    SR3ACoder(bool m,MyIOBase *p=NULL):CoderBase(p){MathodN="SR3A";Mode=m;}
    //Mode: true for encode,false foe decode
};
 #endif
