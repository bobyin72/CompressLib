#ifndef CompressCommon_H
#define CompressCommon_H

#include "MYIOBase.hpp"
#define NDEBUG  // remove for debugging
using namespace std;

// 8, 16, 32 bit unsigned types (adjust as appropriate)
typedef unsigned char  U8;
typedef unsigned short U16;
typedef unsigned int   U32;

#define  CompProName "BobC10"
typedef struct
{
    char ProN[7]; //结构名称
    char Mathod[5]; //算法名称
    unsigned long usize; //原长度
    unsigned long csize; //压缩后长度
} CompressHead;

class CoderBase //Compress Base class
{
protected:
    MyIOBase *Archive;
    string MathodN,ProN,FileName;

    void SetHead()
    {
        char *c=(char *)&Head;

        memcpy(Head.ProN,ProN.c_str(),7);
        memcpy(Head.Mathod,MathodN.c_str(),5);
        for(unsigned i=0;i<sizeof(Head);i++)
            Archive->putbyte(c[i]);
        //c=FileName.c_str();
        if (FileName.size()>0)
            for(unsigned i=0;i<FileName.size();i++)
                Archive->putbyte(FileName.c_str()[i]);
        Archive->putbyte('\0');
    }
    bool GetHead(bool reset=false)
    {
        bool b;
        char *c=(char *)&Head,cc;
        unsigned long n=Archive->tell();

        for(unsigned i=0;i<sizeof(Head);i++)
            c[i]=Archive->getbyte();
        FileName="";
        do
        {
            cc=Archive->getbyte();
            FileName+=cc;
        } while (cc!='\0');

        Head.ProN[6]='\0';
        Head.Mathod[4]='\0';
        b=(ProN==Head.ProN);
        b=b&(MathodN==Head.Mathod);
        Headlen=Archive->tell()-n;
        if (reset) Archive->seek(n);
        return b;
    }
    bool Mode; // mode: TRUE is encode; False is decode
    virtual void Compress(MyIOBase *Source)=0;
    virtual void UnCompress(MyIOBase *Source)=0;
    string ErrMsg;
    bool EndCode;
    CompressHead Head;
    int Headlen;
public:
    CoderBase(MyIOBase *p=NULL):Archive(p){ProN=CompProName;}
    virtual ~CoderBase(){}

    inline string geterrmsg(){return ErrMsg;}
    inline bool getendcode(){return EndCode;}
    inline string getmathod(){return MathodN;}
    inline string getproname(){return ProN;}
    inline void SetArchive(MyIOBase *p)
    {
        Archive=p;
        if (!Mode) GetHead(true);
    }
    inline void CpHead(CompressHead *H){memcpy(H,&Head,sizeof(CompressHead));}

    void ShowHead()
    {
        cout << "    Archive Bloack Head Info:" << endl;
        cout << "              ProName:[" << Head.ProN << ']' <<endl;
        cout << "           MathodName:[" << Head.Mathod << ']' <<endl;
        cout << "            File Name:[" << FileName << ']' << endl;
        cout << "    UnCompressed Size:[" << Head.usize << "]Bytes" << endl;
        cout << "      Compressed Size:[" << Head.csize << "]Bytes" <<endl;
        cout << "       Compress Ratio:[" << Head.csize*100.0/Head.usize << "%]" << endl;
        cout << "    -------------------------" << endl;
    }

    inline unsigned long decode(MyIOBase *Source,string Index,unsigned long &sl) //do compress, default is store
    {
        if (Mode)
        {
            EndCode=false;
            ErrMsg="This EnCoder!";
            return 0;
        }
        if (Archive==NULL)
        {
            EndCode=false;
            ErrMsg="Archive not init!";
            return 0;
        }
        if (!GetHead())
        {
            EndCode=false;
            ErrMsg="Head error!";
            return 0;
        }

        if (Head.usize==0)
        {
            EndCode=false;
            ErrMsg="Uncompress size is Zero!";
            return 0;
        }
        sl=Head.usize;
        Archive->R=true;
        Archive->SetEnd(Head.csize-Headlen-3);

        EndCode=true;
        UnCompress(Source);
        if (!EndCode) return 0;

        Archive->R=false;

        bool tailok=false;
        while (!Archive->isend())
        {
            if (Archive->getbyte()=='|')
                if (Archive->getbyte()=='|')
                    {
                        tailok=true;
                        break;
                    }
        }
        if (!tailok)
        {
            EndCode=false;
            ErrMsg="Tail Bad!";
        }
        else EndCode=true;
        if (EndCode)
            return Head.csize;
        else
            return 0;
    }

    inline unsigned long encode(MyIOBase *Source,string Index,unsigned long &sl) //do compress, default is store
    {
        unsigned long n1=Archive->tell(),n2;

        if (!Mode)
        {
            EndCode=false;
            ErrMsg="This DeCoder!";
            return 0;
        }

        if (Archive==NULL)
        {
            EndCode=false;
            ErrMsg="Archive not init!";
            return 0;
        }

        sl=Source->length();
        if (sl==0)
        {
            EndCode=false;
            ErrMsg="Uncompress size is Zero!";
            return 0;
        }

        Source->top();
        Head.usize=sl;
        FileName=Index;
        SetHead();

        EndCode=true;
        Compress(Source);
        if (!EndCode) return 0;

        Archive->putbyte(0);
        Archive->putbyte('|');
        Archive->putbyte('|');

        n2=Archive->tell();
        Head.csize=n2-n1;
        Archive->seek(n1);
        SetHead();
        Archive->seek(n2);

        EndCode=true;
        return Head.csize;       //注意memory 和 iostream 的区别
    }
};

class StorCoder : public CoderBase //stor class
{
private:
    inline void Compress(MyIOBase *Source)
    {
        for (unsigned long i = 0; i < Head.usize; i++)
        {
            Archive->putbyte(Source->getbyte());
        }
    }
    inline void UnCompress(MyIOBase *Source)
    {
        for (unsigned long i = 0; i < Head.usize; i++)
        {
            Source->putbyte(Archive->getbyte());
        }
    }
public:
    StorCoder(bool m,MyIOBase *p=NULL):CoderBase(p){MathodN="STOR";Mode=m;}
    //Mode: true for encode,false foe decode
};
#endif
