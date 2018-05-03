#ifndef BobCompress_H
#define BobCompress_H

#include "CompLZAR.hpp"
#include "CompComm.hpp"
#include "CompSR3A.hpp"
#include "CompFPAQ.hpp"
#include "CompLPQ8.hpp"
#include "CompLZW.hpp"
#include "CompLZ77.hpp"
#include "CompZpaq.hpp"

void libzpaq::error(const char* msg) {  // print message and exit
    fprintf(stderr, "Oops: %s\n", msg);
    exit(1);
  }

#define BobCompress true
#define BobUnCompress false
enum{ Comp_Stor=0,Comp_SR3A=1,Comp_FPAQ=2,
      Comp_LPQ8=3,Comp_LZW=4,Comp_LZAR=5,
      Comp_LZ77=6}; //—πÀıÀ„∑®

class CompFactory
{
private:
    vector<CoderBase *> buf;
public:
    ~CompFactory()
    {
        if (buf.size()!=0)
        {
            for (unsigned int i=0;i< buf.size();i++)
                if (buf[i]!=NULL)
                    delete buf[i];
        }
    }
    CoderBase *CreatCoder(bool Mode,int Mathod=Comp_FPAQ)
    {
        CoderBase *C=NULL;
        if (Mathod==Comp_Stor)
            C=new StorCoder(Mode);
        if (Mathod==Comp_SR3A)
            C=new SR3ACoder(Mode);
        if (Mathod==Comp_FPAQ)
            C=new FPAQCoder(Mode);
        if (Mathod==Comp_LZW)
            C=new LZWCoder(Mode);
        if (Mathod==Comp_LPQ8)
            C=new LPQ8Coder(Mode);
        if (Mathod==Comp_LZAR)
            C=new LZARCoder(Mode);
        if (Mathod==Comp_LZ77)
            C=new LZ77Coder(Mode);
        if (C!=NULL)
            buf.push_back(C);
        return C;
    }
    void DeleteCoder(CoderBase *C)
    {
        if (buf.size()!=0)
        {
            for (unsigned int i=0;i<buf.size();i++)
                if (buf[i]==C)
                {
                    delete C;
                    buf[i]=NULL;
                }
        }
    }
    string GetMathodName(int Mathod)
    {
        string s="UnKnown";
        switch (Mathod)
        {
            case Comp_Stor:
                s="Stor";
                break;
            case Comp_SR3A:
                s="SR3A";
                break;
            case Comp_FPAQ:
                s="FPAQ";
                break;
            case Comp_LPQ8:
                s="LPQ8";
                break;
            case Comp_LZW:
                s=" LZW";
                break;
            case Comp_LZAR:
                s="LZAR";
                break;
            case Comp_LZ77:
                s="LZ77";
                break;
        }
        return s;
    }

};
#endif

