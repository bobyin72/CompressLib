#include "CompLPQ8.hpp"
using namespace LPQ8;

#define MI 8
#define MC 1280
#define DP_SHIFT 14
#define TOLIMIT_10 64
#define TOLIMIT_11 320
#define TOLIMIT_12 240
#define TOLIMIT_13 140
#define TOLIMIT_14 124
#define TOLIMIT_15 124
#define TOLIMIT_16 112
#define TOLIMIT_2a 384
#define TOLIMIT_2b 680
#define SQUARD 2
#define MIN_LEN 2
//#define MXR_MUL 341/1024
#define MXR_UPD 8
#define MXR_INIT 0x0c00
#define PW_MUL 509
#define P5_MUL 37
#define add2order0 ( (c>>6) + ((c4>>4)&12) )
#define add2order4 mxr_wx4 + 640*(((c4>>6)&3)+(c0&12))
#define get0 get
#define get1 get
#define upd3_cp\
	cp[2]=t1a.get(hash7(c0*23-(c4&0xffffff)*251));\
	cp[3]=t2a.get(hash8(c0   -c4*59));\
	cp[4]=t3b.get(hash9((c0<<5)-c0-c4*197+(c8& 0xffff)*63331 ));
#define upd7_cp\
	cp[2]=t1b.get(hash2(c4&0xffffff));\
	cp[3]=t2b.get(hash3((c4<<7)-c4));\
	cp[4]=t3a.get(hash4(c4*197-(c8&0xffff)*59999));
#define upd7_h123\
    h[0]=c<<8;		/* order 1 */\
    t0c1=t0+( (c4&240) + ((c4>>12)&12) + ((c4>>22)&3)  )*256;\
    prevfail=calcprevfail[fails];\
    fails=1;\
    mm.h1=mm.h1*(7<<1)-c-1&mm.HN;\
    mm.h2=(c4*1021-c8*2039+cc*421)&mm.HN;\
    mm.h3=hash3a(c4*59+c8*383)&mm.HN;

static const U8 len2[63]={ 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,16,16,16,
	17,17,17,17,18,18,18,18,19,19,19,19,20,20,20,20,21,21,21,21,
	22,22,22,22,23,23,23,23,24,24,24,24,25,25,25,25,25,26,26,26,26,26,27 };

LPQ8::GlobalVer::GlobalVer()
{
    MemBuf=new U8[LPQ8MEMBUF];
    init();
}
void LPQ8::GlobalVer::init()
{
    memset(MemBuf,0,LPQ8MEMBUF);

    memused=y20=0;
    mxr_cxt=NULL;
    memset(mxr_tx,0,MI*sizeof(int));

    //allo(stretch_t,4096);
    allo(st_t,4096);
    allo(st_t2,4096);
    allo(sq_t,4096);
    allo(myst,4096);
    allo(dt,1024);
    allo(dta,1024);

    
    int pi=0;
    for (int x=-2047; x<=2047; ++x) 
    {  // invert squash()
        int i;
        if (x==2047) i=4095; else
        {
            if (x==-2047) i=0; else
            {
                double k=4096/(double(1+exp(-(double(x)/256))));
                i=(int)k;
            }
        }
        myst[x+2047]=i;
        sq_t[x+2047]=i+SQUARD;	//rounding,  needed at the end of Predictor::update()
        for (int j=pi; j<=i; ++j)
            st_t[j]=x, st_t2[j]=(x+2047)*23;
        pi=i+1;
  }
  st_t[4095]=2047;
  st_t2[4095]=4094*23;

  for (int i=0; i<1024; ++i)
    dt[i]=512*17/(i+i+3),dta[i]=512*34/(i+i+18);
}

///////////////////////////// Squash //////////////////////////////

// return p = 1/(1 + exp(-d)), d scaled by 8 bits, p scaled by 12 bits
/*int LPQ8::GlobalVer::squash_init(int d) {
  if (d==2047) return 4095;
  if (d==-2047) return 0;
  double k=4096/(double(1+exp(-(double(d)/256))));
  return int(k);
}
*/
///////////////////////// state table ////////////////////////

// State table:
//   nex(state, 0) = next state if bit y is 0, 0 <= state < 256
//   nex(state, 1) = next state if bit y is 1
//
// States represent a bit history within some context.
// State 0 is the starting state (no bits seen).
// States 1-30 represent all possible sequences of 1-4 bits.
// States 31-252 represent a pair of counts, (n0,n1), the number
//   of 0 and 1 bits respectively.  If n0+n1 < 16 then there are
//   two states for each pair, depending on if a 0 or 1 was the last
//   bit seen.
// If n0 and n1 are too large, then there is no state to represent this
// pair, so another state with about the same ratio of n0/n1 is substituted.
// Also, when a bit is observed and the count of the opposite bit is large,
// then part of this count is discarded to favor newer data over old.

static const U8 State_table[2][256*5]={{
  1,  3,  4,  7,  8,  9, 11, 15, 16, 17, 18, 20, 21, 22, 26, 31, 32, 32, 32, 32,
 34, 34, 34, 34, 34, 34, 36, 36, 36, 36, 38, 41, 42, 42, 44, 44, 46, 46, 48, 48,
 50, 53, 54, 54, 56, 56, 58, 58, 60, 60, 62, 62, 50, 67, 68, 68, 70, 70, 72, 72,
 74, 74, 76, 76, 62, 62, 64, 83, 84, 84, 86, 86, 44, 44, 58, 58, 60, 60, 76, 76,
 78, 78, 80, 93, 94, 94, 96, 96, 48, 48, 88, 88, 80,103,104,104,106,106, 62, 62,
 88, 88, 80,113,114,114,116,116, 62, 62, 88, 88, 90,123,124,124,126,126, 62, 62,
 98, 98, 90,133,134,134,136,136, 62, 62, 98, 98, 90,143,144,144, 68, 68, 62, 62,
 98, 98,100,149,150,150,108,108,100,153,154,108,100,157,158,108,100,161,162,108,
110,165,166,118,110,169,170,118,110,173,174,118,110,177,178,118,110,181,182,118,
120,185,186,128,120,189,190,128,120,193,194,128,120,197,198,128,120,201,202,128,
120,205,206,128,120,209,210,128,130,213,214,138,130,217,218,138,130,221,222,138,
130,225,226,138,130,229,230,138,130,233,234,138,130,237,238,138,130,241,242,138,
130,245,246,138,140,249,250, 80,140,253,254, 80,140,253,254, 80,

  2,  2,  4,  6,  8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30, 32, 34, 36, 38,
 40, 42, 44, 46, 48, 50, 52, 54, 56, 58, 60, 62, 64, 66, 68, 70, 72, 74, 76, 78,
 80, 82, 84, 86, 88, 90, 92, 94, 96, 98,100,102,104,106,108,110,112,114,116,118,
120,122,124,126,128, 66, 68, 70, 72, 74, 76, 78, 80, 82, 84, 86, 88, 90, 92, 94,
 96, 98,100,102,104,106,108,110,112,114,116,118,120,122,124, 94,130, 66, 68, 70,
 72, 74, 76, 78, 80, 82, 84, 86, 88, 90, 92, 94, 96, 98,100,102,104,106,108,110,
112,114,116,118,120,122,124,126,132,124,134, 92,136,124,138, 92,140,124,142, 92,
144,124,146, 92,148,124,150, 92,152,124,154, 92,156,124,158, 92,160,124,162, 92,
164,124,166, 92,168,120,170, 92,172,120,174, 92,176,120,178, 92,180,120,182, 92,
184,120,186, 92,188,120,190, 92,192,120,194, 88,196,120,198, 88,200,120,202, 88,
204,120,206, 88,208,120,210, 88,212,120,214, 88,216,120,218, 88,220,120,222, 88,
224,120,226, 88,228,120,230, 88,232,120,234, 88,236,120,238, 88,240,120,242, 88,
244,120,246, 88,248,120,250, 88,252,120,254, 88,252,120,254, 88,

  2,  2,  4,  6,  8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30, 32, 34, 36, 38,
 40, 42, 44, 46, 48, 50, 52, 54, 56, 58, 60, 62, 64, 66, 68, 70, 72, 74, 76, 78,
 80, 82, 84, 86, 88, 90, 92, 94, 96, 98,100,102,104,106,108,110,112,114,116,118,
120,122,124,126,128,130,132,134,136,138,140,142,144,146,148,150,152,154,156,158,
160,162,164,166,168,170,172,174,176,178,180,182,184,186,188,190,192,194,196,198,
200,202,204,206,208,210,212,214,216,218,220,222,224,226,228,230,232,234,236,238,
240,242,244,246,248,250,252,254,128,130,132,134,136,138,140,142,144,146,148,150,
152,154,156,158,160,162,164,166,168,170,172,174,176,178,180,182,184,186,188,190,
192,194,196,198,200,202,204,206,208,210,212,214,216,218,220,222,224,226,228,230,
232,234,236,238,240,242,244,246,248,250,252,190,192,130,132,134,136,138,140,142,
144,146,148,150,152,154,156,158,160,162,164,166,168,170,172,174,176,178,180,182,
184,186,188,190,192,194,196,198,200,202,204,206,208,210,212,214,216,218,220,222,
224,226,228,230,232,234,236,238,240,242,244,246,248,250,252,254,

  1, 16, 17, 18, 38, 40, 42, 70, 46, 63, 95,195,159,191,223,240,  2, 32, 33, 34,
 39, 41, 43, 45, 47, 79,111,143,175,207,225,241,  3, 48, 49, 50, 36, 52, 53, 54,
 55, 53, 57, 58, 59, 60, 61, 62, 19, 35, 36, 66, 67, 64, 54, 70, 71, 72, 73, 74,
 75, 76, 77, 78, 68, 69, 37, 82, 83, 84, 80, 81, 87, 88, 89,  6, 91, 92, 93, 94,
 85, 86,  4, 98, 99,100,101, 96, 97,104,105,106,107,108,109,110,102,103, 20,114,
115,116,117,118,112,113,  6,122,123,124,125,126,119,120,  5,130,131, 70,133,134,
135,128, 97,138,139,140,141,157,136,152, 21,146,147,148,149,150,151,152, 97,145,
155,156,157,158,168,154,  6,162,163,164,165,166,167,168,149,160,161,172,173,174,
170,171, 22,178,179,180,181,182,183,199,185,186,176,177,189,190,187,188,  7,194,
195,196,197,198,199,200,201,202,195,192,226,206,204,220, 23,210,211,212,213,214,
149,216,217,233,219,220,208,209,221,222,  8,226,227,228,229,245,231,232,233,234,
235,236,226,224,238,239, 24,242,243,244,245,246,247,248,249,250,251, 14,253,254,
255,242,  9, 25, 10, 26, 11, 27, 12, 28, 13, 29, 14, 30, 15, 31,

  2, 16,  4, 18,  6, 20,  8, 22, 10, 24, 12, 26, 14, 28,128, 30, 18, 32, 20, 34,
 22, 36, 24, 38, 26, 40, 28, 42, 30, 44,130, 46, 34, 48, 36, 50, 38, 52, 40, 54,
 42, 56, 44, 58, 46, 60, 24, 62, 50, 64, 52, 66, 54, 68, 56, 70, 58, 72, 60, 74,
 62, 76, 40, 78, 66, 80, 68, 82, 70, 84, 72, 86, 74, 88, 76, 90, 78, 92, 40, 94,
 82, 96, 84, 98, 86,100, 88,102, 90,104, 92,106, 94,108, 56,110, 98,112,100,114,
102,116,104,118,106,120,108,122,110,124, 56,126,114,112,116,114,118, 66,120, 66,
122, 68,124, 68,126, 70, 72, 70,132,112,134,114,136,112,138,114,140,112,142,114,
144,112,146,114,148,112,150,114,152,112,154,114,156,112,158,114,160,112,162,114,
164,112,166,114,168,112,170,114,172,112,174,114,176,112,178,114,180,112,182,114,
184,112,186,114,188,112,190,114,192,112,194,114,196,112,198,114,200,112,202,114,
204,112,206,114,208,112,210,114,212,112,214,114,216,112,218,114,220,112,222,114,
224,112,226,114,228,112,230,114,232,112,234,114,236,112,238,114,240,112,242,114,
244,112,246,114,248,112,250,114,252,112,254,114,252,112,254,114},

 { 2,  5,  6, 10, 12, 13, 14, 19, 23, 24, 25, 27, 28, 29, 30, 33, 35, 35, 35, 35,
 37, 37, 37, 37, 37, 37, 39, 39, 39, 39, 40, 43, 45, 45, 47, 47, 49, 49, 51, 51,
 52, 43, 57, 57, 59, 59, 61, 61, 63, 63, 65, 65, 66, 55, 57, 57, 73, 73, 75, 75,
 77, 77, 79, 79, 81, 81, 82, 69, 71, 71, 73, 73, 59, 59, 61, 61, 49, 49, 89, 89,
 91, 91, 92, 69, 87, 87, 45, 45, 99, 99,101,101,102, 69, 87, 87, 57, 57,109,109,
111,111,112, 85, 87, 87, 57, 57,119,119,121,121,122, 85, 97, 97, 57, 57,129,129,
131,131,132, 85, 97, 97, 57, 57,139,139,141,141,142, 95, 97, 97, 57, 57, 81, 81,
147,147,148, 95,107,107,151,151,152, 95,107,155,156, 95,107,159,160,105,107,163,
164,105,117,167,168,105,117,171,172,105,117,175,176,105,117,179,180,115,117,183,
184,115,127,187,188,115,127,191,192,115,127,195,196,115,127,199,200,115,127,203,
204,115,127,207,208,125,127,211,212,125,137,215,216,125,137,219,220,125,137,223,
224,125,137,227,228,125,137,231,232,125,137,235,236,125,137,239,240,125,137,243,
244,135,137,247,248,135, 69,251,252,135, 69,255,252,135, 69,255,

  3,  3,  5,  7,  9, 11, 13, 15, 17, 19, 21, 23, 25, 27, 29, 31, 33, 35, 37, 39,
 41, 43, 45, 47, 49, 51, 53, 55, 57, 59, 61, 63, 65, 67, 69, 71, 73, 75, 77, 79,
 81, 83, 85, 87, 89, 91, 93, 95, 97, 99,101,103,105,107,109,111,113,115,117,119,
121,123,125,127, 65, 67, 69, 71, 73, 75, 77, 79, 81, 83, 85, 87, 89, 91, 93, 95,
 97, 99,101,103,105,107,109,111,113,115,117,119,121,123,125,131, 97, 67, 69, 71,
 73, 75, 77, 79, 81, 83, 85, 87, 89, 91, 93, 95, 97, 99,101,103,105,107,109,111,
113,115,117,119,121,123,125,129, 67,133, 99,135, 67,137, 99,139, 67,141, 99,143,
 67,145, 99,147, 67,149, 99,151, 67,153, 99,155, 67,157, 99,159, 67,161, 99,163,
 67,165, 99,167, 71,169, 99,171, 71,173, 99,175, 71,177, 99,179, 71,181, 99,183,
 71,185, 99,187, 71,189, 99,191, 71,193,103,195, 71,197,103,199, 71,201,103,203,
 71,205,103,207, 71,209,103,211, 71,213,103,215, 71,217,103,219, 71,221,103,223,
 71,225,103,227, 71,229,103,231, 71,233,103,235, 71,237,103,239, 71,241,103,243,
 71,245,103,247, 71,249,103,251, 71,253,103,255, 71,253,103,255,

  3,  3,  5,  7,  9, 11, 13, 15, 17, 19, 21, 23, 25, 27, 29, 31, 33, 35, 37, 39,
 41, 43, 45, 47, 49, 51, 53, 55, 57, 59, 61, 63, 65, 67, 69, 71, 73, 75, 77, 79,
 81, 83, 85, 87, 89, 91, 93, 95, 97, 99,101,103,105,107,109,111,113,115,117,119,
121,123,125,127,129,131,133,135,137,139,141,143,145,147,149,151,153,155,157,159,
161,163,165,167,169,171,173,175,177,179,181,183,185,187,189,191,193,195,197,199,
201,203,205,207,209,211,213,215,217,219,221,223,225,227,229,231,233,235,237,239,
241,243,245,247,249,251,253,255,129,131,133,135,137,139,141,143,145,147,149,151,
153,155,157,159,161,163,165,167,169,171,173,175,177,179,181,183,185,187,189,191,
193,195,197,199,201,203,205,207,209,211,213,215,217,219,221,223,225,227,229,231,
233,235,237,239,241,243,245,247,249,251,253,191,193,131,133,135,137,139,141,143,
145,147,149,151,153,155,157,159,161,163,165,167,169,171,173,175,177,179,181,183,
185,187,189,191,193,195,197,199,201,203,205,207,209,211,213,215,217,219,221,223,
225,227,229,231,233,235,237,239,241,243,245,247,249,251,253,255,

 51, 66, 52,  4, 40,116, 97, 75, 47, 79,141,188,250,238,137,212, 37, 67, 38, 85,
130,102, 89, 61,  9,110,172,219,207,240,182,227, 82, 70,100,130, 41, 84, 55, 21,
131,117,103,104, 90, 76, 62, 63, 80,115, 21, 65, 99, 70, 41,146,132,118,112,105,
 91, 77, 78, 94, 80, 56, 69,114, 85, 71,  6,147,133,119,113,106, 92, 93,109,125,
 42,162,  5,100, 81, 57, 22,148,134,120,121,107,108,124,140,156,163,149,115, 86,
 72, 43,178,164,135,128,122,123,139,155,161,176,150,136,101, 87, 58,  7,179,165,
151,129,138,145,160,171,187,203,137,144, 96, 73, 44,194,180,166,152,153,154,170,
186,202,218,234,169,185, 88, 59, 23,195,181,167,168,184,200,201,217,233,249, 13,
216,232, 74, 45,210,196,182,183,199,215,231,247,248, 28,191,209, 12,175, 60,  8,
211,197,198,214,230,246, 27,159,190,206,222,253,208,237, 46,226,212,213,229,245,
 11,143,174,193,221,252, 30,255,225, 31, 24,227,228,244, 26,127,158,189,205,236,
 14,239,241,152,152,182,242,243, 10,111,142,173,192,220,251,223,254,237,167,197,
212,242, 25, 95,126,157,177,204,235, 29,224, 15,252,167,197,227,

  1, 17,  3, 19,  5, 21,  7, 23,  9, 25, 11, 27, 13, 29, 15, 31, 19, 33, 21, 35,
 23, 37, 25, 39, 27, 41, 29, 43, 31, 45, 31, 47, 35, 49, 37, 51, 39, 53, 41, 55,
 43, 57, 45, 59, 47, 61, 25, 63, 51, 65, 53, 67, 55, 69, 57, 71, 59, 73, 61, 75,
 63, 77, 25, 79, 67, 81, 69, 83, 71, 85, 73, 87, 75, 89, 77, 91, 79, 93, 41, 95,
 83, 97, 85, 99, 87,101, 89,103, 91,105, 93,107, 95,109, 41,111, 99,113,101,115,
103,117,105,119,107,121,109,123,111,125, 57,127,115,129,117,131,119, 67,121, 69,
123, 69,125, 71,127, 71, 57, 73, 15,133, 31,135, 15,137, 31,139, 15,141, 31,143,
 15,145, 31,147, 15,149, 31,151, 15,153, 31,155, 15,157, 31,159, 15,161, 31,163,
 15,165, 31,167, 15,169, 31,171, 15,173, 31,175, 15,177, 31,179, 15,181, 31,183,
 15,185, 31,187, 15,189, 31,191, 15,193, 31,195, 15,197, 31,199, 15,201, 31,203,
 15,205, 31,207, 15,209, 31,211, 15,213, 31,215, 15,217, 31,219, 15,221, 31,223,
 15,225, 31,227, 15,229, 31,231, 15,233, 31,235, 15,237, 31,239, 15,241, 31,243,
 15,245, 31,247, 15,249, 31,251, 15,253, 31,255, 15,253, 31,255
}};
//#define nex(state,sel) *(p+state+(sel<<8))
//#define nex(state,sel) p[state+(sel<<8)]

LPQ8::StateMap::StateMap(GlobalVer *G,int n): GV(G) {
  mn=n;
  if (GV!=NULL) init();
}
void StateMap::init() {
  GV->allo(t, mn);
  t_cxt=t;
  for (int i=0; i<mn; ++i)
    t[i]=0x80000000;
}

// An APM maps a probability and a context to a new probability.  Methods:
//
// APM a(n) creates with n contexts using 96*n bytes memory.
// a.pp(y, pr, cx, limit) updates and returns a new probability (0..4095)
//     like with StateMap.  pr (0..4095) is considered part of the context.
//     The output is computed by interpolating pr into 24 ranges nonlinearly
//     with smaller ranges near the ends.  The initial output is pr.
//     y=(0..1) is the last bit.  cx=(0..n-1) is the other context.
//     limit=(0..1023) defaults to 255.

  inline int LPQ8::APM::p1(int pr, int cx) {	//, int limit=1023)
  {
    U32 *p=&t[cxt], p0=p[0];
    U32 i=p0&1023, pr=p0>>12; // count, prediction
    p0+=(i<TOLIMIT_2a);
    p0+=((GV->y20-(int)pr)*GV->dta[i]+0x200)&0xfffffc00;
    p[0]=p0;
  }
    int wt=pr&0xfff;  // interpolation weight of next element
    cx=cx*24+(pr>>12);
    cxt=cx+(wt>>11);
    pr=(t[cx]>>13)*(0x1000-wt)+(t[cx+1]>>13)*wt>>19;
    return pr;
  }

  inline int LPQ8::APM::p2(int pr, int cx) {	//, int limit=1023)
  {
    U32 *p=&t[cxt], p0=p[0];
    U32 i=p0&1023, pr=p0>>12; // count, prediction
    p0+=(i<TOLIMIT_2b);
    p0+=((GV->y20-(int)pr)*GV->dta[i]+0x200)&0xfffffc00;
    p[0]=p0;
  }
    int wt=pr&0xfff;  // interpolation weight of next element
    cx=cx*24+(pr>>12);
    cxt=cx+(wt>>11);
    pr=(t[cx]>>13)*(0x1000-wt)+(t[cx+1]>>13)*wt>>19;
    return pr;
  }

LPQ8::APM::APM(GlobalVer *G,int n): cxt(0),GV(G) {
  GV->allo(t, n);
  for (int i=0; i<n; ++i) {
    int p=((i%24*2+1)*4096)/48-2048;
    t[i]=(U32(GV->myst[p+2047])<<20)+12;
  }
}

  // update bit y (0..1), predict next bit in context cx
#define smp(x) {\
    U32 p0=*t_cxt;\
    U32 i=p0&1023, pr=p0>>12; /* count, prediction */ \
    p0+=(i<x);\
    p0+=((GV->y20-(int)pr)*GV->dt[i])&0xfffffc00;\
    *t_cxt=p0;\
    t_cxt=t+cx;\
    return (*t_cxt) >>20;\
  }

  inline int LPQ8::StateMap::p0(int cx) smp(TOLIMIT_10)
  inline int LPQ8::StateMap::p1(int cx) smp(TOLIMIT_11)
  inline int LPQ8::StateMap::p2(int cx) smp(TOLIMIT_12)
  inline int LPQ8::StateMap::p3(int cx) smp(TOLIMIT_13)
  inline int LPQ8::StateMap::p4(int cx) smp(TOLIMIT_14)
  inline int LPQ8::StateMap::p5(int cx) smp(TOLIMIT_15)
  inline int LPQ8::StateMap::p6(int cx) smp(TOLIMIT_16)


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

inline void LPQ8::GlobalVer::train(int err) {
    int *w=mxr_cxt;
    w[0]+=mxr_tx[0]*err+0x0800>>12;
    w[1]+=mxr_tx[1]*err+0x0800>>12;
    w[2]+=mxr_tx[2]*err+0x1000>>13;
    w[3]+=mxr_tx[3]*err+0x0800>>12;
    w[4]+=mxr_tx[4]*err+0x0800>>12;
    w[5]+=mxr_tx[5]*err+0x0800>>12;
    w[6]+=mxr_tx[6]*err+0x0800>>12;
    w[7]+=	    err+4 >>3;
}
inline int LPQ8::GlobalVer::dot_product() {
    int *w=mxr_cxt;
    int sum =mxr_tx[0]*w[0];
	sum+=mxr_tx[1]*w[1];
	sum+=mxr_tx[2]*w[2];
	sum+=mxr_tx[3]*w[3];
	sum+=mxr_tx[4]*w[4];
	sum+=mxr_tx[5]*w[5];
	sum+=mxr_tx[6]*w[6];
	sum+=		w[7]<<8;
  sum>>=DP_SHIFT;
  if (sum<-2047) sum=-2047;
  if (sum> 2047) sum= 2047;
  return sum;
}

  // Adjust weights to minimize coding cost of last prediction
  //int err=-mxr_pr;
  //GV->train(err*MXR_MUL);
#define m_update(y) {			\
    int err=(y<<12)-y-mxr_pr;		\
    fails<<=1;				\
    if (err<MXR_UPD && err>-MXR_UPD) {} else {	\
      fails|=calcfails[err+4096];	\
      GV->train(err*341/1024);  		\
    }\
  }

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
LPQ8::HashTable<B>::HashTable(GlobalVer *G,int n): NB(n-B),GV(G) {
  GV->allo(t, n+512+B*2);
  t=(U8*)(((long)t+511)&0xfffffe00)+1;	// align on cache line boundary
}

#define RORi(x,y) x<<(32-y)|x>>y

inline U32 hash1(U32 i) {  i*=0xfffefdfb; i=RORi(i,10); return (i*765432197); }
inline U32 hash2(U32 i) {  i*=0xfffefdfb; i=RORi(i,13); return (i*654321893); }
inline U32 hash3(U32 i) {  i*=0xfffefdfb; i=RORi(i,11); return (i*543210973); }
inline U32 hash4(U32 i) {  i*=0xfffefdfb; i=RORi(i,12); return (i*432109879); }
inline U32 hash5(U32 i) {  i*=987654323;  i=RORi(i,11); return (i*234567891); }
inline U32 hash6(U32 i) {  i*=0xfeff77ff; i=RORi(i,13); return (i*876543211); }
inline U32 hash7(U32 i) {  i*=0xfeff77ff; i=RORi(i,12); return (i	   ); }
inline U32 hash8(U32 i) {		  i=RORi(i,14); return (i*654321893); }
inline U32 hash9(U32 i) {  i*=0xfeff77ff; i=RORi(i,10); return (i*543210973); }
inline U32 hash0(U32 i) {		  i=RORi(i,14); return (i*432109879); }
inline U32 hash3a(U32 i){		  i=RORi(i,10); return (i*543210973); }

template <int B>
inline U8* LPQ8::HashTable<B>::get(U32 i) {
  U8 *p=t+(i*B&NB), *q, *r, f;
  i>>=24;
  f=*(p-1);
  if (f==U8(i)) return p;
  q=p+B;
  f=*(q-1);
  if (f==U8(i)) return q;
  r=p+B*2;
  f=*(r-1);
  if (f==U8(i)) return r;
  if (*p>*q) p=q;
  if (*p>*r) p=r;
		*(U32*)(p -1)=i;	// This is NOT big-endian-compatible
		*(U32*)(p+ 3)=0;
		*(U32*)(p+ 7)=0;
		*(U32*)(p+11)=0;
  return p;
}

//////////////////////////// MatchModel ////////////////////////

// MatchModel(n) predicts next bit using most recent context match.
//     using n bytes of memory.  n must be a power of 2 at least 8.
// MatchModel::p(y, m) updates the model with bit y (0..1) and writes
//     a prediction of the next bit to Mixer m.  It returns the length of
//     context matched (0..62).


LPQ8::MatchModel::MatchModel(GlobalVer *G,int n): sm(G,55<<8),GV(G) {
  h1=h2=h3=pos=0;
  N=n/2-1;
  HN=n/8-1;
  h1=h2=h3=match=len=0;
  GV->allo(buf, N+1);
  GV->allo(ht, HN+1);
}

#define SEARCH(hsh) {	\
	len=1;		\
	match=ht[hsh];	\
	if (match!=pos) {\
	  while (len<MAXLEN+1 && buf[match-len&N]==buf[pos-len&N])	++len; \
	}		\
}
#define SEARCH2(hsh) {	\
	len=1;		\
	match=ht[hsh];	\
	if (match!=pos) {\
	  p=p1;		\
	  while (len<MAXLEN+1 && buf[match-len&N]==*p)		--p, ++len; \
	}		\
}

inline void LPQ8::MatchModel::upd() {
    // find or extend match
    if (len>MIN_LEN) {
      ++match;
      match&=N;
      if (len<MAXLEN)	++len;
    }
    else {
	if (pos>=MAXLEN) {
		U8 *p1=buf+pos-1, *p;
			     SEARCH2(h1)
		if (len<4) { SEARCH2(h2)
		if (len<4)   SEARCH2(h3)
		}
	}
	else {
			     SEARCH(h1)
		if (len<4) { SEARCH(h2)
		if (len<4) SEARCH(h3)
		}
	}

	--len;
    }
    buf_match=buf[match]+256;

    // update index
    ht[h1]=pos;
    ht[h2]=pos;
    ht[h3]=pos;
}

inline int LPQ8::MatchModel::p(int bc,int c1,U32 *l,int c0) {

  int cxt=c0;
  if (len>0) {
    int b=buf_match>>bc;
    if ((b>>1)==cxt)
      b=b&1,	// next bit
      cxt=l[len*2-b] + c1;   //len2cxt
    else
	len=0;
  }

  GV->m_add(0, sm.p0(cxt));
  return len;
}
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

//#define nex(state,sel) p[state+(sel<<8)]
#define nex(state,sel) State_table[y][state+(sel<<8)]

#define cp_update(m0,m1,m2,m3,m4,m5)\
  GV->y20=y<<20;		\
  {			\
  *cp[0]=nex(*cp[0], m0);  \
  *cp[1]=nex(*cp[1], m1);  \
  *cp[2]=nex(*cp[2], m2);  \
  *cp[3]=nex(*cp[3], m3);  \
  *cp[4]=nex(*cp[4], m4);  \
  *cp[5]=nex(*cp[5], m5);  \
  }			   \
  c0+=c0+y;

#define upd0(m0,m1,m2,m3,m4,m5,bc) \
  GV->y20=y<<20;		\
	bcount=bc;		\
	add2order+=MI*10;	\
	{			\
	  int j=y+1 << (2-(bc&3));	\
	  *cp[1]=nex(*cp[1], m1); cp[1]+=j; \
	  *cp[2]=nex(*cp[2], m2); cp[2]+=j; \
	  *cp[3]=nex(*cp[3], m3); cp[3]+=j; \
	  *cp[4]=nex(*cp[4], m4); cp[4]+=j; \
	  *cp[5]=nex(*cp[5], m5); cp[5]+=j; \
	  *cp[0]=nex(*cp[0], m0);\
	}			\
  c0+=c0+y;

#define upd3(m0,m1,m2,m3,m4,m5,bc) \
  cp_update(m0,m1,m2,m3,m4,m5)	\
	bcount=bc;		\
	add2order=add2order4;	\
	  cp[1]=t1b.get0(hash6(c0   -h[1]));\
	  upd3_cp			    \
	  cp[5]=t2b.get0(hash0(c0*P5_MUL-h[5]));\

//add2order=mxr_wx+MI*10*8*add2order0;
//h[1]=(c4&0xffff)<<13-(c4&0xffff);	/* order 2 */
#define upd7(m0,m1,m2,m3,m4,m5) \
  cp_update(m0,m1,m2,m3,m4,m5)		\
    {					\
    int c=c0-256;			\
    add2order=mxr_wx+640*add2order0;\
    c1=(c1>>4)+((c<<1)&240);		\
    mm.buf[mm.pos]=c;				\
    mm.pos=(mm.pos+1)&mm.N;			\
    cc=(cc<<8)+(c8>>24);		\
    c8=(c8<<8)+(c4>>24);		\
    c4=c4<<8|c;				\
    upd7_h123				\
    h[1]=((c4&0xffff)<<13)-(c4&0xffff);	/* order 2 */\
    cp[1]=t1b.get1(hash1(h[1]));	\
    upd7_cp				\
    if (c>=97 && c<=122) c-=32;	/* lowercase unigram word order */ \
    if (c>=65 && c<=90) h[5]=((h[5]<<1)+c)*191;\
    else pw=h[5]*PW_MUL, h[5]=0;	\
    cp[5]=t2a.get1(hash5(h[5]-pw));	\
    c0=1;		\
    bcount=7;		\
    mm.upd();		\
    }

U32 LPQ8::Encoder::Predictor_upd(int y) {
  m_update(y);

  // predict
  int len=mm.p(bcount,c1,len2cxt,c0), pr;
  if (len==0) {
	if (*cp[1]!=0) { len =MI;
	if (*cp[2]!=0) { len+=MI;
	if (*cp[3]!=0) { len+=MI;
	if (*cp[4]!=0)   len+=MI;
	}}}
   }
   else len=len2order[len];
  GV->mxr_cxt=add2order+len;
  GV->m_add(1, sm[1].p1(*cp[1]));
  GV->m_add(2, sm[2].p2(*cp[2]));
  GV->m_add(3, sm[3].p3(*cp[3]));
  GV->m_add(4, sm[4].p4(*cp[4]));
  GV->m_add(5, sm[5].p5(*cp[5]));

  cp[0]=t0c1+c0;
  GV->m_add(6, sm[0].p6(*cp[0]));
  pr=GV->dot_product()+2047;
  pr=squash_t[pr]+3*a1.p1(pr*23, h[0]+c0) >>2;
  mxr_pr=pr;
  pr=pr*5	+11*a2.p2(stretch_t2[pr], fails+prevfail)+8 >>4;
  return pr+(pr<2048);
}


  // Compress bit y
#define enc1(k)\
    int y=(c>>k)&1;\
    U32 xmid=x1 + (x2-x1>>12)*p + ((x2-x1&0xfff)*p>>12);\
    y ? (x2=xmid) : (x1=xmid+1);

#define enc2 \
    p=Predictor_upd(y);\
    while (((x1^x2)&0xff000000)==0) {  /* pass equal leading bytes of range */ \
      Arc->putbyte(x2>>24); \
      x1<<=8;		\
      x2=(x2<<8)+255;	\
    }

  // Return decompressed bit
#define dec1(k) \
    U32 xmid=x1 + (x2-x1>>12)*p + ((x2-x1&0xfff)*p>>12);\
    int y=x<=xmid;					\
    y ? (x2=xmid) : (x1=xmid+1);

#define dec2 \
    p=Predictor_upd(y);\
    while (((x1^x2)&0xff000000)==0) {  /* pass equal leading bytes of range */ \
      x=(x<<8)+(Arc->getbyte()&255);	/* EOF is OK */ \
      x1<<=8;						\
      x2=(x2<<8)+255;					\
    }

#define eight_bits(part1,part2) \
    sm[4].t+=256;\
    sm[0].t+=256;\
    sm[5].t+=256;\
    { part1(7); upd0(1,0,0,3,4,2, 6); part2; }\
    { part1(6); upd0(1,0,0,3,0,1, 5); part2; }	sm[2].t+=256; sm[5].t+=256; \
    { part1(5); upd0(1,0,0,3,0,1, 4); part2; }	sm[1].t+=256;\
    { part1(4); upd3(1,0,1,3,0,1, 3); part2; }	sm[0].t+=256;\
    { part1(3); upd0(1,0,1,3,0,1, 2); part2; }	sm[3].t+=256;\
    { part1(2); upd0(1,0,1,3,0,1, 1); part2; }\
    { part1(1); upd0(1,0,1,2,0,1, 0); part2; }	sm[0].t-=512; sm[1].t-=256; sm[2].t-=256; sm[3].t-=256; sm[5].t-=512;\
						sm[4].t-=256;\
    { part1(0); upd7(1,0,1,2,0,1   ); part2; }

  // Compress one byte
  void LPQ8::Encoder::compress(int c) {
    eight_bits(enc1,enc2)
  }

  // Decompress and return one byte
  int LPQ8::Encoder::decompress() {
    eight_bits(dec1,dec2)
    int c=c4&255;
    return c;
  }

LPQ8::Encoder::Encoder(GlobalVer *G,Mode m, MyIOBase *A):GV(G),
    t1(G,LPQ8MEM/2), t2(G,LPQ8MEM), t3(G,LPQ8MEM/2),mm(G,LPQ8MEM),
    a1(G,24 * 0x10000), a2(G,24 * 0x800),mxr_pr(2048)
    {
    p=2048;
    Arc=A;
    bcount=7;
    x2=0xffffffff;
    c0=1;
  for (int i=0;i<6;i++) sm[i].SetGV(GV);
  GV->allo(calcprevfail,256);
  GV->allo(calcfails,8192);
  memset(h,0,6*sizeof(U32));
  pw=c8=cc=prevfail=c4=c1=x=x1=fails=0;
//  memcpy(len2cxt0,len2,63);
  GV->allo(len2cxt,MAXLEN*2+1);
  GV->allo(len2order,MAXLEN+1);
  GV->allo(t0,0x10000);
  t0c1=t0;
  cp[0]=cp[1]=cp[2]=cp[3]=cp[4]=cp[5]=t0;
  stretch_t=GV->st_t;
  squash_t=GV->sq_t;
  stretch_t2=GV->st_t2;

  int i,pi;
  for (i=0; i<256; ++i) {
    pi=0;
    if (i&  3) pi+=1024;
    if (i& 28) pi+=512;
    if (i&224) pi+=256;
    calcprevfail[i]=pi;
  }

  int	cf1=1216, cf3=2592;

  for (i=-4096; i<4096; ++i) {
    int e=i, v=0;
    if (e<0) e=-e;
    if (e > cf1) v=1;
    if (e > cf3) v=3;
    calcfails[i+4096]=v;
  }

  for (i=2; i<=MAXLEN*2; i+=2) {
      int c = len2[i>>1]*512;
	 len2cxt[i]=c;
	 len2cxt[i-1]=c-256;
	c=i>>1;
	len2order[c]=(5+(c>=8) +(c>=12)+(c>=18)+(c==MAXLEN))*MI;
  }

  GV->allo(mxr_wx, MI*MC);
  for (i=0; i<MI*MC; ++i)	mxr_wx[i] = MXR_INIT;
  mxr_wx4=mxr_wx + MI*40;
  GV->mxr_cxt=mxr_wx;
  add2order=mxr_wx;
}



