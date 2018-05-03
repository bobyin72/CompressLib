#ifndef MYIOBase_H
#define MYIOBase_H

#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <cmath>
#include <ctime>
#include <cstring>
#include <new>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>
using namespace std;

class MyIOBase
{
protected:
    inline virtual int getone()=0;
    inline virtual size_t _read(char *p, size_t size)=0;
    size_t End; //读取数据的限制,结束标尺
public:
    bool R;
    inline virtual int getbyte()
    {
        return ((R)&&((tell()>=End)))?-1:getone();
    }
    inline size_t read(char *p, size_t size)
    { 
        return _read(p,(R&&((tell()+size)>End)?(End-tell()):size));
        /*if (R)
        {   if ((tell()+size)>End)
                return read(p,End-tell());
        }else return read(p,size);*/
    }
    inline virtual void putbyte(int c)=0;
    inline virtual void top()=0;
    inline virtual void bottom()=0;
    inline virtual bool isend()=0;
    inline virtual size_t tell()=0;
    inline virtual void seek(long l)=0;
    inline virtual size_t write(const char *p, size_t size)=0;
    inline void SetEnd(unsigned long csize){End=tell()+csize;}
    unsigned long length()
    {
        unsigned long l,n=tell();
        bottom();
        l=tell();
        seek(n);
        return l;
    }
    MyIOBase(){R=false;End=0;};
    virtual ~MyIOBase(){}
};

class MyFileIO : public MyIOBase
{
private:
    FILE *fp;
    inline virtual int getone(){return getc(fp);}
    inline virtual size_t _read(char *p, size_t size){return fread(p,size,1,fp);} 
public:
    MyFileIO(FILE *f):fp(f){length();}

    inline virtual void putbyte(int c){putc(c, fp);}
    inline virtual bool isend(){return feof(fp);}
    inline virtual void top(){fseek (fp,0,SEEK_SET);}
    inline virtual void bottom(){fseek(fp,0,SEEK_END);}
    inline virtual size_t tell(){return ftell(fp);}
    inline virtual void seek(long l){fseek(fp,l,SEEK_SET);}
    inline virtual size_t write(const char *p, size_t size){return fwrite(p,size,1,fp);};

    inline FILE *GetFP(){return fp;}
};

class MyStreamIn: public MyIOBase
{
private:
    istream *IO;
    inline virtual int getone(){return IO->get();}
    inline virtual size_t _read(char * p, size_t size){IO->read(p,size); return size;}
public:
    MyStreamIn(istream &p):IO(&p){}
    inline virtual void putbyte(int c){}
    inline virtual bool isend(){return IO->eof();}
    inline virtual void top(){IO->seekg (0, ios::beg);}
    inline virtual void bottom(){IO->seekg(0,ios::end);}
    inline virtual size_t tell(){return IO->tellg();}
    inline virtual void seek(long l){IO->seekg(l,ios::beg);}
    inline virtual size_t write(const char *p, size_t size){return 0;}

    inline istream *GetIOStream(){return IO;}
};

class MyStreamOut: public MyIOBase
{
private:
    ostream *IO;
    inline virtual int getone(){return -1;}
    inline virtual size_t _read(char * p, size_t size){return -1;}
public:
    MyStreamOut(ostream &p):IO(&p){}
    inline virtual void putbyte(int c){IO->put(c);}
    inline virtual bool isend(){return IO->eof();}
    inline virtual void top(){IO->seekp(0, ios::beg);}
    inline virtual void bottom(){IO->seekp(0,ios::end);}
    inline virtual size_t tell(){return IO->tellp();}
    inline virtual void seek(long l){IO->seekp(l,ios::beg);}
    inline virtual size_t write(const char *p, size_t size){IO->write(p,size);return size;}

    inline ostream *GetIOStream(){return IO;}
};

class MyMemoryIO: public MyIOBase
{
private:
    unsigned char *buf;
    unsigned long buflen,index;
    inline virtual int getone(){return (index<buflen)?buf[index++]:EOF;}
    inline virtual size_t _read(char * p, size_t size)
    {
        size_t s=((index+size)<buflen)?size:(buflen-index);
        memcpy(p,&buf[index],s);
        index+=s;
        return s;
    }
public:
    MyMemoryIO(void *b, long l):buflen(l),index(0){buf=(unsigned char *)b;}
    inline virtual void putbyte(int c){if (index<buflen) buf[index++]=c;}
    inline virtual bool isend(){return (index==buflen);}
    inline virtual void top(){index=0;}
    inline virtual void bottom(){index=buflen;}
    inline virtual size_t tell(){return index;}
    inline virtual void seek(long l){index=((unsigned long)l<buflen)?l:index;}
    inline virtual size_t write(const char *p, size_t size)
    {
        size_t s=((index+size)<buflen)?size:(buflen-index);
        memcpy(&buf[index],p,s);
        index+=s;
        return s;
    }

    inline void *GetBuf(){return buf;}
    inline long GetBufLen(){return buflen;}
};
#endif

