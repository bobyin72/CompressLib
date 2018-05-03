#ifndef FPAQCompress_H
#define FPAQCompress_H
//This is fast compress ratio class
//based on fpaq0f2 - Adaptive order 0 file compressor.
//    (C) 2008, Matt Mahoney, mmahoney@cs.fit.edu
//resutruct by  bobyin 2008

namespace FPAQ
{
    /* fpaq0f2 - 
    (C) 2008, Matt Mahoney under GPL, http://www.gnu.org/licenses/gpl.txt
    
    fpaq0f2 is an order-0 file compressor and arithmetic coder.  Each bit is
    modeled in the context of the previous bits in the current byte, plus
    the bit history (last 8 bits) observed in this context.
    */
    
    #include "CompComm.hpp"
    using namespace std;
    
    //////////////////////////// StateMap //////////////////////////
    
    // A StateMap maps a context to a probability.  After a bit prediction, the
    // map is updated in the direction of the actual value to improve future
    // predictions in the same context.
    
    class StateMap {
    protected:
      const int N;  // Number of contexts
      int cxt;      // Context of last prediction
      U32 *t;       // cxt -> prediction in high 24 bits, count in low 8 bits
      int dt[256];  // reciprocal table: i -> 16K/(i+1.5)
    public:
      StateMap(int n=256)  // create allowing n contexts
      : N(n), cxt(0) 
      {
        t=new U32[N];
        for (int i=0; i<N; ++i) {
          // Count 1 bits to determine initial probability.
          U32 n=(i&1)*2+(i&2)+(i>>2&1)+(i>>3&1)+(i>>4&1)+(i>>5&1)+(i>>6&1)+(i>>7&1)+3;
          t[i]=n<<28|6;
        }
        for (int i=0; i<256; ++i)
          dt[i]=32768/(i+i+3);
      }
      ~StateMap(){delete []t;}
    
      // Predict next bit to be updated in context cx (0..n-1).
      // Return prediction as a 16 bit number (0..65535) that next bit is 1.
      int p(int cx) {
        assert(cx>=0 && cx<N);
        return t[cxt=cx]>>16;
      }
    
      // Update the model with bit y (0..1).
      // limit (1..255) controls the rate of adaptation (higher = slower)
      void update(int y, int limit=255) {
        assert(cxt>=0 && cxt<N);
        assert(y==0 || y==1);
        assert(limit>=0 && limit<255);
        int n=t[cxt]&255, p=t[cxt]>>14;  // count, prediction
        if (n<limit) ++t[cxt];
        t[cxt]+=((y<<18)-p)*dt[n]&0xffffff00;
      }
    };
    
    //////////////////////////// Predictor /////////////////////////
    
    /* A Predictor estimates the probability that the next bit of
       uncompressed data is 1.  Methods:
       p() returns P(1) as a 16 bit number (0..65535).
       update(y) trains the predictor with the actual bit (0 or 1).
    */
    
    class Predictor {
      int cxt;  // Context: 0=not EOF, 1..255=last 0-7 bits with a leading 1
      StateMap sm;
      int state[256];
    public:
      Predictor(): cxt(0), sm(0x10000) {
      for (int i=0; i<0x100; ++i)
        state[i]=0x66;
      }
    
      // Assume order 0 stream of 9-bit symbols
      int p() {
        return sm.p(cxt<<8|state[cxt]);
      }
    
      void update(int y) {
        sm.update(y, 90);
        int& st=state[cxt];
        (st+=st+y)&=255;
        if ((cxt+=cxt+y) >= 256)
          cxt=0;
      }
    };
   
    //////////////////////////// Encoder ////////////////////////////
    
    /* An Encoder does arithmetic encoding.  Methods:
       Encoder(COMPRESS, f) creates encoder for compression to archive f, which
         must be open past any header for writing in binary mode
       Encoder(DECOMPRESS, f) creates encoder for decompression from archive f,
         which must be open past any header for reading in binary mode
       encode(bit) in COMPRESS mode compresses bit to file f.
       decode() in DECOMPRESS mode returns the next decompressed bit from file f.
       flush() should be called when there is no more to compress.
    */
    
    typedef enum {COMPRESS, DECOMPRESS} Mode;
    class Encoder {
    private:
      Predictor predictor;   // Computes next bit probability (0..65535)
      const Mode mode;       // Compress or decompress?
      MyIOBase *Arc;
      U32 x1, x2;            // Range, initially [0, 1), scaled by 2^32
      U32 x;                 // Last 4 input bytes of archive.
    public:
      Encoder(Mode m,MyIOBase *A): predictor(), mode(m), Arc(A), x1(0),
                                       x2(0xffffffff), x(0) {}
      void encode(int y)
      {
      
        // Update the range
        const U32 p=predictor.p();
        assert(p<=0xffff);
        assert(y==0 || y==1);
        const U32 xmid = x1 + (x2-x1>>16)*p + ((x2-x1&0xffff)*p>>16);
        assert(xmid >= x1 && xmid < x2);
        if (y)
          x2=xmid;
        else
          x1=xmid+1;
        predictor.update(y);
      
        // Shift equal MSB's out
        while (((x1^x2)&0xff000000)==0) {
          Arc->putbyte(x2>>24);
          x1<<=8;
          x2=(x2<<8)+255;
        }
      }
      int decode()
      {
        // Update the range
        const U32 p=predictor.p();
        assert(p<=0xffff);
        const U32 xmid = x1 + (x2-x1>>16)*p + ((x2-x1&0xffff)*p>>16);
        assert(xmid >= x1 && xmid < x2);
        int y=0;
        if (x<=xmid) {
          y=1;
          x2=xmid;
        }
        else
          x1=xmid+1;
        predictor.update(y);
      
        // Shift equal MSB's out
        while (((x1^x2)&0xff000000)==0) {
          x1<<=8;
          x2=(x2<<8)+255;
          int c=Arc->getbyte();
          x=(x<<8)+c;
        }
        return y;
      }
      void flush()
      {
        // In COMPRESS mode, write out the remaining bytes of x, x1 < x < x2
        if (mode==COMPRESS) {
          while (((x1^x2)&0xff000000)==0) {
            Arc->putbyte(x2>>24);
            x1<<=8;
            x2=(x2<<8)+255;
          }
          Arc->putbyte(x2>>24);  // First unequal byte
        }
      }
      void deinit()
      {
        // In DECOMPRESS mode, initialize x to the first 4 bytes of the archive
        if (mode==DECOMPRESS) {
          for (int i=0; i<4; ++i) {
            int c=Arc->getbyte();
            x=(x<<8)+(c&0xff);
          }
        }
      }
    };
}//namespace
class FPAQCoder : public CoderBase //Encode class
{
private:
    inline void Compress(MyIOBase *Source)
    {
        FPAQ::Encoder En(FPAQ::COMPRESS,Archive);
        int c;
        for (unsigned long j = 0; j < Head.usize; j++)
        {
            c=Source->getbyte();
            En.encode(1);
            for (int i=7; i>=0; --i)
                En.encode((c>>i)&1);
        }
        En.encode(0);
        En.flush();
    }
    inline void UnCompress(MyIOBase *Source)
    {
        FPAQ::Encoder De(FPAQ::DECOMPRESS,Archive);
        De.deinit();
        for (unsigned long i = 0; i < Head.usize; i++)
        {
            int c=1;
            De.decode();
            while (c<256) c+=c+De.decode();
            Source->putbyte(c-256);
        }
    }
public:
    FPAQCoder(bool m,MyIOBase *p=NULL):CoderBase(p){MathodN="FPAQ";Mode=m;}
    //Mode: true for encode,false foe decode
};
#endif
