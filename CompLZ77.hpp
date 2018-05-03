#ifndef LZ77Commpress_H
#define LZ77Commpress_H

#include "CompComm.hpp"

// 使用在自己的堆中分配索引节点，不滑动窗口
// 每次最多压缩 65536 字节数据
// 的优化版本   

namespace LZ77
{
    // 滑动窗口的字节大小
    enum{_MAX_WINDOW_SIZE=65536};
    //typedef unsigned char U8;
    //typedef unsigned short U16;
    //typedef unsigned int U32;
    //typedef bool bool;
    //#define true true
    //#define false false 
    #define LZ77min(a,b) (((a)>=(b))?(b):(a))

    class CCompress
    {
    public:
        CCompress() {};
        virtual ~CCompress() {};

    public:
        virtual int Compress(U8* src, int srclen, U8* dest) = 0;
        virtual bool Decompress(U8* src, int srclen, U8* dest) = 0;

    protected:
        // tools 

        /////////////////////////////////////////////////////////
        // CopyBitsInAByte : 在一个字节范围内复制位流
        // 参数含义同 CopyBits 的参数
        // 说明：
        //        此函数由 CopyBits 调用，不做错误检查，即
        //        假定要复制的位都在一个字节范围内
        void CopyBitsInAByte(U8* memDest, int nDestPos, 
                      U8* memSrc, int nSrcPos, int nBits)
        {
            U8 b1, b2;
            b1 = *memSrc;
            b1 <<= nSrcPos; b1 >>= 8 - nBits;    // 将不用复制的位清0
            b1 <<= 8 - nBits - nDestPos;        // 将源和目的字节对齐
            *memDest |= b1;        // 复制值为1的位
            b2 = 0xff; b2 <<= 8 - nDestPos;        // 将不用复制的位置1
            b1 |= b2;
            b2 = 0xff; b2 >>= nDestPos + nBits;
            b1 |= b2;
            *memDest &= b1;        // 复制值为0的位
        }
        ////////////////////////////////////////////////////////
        // CopyBits : 复制内存中的位流
        //        memDest - 目标数据区
        //        nDestPos - 目标数据区第一个字节中的起始位
        //        memSrc - 源数据区
        //        nSrcPos - 源数据区第一个字节的中起始位
        //        nBits - 要复制的位数
        //    说明：
        //        起始位的表示约定为从字节的高位至低位（由左至右）
        //        依次为 0，1，... , 7
        //        要复制的两块数据区不能有重合
        void CopyBits(U8* memDest, int nDestPos, 
                      U8* memSrc, int nSrcPos, int nBits)
        {
            int iByteDest = 0, iBitDest;
            int iByteSrc = 0, iBitSrc = nSrcPos;

            int nBitsToFill, nBitsCanFill;

            while (nBits > 0)
            {
                // 计算要在目标区当前字节填充的位数
                nBitsToFill = LZ77min(nBits, iByteDest ? 8 : 8 - nDestPos);
                // 目标区当前字节要填充的起始位
                iBitDest = iByteDest ? 0 : nDestPos;
                // 计算可以一次从源数据区中复制的位数
                nBitsCanFill = LZ77min(nBitsToFill, 8 - iBitSrc);
                // 字节内复制
                CopyBitsInAByte(memDest + iByteDest, iBitDest, 
                    memSrc + iByteSrc, iBitSrc, nBitsCanFill);        
                // 如果还没有复制完 nBitsToFill 个
                if (nBitsToFill > nBitsCanFill)
                {
                    iByteSrc++; iBitSrc = 0; iBitDest += nBitsCanFill;
                    CopyBitsInAByte(memDest + iByteDest, iBitDest, 
                            memSrc + iByteSrc, iBitSrc, 
                            nBitsToFill - nBitsCanFill);
                    iBitSrc += nBitsToFill - nBitsCanFill;
                }
                else 
                {
                    iBitSrc += nBitsCanFill;
                    if (iBitSrc >= 8)
                    {
                        iByteSrc++; iBitSrc = 0;
                    }
                }

                nBits -= nBitsToFill;    // 已经填充了nBitsToFill位
                iByteDest++;
            }    
        }

        //////////////////////////////////////////////////////////////
        // 将U32值从高位字节到低位字节排列
        void InvertDWord(U32* pDW)
        {
            union UDWORD{ U32 dw; U8 b[4]; };
            UDWORD* pUDW = (UDWORD*)pDW;
            U8 b;
            b = pUDW->b[0];    pUDW->b[0] = pUDW->b[3]; pUDW->b[3] = b;
            b = pUDW->b[1];    pUDW->b[1] = pUDW->b[2]; pUDW->b[2] = b;
        }
        /////////////////////////////////////////////////////////////
        // 设置byte的第iBit位为aBit
        //        iBit顺序为高位起从0记数（左起）
        void SetBit(U8* byte, int iBit, U8 aBit)
        {
            if (aBit)
                (*byte) |= (1 << (7 - iBit));
            else
                (*byte) &= ~(1 << (7 - iBit));
        }
        ////////////////////////////////////////////////////////////
        // 得到字节byte第pos位的值
        //        pos顺序为高位起从0记数（左起）
        U8 GetBit(U8 byte, int pos)
        {
            int j = 1;
            j <<= 7 - pos;
            if (byte & j)
                return 1;
            else 
                return 0;
        }
        ////////////////////////////////////////////////////////////
        // 将位指针*piByte(字节偏移), *piBit(字节内位偏移)后移num位
        void MovePos(int* piByte, int* piBit, int num)
        {
            num += (*piBit);
            (*piByte) += num / 8;
            (*piBit) = num % 8;
        }
        /////////////////////////////////////////////////////////
        // 取log2(n)的upper_bound
        int UpperLog2(int n)
        {
            int i = 0;
            if (n > 0)
            {
                int m = 1;
                while(1)
                {
                    if (m >= n)
                        return i;
                    m <<= 1;
                    i++;
                }
            }
            else 
                return -1;
        }

        /////////////////////////////////////////////////////////
        // 取log2(n)的lower_bound
        int LowerLog2(int n)
        {
            int i = 0;
            if (n > 0)
            {
                int m = 1;
                while(1)
                {
                    if (m == n)
                        return i;
                    if (m > n)
                        return i - 1;
                    m <<= 1;
                    i++;
                }
            }
            else 
                return -1;
        }
    };

    class CCompressLZ77 : public CCompress
    {
    public:
        CCompressLZ77(){SortHeap = new struct STIDXNODE[_MAX_WINDOW_SIZE];}
        virtual ~CCompressLZ77(){delete[] SortHeap;}
    public:
        /////////////////////////////////////////////
        // 压缩一段字节流
        // src - 源数据区
        // srclen - 源数据区字节长度, srclen <= 65536
        // dest - 压缩数据区，调用前分配srclen字节内存
        // 返回值 > 0 压缩数据长度
        // 返回值 = 0 数据无法压缩
        // 返回值 < 0 压缩中异常错误
        int Compress(U8* src, int srclen, U8* dest)
        {
            int i;
            CurByte = 0; CurBit = 0;    
            int off, len;

            if (srclen > 65536) 
                return -1;

            pWnd = src;
            _InitSortTable();
            for (i = 0; i < srclen; i++)
            {        
                if (CurByte >= srclen)
                    return 0;
                if (_SeekPhase(src, srclen, i, &off, &len))
                {            
                    // 输出匹配术语 flag(1bit) + len(γ编码) + offset(最大16bit)
                    _OutCode(dest, 1, 1, false);
                    _OutCode(dest, len, 0, true);

                    // 在窗口不满64k大小时，不需要16位存储偏移
                    _OutCode(dest, off, UpperLog2(nWndSize), false);
                                
                    _ScrollWindow(len);
                    i += len - 1;
                }
                else
                {
                    // 输出单个非匹配字符 0(1bit) + char(8bit)
                    _OutCode(dest, 0, 1, false);
                    _OutCode(dest, (U32)(src[i]), 8, false);
                    _ScrollWindow(1);
                }
            }
            int destlen = CurByte + ((CurBit) ? 1 : 0);
            if (destlen >= srclen)
                return 0;
            return destlen;
        }
        /////////////////////////////////////////////
        // 解压缩一段字节流
        // src - 接收原始数据的内存区, srclen <= 65536
        // srclen - 源数据区字节长度
        // dest - 压缩数据区
        // 返回值 - 成功与否
        bool Decompress(U8* src, int srclen, U8* dest)
        {
            int i;
            CurByte = 0; CurBit = 0;
            pWnd = src;        // 初始化窗口
            nWndSize = 0;

            if (srclen > 65536) 
                return false;
            
            for (i = 0; i < srclen; i++)
            {        
                U8 b = GetBit(dest[CurByte], CurBit);
                MovePos(&CurByte, &CurBit, 1);
                if (b == 0) // 单个字符
                {
                    CopyBits(src + i, 0, dest + CurByte, CurBit, 8);
                    MovePos(&CurByte, &CurBit, 8);
                    nWndSize++;
                }
                else        // 窗口内的术语
                {
                    int q = -1;
                    while (b != 0)
                    {
                        q++;
                        b = GetBit(dest[CurByte], CurBit);
                        MovePos(&CurByte, &CurBit, 1);                
                    }
                    int len, off;
                    U32 dw = 0;
                    U8* pb;
                    if (q > 0)
                    {                
                        pb = (U8*)&dw;
                        CopyBits(pb + (32 - q) / 8, (32 - q) % 8, dest + CurByte, CurBit, q);
                        MovePos(&CurByte, &CurBit, q);
                        InvertDWord(&dw);
                        len = 1;
                        len <<= q;
                        len += dw;
                        len += 1;
                    }
                    else
                        len = 2;

                    // 在窗口不满64k大小时，不需要16位存储偏移
                    dw = 0;
                    pb = (U8*)&dw;
                    int bits = UpperLog2(nWndSize);
                    CopyBits(pb + (32 - bits) / 8, (32 - bits) % 8, dest + CurByte, CurBit, bits);
                    MovePos(&CurByte, &CurBit, bits);
                    InvertDWord(&dw);
                    off = (int)dw;
                    // 输出术语
                    for (int j = 0; j < len; j++)
                    {
                        //_ASSERT(i + j <  srclen);
                        //_ASSERT(off + j <  _MAX_WINDOW_SIZE);
                        
                        src[i + j] = pWnd[off + j];
                    }
                    nWndSize += len;
                    i += len - 1;
                }
                // 滑动窗口
                if (nWndSize > _MAX_WINDOW_SIZE)
                {
                    pWnd += nWndSize - _MAX_WINDOW_SIZE;
                    nWndSize = _MAX_WINDOW_SIZE;            
                }
            }
            return true;
        }
    protected:
        U8* pWnd;
        // 窗口大小最大为 64k ，并且不做滑动
        // 每次最多只压缩 64k 数据，这样可以方便从文件中间开始解压
        // 当前窗口的长度
        int nWndSize;
        // 对滑动窗口中每一个2字节串排序
        // 排序是为了进行快速术语匹配
        // 排序的方法是用一个64k大小的指针数组
        // 数组下标依次对应每一个2字节串：(00 00) (00 01) ... (01 00) (01 01) ...
        // 每一个指针指向一个链表，链表中的节点为该2字节串的每一个出现位置
        struct STIDXNODE
        {
            U16 off;        // 在src中的偏移
            U16 off2;        // 用于对应的2字节串为重复字节的节点
                            // 指从 off 到 off2 都对应了该2字节串
            U16 next;        // 在SortHeap中的指针
        };
        
        U16 SortTable[65536];  // 256 * 256 指向SortHeap中下标的指针

        // 因为窗口不滑动，没有删除节点的操作，所以
        // 节点可以在SortHeap 中连续
        struct STIDXNODE* SortHeap;
        int HeapPos;    // 当前分配位置

        // 当前输出位置(字节偏移及位偏移)
        int CurByte, CurBit;

    protected:
        ////////////////////////////////////////
        // 输出压缩码
        // code - 要输出的数
        // bits - 要输出的位数(对isGamma=true时无效)
        // isGamma - 是否输出为γ编码
        void _OutCode(U8* dest, U32 code, int bits, bool isGamma)
        {    
            if ( isGamma )
            {
                U8* pb;
                U32 out;
                // 计算输出位数
                int GammaCode = (int)code - 1;
                int q = LowerLog2(GammaCode);
                if (q > 0)
                {
                    out = 0xffff;
                    pb = (U8*)&out;
                    // 输出q个1
                    CopyBits(dest + CurByte, CurBit, 
                        pb, 0, q);
                    MovePos(&CurByte, &CurBit, q);
                }
                // 输出一个0
                out = 0;
                pb = (U8*)&out;        
                CopyBits(dest + CurByte, CurBit, pb + 3, 7, 1);
                MovePos(&CurByte, &CurBit, 1);
                if (q > 0)
                {
                    // 输出余数, q位
                    int sh = 1;
                    sh <<= q;
                    out = GammaCode - sh;
                    pb = (U8*)&out;
                    InvertDWord(&out);
                    CopyBits(dest + CurByte, CurBit, 
                        pb + (32 - q) / 8, (32 - q) % 8, q);
                    MovePos(&CurByte, &CurBit, q);
                }
            }
            else 
            {
                U32 dw = (U32)code;
                U8* pb = (U8*)&dw;
                InvertDWord(&dw);
                CopyBits(dest + CurByte, CurBit, 
                        pb + (32 - bits) / 8, (32 - bits) % 8, bits);
                MovePos(&CurByte, &CurBit, bits);
            }
        }
        ///////////////////////////////////////////////////////////
        // 在滑动窗口中查找术语
        // nSeekStart - 从何处开始匹配
        // offset, len - 用于接收结果，表示在滑动窗口内的偏移和长度
        // 返回值- 是否查到长度为3或3以上的匹配字节串
        bool _SeekPhase(U8* src, int srclen, int nSeekStart, int* offset, int* len)
        {    
            int j, m, n;

            if (nSeekStart < srclen - 1)
            {
                U8 ch1, ch2;
                ch1 = src[nSeekStart]; ch2 = src[nSeekStart + 1];
                U16 p;
                p = SortTable[ch1 * 256 + ch2];
                if (p != 0)
                {
                    m = 2; n = SortHeap[p].off;
                    while (p != 0)
                    {
                        j = _GetSameLen(src, srclen, 
                            nSeekStart, SortHeap[p].off);
                        if ( j > m )
                        { 
                            m = j; 
                            n = SortHeap[p].off;
                        }            
                        p = SortHeap[p].next;
                    }    
                    (*offset) = n; 
                    (*len) = m;
                    return true;        
                }    
            }
            return false;
        }
        ///////////////////////////////////////////////////////////
        // 得到已经匹配了3个字节的窗口位置offset
        // 共能匹配多少个字节
        inline int _GetSameLen(U8* src, int srclen, int nSeekStart, int offset)
        {
            int i = 2; // 已经匹配了2个字节
            int maxsame = LZ77min(srclen - nSeekStart, nWndSize - offset);
            while (i < maxsame
                    && src[nSeekStart + i] == pWnd[offset + i])
                i++;
            return i;
        }
        //////////////////////////////////////////
        // 将窗口向右滑动n个字节
        inline void _ScrollWindow(int n)
        {    
            for (int i = 0; i < n; i++)
            {        
                nWndSize++;        
                if (nWndSize > 1)            
                    _InsertIndexItem(nWndSize - 2);
            }
        }
        // 向索引中添加一个2字节串
        inline void _InsertIndexItem(int off)
        {
            U16 q;
            U8 ch1, ch2;
            ch1 = pWnd[off]; ch2 = pWnd[off + 1];    
            
            if (ch1 != ch2)
            {
                // 新建节点
                q = HeapPos;
                HeapPos++;
                SortHeap[q].off = off;
                SortHeap[q].next = SortTable[ch1 * 256 + ch2];
                SortTable[ch1 * 256 + ch2] = q;
            }
            else
            {
                // 对重复2字节串
                // 因为没有虚拟偏移也没有删除操作，只要比较第一个节点
                // 是否和 off 相连接即可
                q = SortTable[ch1 * 256 + ch2];
                if (q != 0 && off == SortHeap[q].off2 + 1)
                {        
                    // 节点合并
                    SortHeap[q].off2 = off;
                }        
                else
                {
                    // 新建节点
                    q = HeapPos;
                    HeapPos++;
                    SortHeap[q].off = off;
                    SortHeap[q].off2 = off;
                    SortHeap[q].next = SortTable[ch1 * 256 + ch2];
                    SortTable[ch1 * 256 + ch2] = q;
                }
            }
        }
        // 初始化索引表，释放上次压缩用的空间
        void _InitSortTable()
        {
            memset(SortTable, 0, sizeof(U16) * 65536);
            nWndSize = 0;
            HeapPos = 1;
        }
    };
}//namespace
class LZ77Coder : public CoderBase //Encode class
{
private:
    LZ77::CCompressLZ77 C;
	U8 soubuf[65536];
	U8 destbuf[65536 + 16];

    inline void Compress(MyIOBase *Source)
    {
        unsigned long last = Head.usize;
		int act;
		U16 flag1, flag2;
		while ( last > 0 )
		{
			act = LZ77min(65536, last);
			Source->read((char *)soubuf, act);
			last -= act;
			flag1=(act == 65536)?0:act;
			Archive->write((char *)&flag1, sizeof(U16));

			int destlen = C.Compress(soubuf, act,destbuf);
			if (destlen == 0)		// can't compress the block
			{
				flag2 = flag1;
				Archive->write((char *)&flag2, sizeof(U16));
				Archive->write((char *)soubuf, act);
			}
			else
			{
				flag2 = (U16)destlen;
				Archive->write((char *)&flag2, sizeof(U16));
				Archive->write((char *)destbuf, destlen);
			}
		}
    }
    inline void UnCompress(MyIOBase *Source)
    {
		unsigned long last = Head.usize;
		int act;
		U16 flag1, flag2;
		while (last > 0)
		{
			Archive->read((char *)&flag1,sizeof(U16));
			Archive->read((char *)&flag2, sizeof(U16));
			act=(flag1 == 0)?65536:flag1;
			last-= act;

			if (flag2 == flag1)
				Archive->read((char *)soubuf, act);
			else
			{
				Archive->read((char *)destbuf, flag2);
				if (!C.Decompress(soubuf, act,destbuf))
				{
					EndCode=true;
					ErrMsg="LZ77 Decompress error";
					return;
				}
			}
			Source->write((char *)soubuf, act);				
		}
    }
public:
    LZ77Coder(bool m,MyIOBase *p=NULL):CoderBase(p)
    {
      MathodN="LZ77";
      Mode=m;
    }
    //Mode: true for encode,false for decode
};
#endif 
