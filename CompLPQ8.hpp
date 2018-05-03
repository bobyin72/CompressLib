#ifndef LPAQ8Commpress_H
#define LPAQ8Commpress_H
#include "CompComm.hpp"

namespace LPQ8
{

#define LPQ8MEM (1<<20) // Global memory usage = 3*MEM bytes (1<<20 .. 1<<29)
#define LPQ8MEMBUF (10*1024*1024)
#define LPQ8MI 8
enum {MAXLEN=62};   // maximum match length, at most 62
class GlobalVer
{
private:
  U32 memused;
  U8 *MemBuf;
public:
  //int *stretch_t;	//initialized when Encoder is created
  int *myst;
  int *st_t,*st_t2,*sq_t;
  int *dt;	// i -> 16K/(i+3)
  int *dta; // i ->  8K/(i+3)
  int mxr_tx[LPQ8MI];	// MI inputs
  int y20;   // y<<20
  int* mxr_cxt;		// context


  GlobalVer();
  inline ~GlobalVer(){delete []MemBuf;}

  void init();
  inline void quit(const char* message)
  {
    if (message) printf("%s\n", message);
    exit(1);
  }

  template <class T> void allo(T*&p, int n)
  {
    if (n<=1) quit("allo parameter error");
    if ((memused+n*sizeof(T))>LPQ8MEMBUF) quit("out of memory");
    p=(T *)&MemBuf[memused];
    memused+=n*sizeof(T);
  }
  inline U32 GetMemUsed(){return memused;}
  inline void m_add(int a,int b){mxr_tx[a]=st_t[b]; }
  inline void train(int err);
  inline int dot_product();
};

//////////////////////////// StateMap, APM //////////////////////////
// A StateMap maps a context to a probability.  Methods:
//
// Statemap sm(n) creates a StateMap with n contexts using 4*n bytes memory.
// sm.p(y, cx, limit) converts state cx (0..n-1) to a probability (0..4095).
//	that the next y=1, updating the previous prediction with y (0..1).
//	limit (1..1023, default 1023) is the maximum count for computing a
//	prediction.  Larger values are better for stationary sources.

class StateMap {
private:
  GlobalVer *GV;
  int mn;
  void init();
public:
  U32 *t_cxt;	// Context of last prediction
  U32 *t;	// cxt -> prediction in high 22 bits, count in low 10 bits

  StateMap(GlobalVer *G=NULL,int n=768);

  inline void SetGV(GlobalVer *G){GV=G;init();}
  inline int p0(int cx);
  inline int p1(int cx);
  inline int p2(int cx);
  inline int p3(int cx);
  inline int p4(int cx);
  inline int p5(int cx);
  inline int p6(int cx);

};

// An APM maps a probability and a context to a new probability.  Methods:
//
// APM a(n) creates with n contexts using 96*n bytes memory.
// a.pp(y, pr, cx, limit) updates and returns a new probability (0..4095)
//     like with StateMap.  pr (0..4095) is considered part of the context.
//     The output is computed by interpolating pr into 24 ranges nonlinearly
//     with smaller ranges near the ends.  The initial output is pr.
//     y=(0..1) is the last bit.  cx=(0..n-1) is the other context.
//     limit=(0..1023) defaults to 255.

class APM {
protected:
  int cxt;	// Context of last prediction
  U32 *t;	// cxt -> prediction in high 22 bits, count in low 10 bits
  GlobalVer *GV;
public:
  APM(GlobalVer *G,int n);

  inline int p1(int pr, int cx) ;
  inline int p2(int pr, int cx) ;
};


//////////////////////////// Mixer /////////////////////////////

// Mixer m(MI, MC) combines models using MC neural networks with
//	MI inputs each using 4*MC*MI bytes of memory.  It is used as follows:
// m.update(y) trains the network where the expected output is the
//	last bit, y.
// m.add(stretch(p)) inputs prediction from one of MI models.  The
//	prediction should be positive to predict a 1 bit, negative for 0,
//	nominally -2K to 2K.
// m.set(cxt) selects cxt (0..MC-1) as one of MC neural networks to use.
// m.p() returns the output prediction that the next bit is 1 as a
//	12 bit number (0 to 4095).  The normal sequence per prediction is:
//
// - m.add(x) called MI times with input x=(-2047..2047)
// - m.set(cxt) called once with cxt=(0..MC-1)
// - m.p() called once to predict the next bit, returns 0..4095
// - m.update(y) called once for actual bit y=(0..1).

//////////////////////////// HashTable /////////////////////////

// A HashTable maps a 32-bit index to an array of B bytes.
// The first byte is a checksum using the upper 8 bits of the
// index.  The second byte is a priority (0 = empty) for hash
// replacement.  The index need not be a hash.

// HashTable<B> h(n) - create using n bytes  n and B must be
//     powers of 2 with n >= B*4, and B >= 2.
// h[i] returns array [1..B-1] of bytes indexed by i, creating and
//     replacing another element if needed.  Element 0 is the
//     checksum and should not be modified.

template <int B>
class HashTable {
  U8* t;	// table: 1 element = B bytes: checksum,priority,data,data,...
  const int NB;	// size in bytes
  GlobalVer *GV;
public:
  HashTable(GlobalVer *G,int n);
  U8* get(U32 i);
};


//////////////////////////// MatchModel ////////////////////////

// MatchModel(n) predicts next bit using most recent context match.
//     using n bytes of memory.  n must be a power of 2 at least 8.
// MatchModel::p(y, m) updates the model with bit y (0..1) and writes
//     a prediction of the next bit to Mixer m.  It returns the length of
//     context matched (0..62).

class MatchModel {
  int* ht;    // context hash -> next byte in buf
  int match;  // pointer to current byte in matched context in buf
  int buf_match;  // buf[match]+256
  int len;    // length of match
  StateMap sm;  // len, bit, last byte -> prediction
  GlobalVer *GV;
public:
  U8* buf;	// input buffer
  int pos;	// number of bytes in buf
  U32 h1, h2, h3; // context hashes
  int N, HN;

  MatchModel(GlobalVer *G,int n=LPQ8MEM);  // n must be a power of 2 at least 8.
  inline int p(int b,int c1,U32 *l,int c0);	// predict next bit to m
  inline void upd();	// update bit y (0..1)
};

//////////////////////////// Predictor /////////////////////////

// A Predictor estimates the probability that the next bit of
// uncompressed data is 1.  Methods:
// Predictor(n) creates with 3*n bytes of memory.
// p() returns P(1) as a 12 bit number (0-4095).
// update(y) trains the predictor with the actual bit (0 or 1).

//////////////////////////// Encoder ////////////////////////////

// An Encoder does arithmetic encoding.  Methods:
// Encoder(COMPRESS, f) creates encoder for compression to archive f, which
//     must be open past any header for writing in binary mode.
// Encoder(DECOMPRESS, f) creates encoder for decompression from archive f,
//     which must be open past any header for reading in binary mode.
// code(i) in COMPRESS mode compresses bit i (0 or 1) to file f.
// code() in DECOMPRESS mode returns the next decompressed bit from file f.
// compress(c) in COMPRESS mode compresses one byte.
// decompress() in DECOMPRESS mode decompresses and returns one byte.
// flush() should be called exactly once after compression is done and
//     before closing f.  It does nothing in DECOMPRESS mode.

typedef enum {COMPRESS, DECOMPRESS} Mode;
class Encoder {
private:
  GlobalVer *GV;
  int *calcprevfail;
  int *stretch_t;
  int *squash_t;	//initialized when Encoder is created
  int *stretch_t2;
  int *calcfails;
  int bcount;
  int c4;  // last 4 bytes
  int c1;
  int mxr_pr;
  U8 fails;
  U32 h[6];
  U32 pw, c8, cc, prevfail;
  U32 *len2cxt,*len2order;
  U8 *t0;  // order 1 cxt -> state
  U8 *t0c1, *cp[6]; // pointer to bit history
  int* mxr_wx;		// MI*MC weights
  int* mxr_wx4;		// mxr_wx + MI*40
  int c0;  // last 0-7 bits with leading 1
public:
  HashTable<16> t1;  // cxt -> state
  HashTable<16> t2;  // cxt -> state
  HashTable<16> t3;  // cxt -> state
#define t1a t1
#define t2a t2
#define t3a t3
#define t1b t1
#define t2b t2
#define t3b t3
  StateMap sm[6];
  APM a1, a2;
  MatchModel mm;	// predicts next bit by matching context
//  FILE *archive;	// Compressed data file
  MyIOBase *Arc;
  U32 x1, x2;		// Range, initially [0, 1), scaled by 2^32
  U32 x;		// Decompress mode: last 4 input bytes of archive
  U32 p;
  int *add2order;

  U32 Predictor_upd(int y);
  Encoder(GlobalVer *G,Mode m, MyIOBase *A);

  inline void flush(){Arc->putbyte(x1>>24);}

  // Compress one byte
  void compress(int c);
  // Decompress and return one byte
  int decompress() ;
  inline void deinit() {  // x = first 4 bytes of archive
    for (int i=0; i<4; ++i)
      x=(x<<8)+(Arc->getbyte()&255);
  }
};
}
class LPQ8Coder : public CoderBase //Encode class
{
private:
    LPQ8::GlobalVer G;
    inline void Compress(MyIOBase *Source)
    {
        G.init();
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
    LPQ8Coder(bool m,MyIOBase *p=NULL):CoderBase(p){MathodN="LPQ8";Mode=m;}
    //Mode: true for encode,false for decode
};
#endif
