#include "BobComp.hpp"

char *sbuf,*dbuf;
#define BUFLEN (3*1024*1024)

bool doComp(CoderBase *E, MyIOBase *EA, MyIOBase *ES,bool b,string FN)
{
    int start_time;
    double elapsed_time;
    CompressHead H;
    unsigned long sl=0;
    char s[100];

    E->SetArchive(EA);

    start_time=clock();
    if (b)
    {
        E->encode(ES,FN,sl);
        if (!E->getendcode())
        {
            cout << "Encode error, msg:" << E->geterrmsg() << endl;
            return false;
        }
        E->CpHead(&H);
//        E->ShowHead();
    }
    else
    {
        E->decode(ES,FN,sl);
        if (!E->getendcode())
        {
            cout << "Decode error, msg:" << E->geterrmsg() << endl;
            return false;
        }
        E->CpHead(&H);
    }
    elapsed_time=double(clock()-start_time)/CLOCKS_PER_SEC;
    elapsed_time=(elapsed_time==0)?0.001:elapsed_time;
    sprintf(s,"[%lu/%lu %1.2f%% %1.2fs %4.0f KB/s]",H.csize,H.usize,
            (float)H.csize*100.0/H.usize,elapsed_time,
            H.usize/(elapsed_time*1000.0));
    cout << s;
    if (!b) cout << endl;
    return true;
}

bool CheckFF(CoderBase *E,CoderBase *R,string FN)
{
    string AName=E->getmathod()+'_'+FN+".F2F";

    cout << "F:" ;
    FILE *A,*F;
    A=fopen(AName.c_str(), "wb");
    if (!A) {
      cout << "Cannot create archive: " << AName << endl;;
      return false;
    }
    F=fopen(FN.c_str(), "rb");
    if (!F) {
      cout << "Cannot open file: " << FN << endl;
      return false;
    }
    MyFileIO EA(A),ES(F);
    if (!doComp(E,&EA,&ES,true,FN)) return false;
    fclose(A);
    fclose(F);

    FN=AName+".check";
    A=fopen(AName.c_str(), "rb");
    if (!A) {
      cout << "Cannot create archive: " << AName << endl;;
      return false;
    }
    F=fopen(FN.c_str(), "wb");
    if (!F) {
      cout << "Cannot open file: " << FN << endl;
      return false;
    }

    MyFileIO RS(F),RA(A);
    if (!doComp(R,&RA,&RS,false,FN)) return false;
    fclose(A);
    fclose(F);

    return true;
}

bool CheckSS(CoderBase *E,CoderBase *R,string FN)
{
    string AName=E->getmathod()+'_'+FN+".S2S";

    cout << "S:";
    fstream A,F;

    A.open(AName.c_str(), ios::out|ios::binary);
    if (!A) {
      cout << "Cannot create archive: " << AName << endl;;
      return false;
    }
    F.open(FN.c_str(), ios::in|ios::binary);
    if (!F) {
      cout << "Cannot open file: " << FN << endl;
      return false;
    }

    MyStreamIn ES(F);
    MyStreamOut EA(A);
    if (!doComp(E,&EA,&ES,true,FN)) return false;
    A.close();
    A.clear();
    F.close();
    F.clear();

    FN=AName+".check";
    A.open(AName.c_str(), ios::in|ios::binary);
    if (!A) {
      cout << "Cannot create archive: " << AName << endl;;
      return false;
    }
    F.open(FN.c_str(), ios::out|ios::binary);
    if (!F) {
      cout << "Cannot open file: " << FN << endl;
      return false;
    }

    MyStreamIn  RA(A);
    MyStreamOut RS(F);
    if (!doComp(R,&RA,&RS,false,FN)) return false;
    A.close();
    F.close();
    return true;
}

bool CheckMM(CoderBase *E,CoderBase *R, string FN)
{
    string AName=E->getmathod()+'_'+FN+".M2M";
    unsigned long len;
    CompressHead H;

    cout << "M:";
    fstream A,F;
    A.open(AName.c_str(), ios::out|ios::binary);
    if (!A) {
      cout << "Cannot create archive: " << AName << endl;;
      return false;
    }
    F.open(FN.c_str(), ios::in|ios::binary);
    if (!F) {
      cout << "Cannot open file: " << FN << endl;
      return false;
    }

    F.seekg(0,ios::end);
    len=F.tellg();
    F.seekg(0,ios::beg);
    if (len>BUFLEN)
    {
      cout << "File too Big" << endl;
      return false;
    }
    
    F.read(sbuf,len);
    MyMemoryIO ES(sbuf,len),EA(dbuf,BUFLEN);
    if (!doComp(E,&EA,&ES,true,FN)) return false;
    E->CpHead(&H);
    A.write(dbuf,H.csize);
    A.close();
    F.close();
    F.clear();

    FN=AName+".check";
    memset(sbuf,0,len+1024);

    MyMemoryIO RS(sbuf,len),RA(dbuf,H.csize);
    if (!doComp(R,&RA,&RS,false,FN)) return false;
    R->CpHead(&H);
    F.open(FN.c_str(), ios::out|ios::binary);
    if (!F) {
      cout << "Cannot create archive: " << AName << endl;;
      return false;
    }
    F.write(sbuf,H.usize);
    F.close();

    return true;
}

bool Check(CoderBase *E,CoderBase *R, string FN)
{
    bool Re=true;
    cout << "Encoder:[" << E->getproname() << '-' << E->getmathod() << ']' ;
    cout << " Decoder:[" << R->getproname() << '-' << R->getmathod() << ']' << endl;
    cout << "-----------------------------------------------------------------\n";
    Re=CheckMM(E,R,FN);
    if (Re) Re=Re&&CheckFF(E,R,FN);
//    if (Re) Re=Re&&CheckSS(E,R,FN);
    return Re;
}

int main(int argc, char** argv)
{
    if (argc!=2)
    {
        cout << "Usage:" << endl;
        cout << argv[0] << " filename" << endl;
        return 1;
    }
    CompFactory CF;
    cout << "Processing file:[" << argv[1] << ']' << endl << endl;
    bool Re=true;
    sbuf=new char[BUFLEN];
    dbuf=new char[BUFLEN];

    if (Re) Check(CF.CreatCoder(BobCompress,Comp_Stor),
                  CF.CreatCoder(BobUnCompress,Comp_Stor),
                  argv[1]);
    if (Re) Check(CF.CreatCoder(BobCompress,Comp_SR3A),
                  CF.CreatCoder(BobUnCompress,Comp_SR3A),
                  argv[1]);
    if (Re) Check(CF.CreatCoder(BobCompress,Comp_FPAQ),
                  CF.CreatCoder(BobUnCompress,Comp_FPAQ),
                  argv[1]);
    if (Re) Check(CF.CreatCoder(BobCompress,Comp_LPQ8),
                  CF.CreatCoder(BobUnCompress,Comp_LPQ8),
                  argv[1]);
    if (Re) Check(CF.CreatCoder(BobCompress,Comp_LZW),
                  CF.CreatCoder(BobUnCompress,Comp_LZW),
                  argv[1]);
    if (Re) Check(CF.CreatCoder(BobCompress,Comp_LZAR),
                  CF.CreatCoder(BobUnCompress,Comp_LZAR),
                  argv[1]);
    if (Re) Check(CF.CreatCoder(BobCompress,Comp_LZ77),
                  CF.CreatCoder(BobUnCompress,Comp_LZ77),
                  argv[1]);
    
    delete []sbuf;
    delete []dbuf;
    return 0;
}
