/*
      file: basehtmlparser.h
      desc: simple html parser, return every tag and its attributes and texts
      author: chen hua 
      create: 2005-03-16
*/
#ifndef _BaseHtmlParser_H_
#define _BaseHtmlParser_H_
#include <vector>
#include <string>
using namespace std; 

#ifndef WIN32
#define strnicmp strncasecmp
#endif 

class CBaseHtmlParser
{
public:
	 //!struct to save a string, with a pointer and a size
	 struct SZ_STRING
	 {
	  const char* pbData;
	  size_t cbData;
	 }; 
	 CBaseHtmlParser(){}
	 virtual ~CBaseHtmlParser(){} 

	 //Init or Destroy, do nothing here now
	 virtual void Initialize(){}
	 virtual void Destroy(){} 

	 //Two interface to parser html page
	 virtual bool Parse(const string& URL,const string& Content)
	{
		 SZ_STRING strUrl,strContent;
		 strUrl.pbData=URL.c_str();
		 strUrl.cbData=URL.length();
		 strContent.pbData=Content.c_str();
		 strContent.cbData=Content.length();
		 
		 return Parse(strUrl,strContent);
	} 
	 virtual bool Parse(const SZ_STRING &strUrl,const SZ_STRING &strContent)
	{
		 sprintf(m_szBaseURL,"%.*s",strUrl.cbData,strUrl.pbData);
		 char* pend=strrchr(m_szBaseURL,'/');
		 if(pend!=NULL)
		 {
		  pend++;
		  *pend='\0';
		 }
		 sprintf(m_szBaseDomain,"%.*s",strUrl.cbData,strUrl.pbData);
		 pend=strchr(m_szBaseDomain+strlen("http://")+1,'/');
		 if(pend!=NULL)
		 {
		  *pend='\0';
		 } 

		 size_t i;
		 size_t nContent=strContent.cbData;
		 const char* pContent=(const char*)(strContent.pbData);
		 for(i=0;i<nContent;)
		 {
		  if(pContent[i]=='<')//TagStart
		  {
		   if(nContent>4)
		   if((i<nContent-4)&&(pContent[i+1]=='!')&&(pContent[i+2]=='-')&&(pContent[i+3]=='-'))//comment start
		   {
			i+=4;
			size_t nCommentStart=i; 

			if(nContent>3)
			while(i<nContent-3)
			{
			 if((pContent[i]=='-')&&(pContent[i+1]=='-')&&(pContent[i+2]=='>'))//comment end
			 {
			  SZ_STRING strComment;
			  strComment.pbData=pContent+nCommentStart;
			  strComment.cbData=i-nCommentStart;
			  OnComment(strComment);
			  i+=3;
			  break;
			 }
			 i++;
			}
			continue;
		   }
		   //tag here
		   size_t nTagNameStart=i+1;
		   while((nTagNameStart<nContent)&&(pContent[nTagNameStart]==' '))
			nTagNameStart++;
		   size_t nTagNameEnd=nTagNameStart+1;
		   while((nTagNameEnd<nContent)&&(pContent[nTagNameEnd]!=' ')&&(pContent[nTagNameEnd]!='>'))
			nTagNameEnd++; 

		   SZ_STRING strTagName;
		   strTagName.pbData = pContent+nTagNameStart;
		   strTagName.cbData = nTagNameEnd-nTagNameStart; 

		   size_t nTagEnd=nTagNameEnd;
		   while((nTagEnd<nContent)&&(pContent[nTagEnd]!='>'))
			nTagEnd++;
		   nTagEnd++; 

		   const char* pTag=pContent+i;
		   size_t nTag=nTagEnd-i; 

		   i=nTagEnd; 

		   vector< pair<SZ_STRING,SZ_STRING> > Attribs; 

		   if((strTagName.cbData ==6)&&(strnicmp((char*)strTagName.pbData,"script",6)==0))//<script></script>
		   {
			size_t nScriptStart=i;
			if(nContent>8)
			while(i<nContent-8)
			{
			 if(strnicmp((char*)pContent+i,"</script",8)==0)
			 {
			  OnStartTag(strTagName,Attribs);
			  SZ_STRING strComment;
			  strComment.pbData = pContent+nScriptStart;
			  strComment.cbData = i-nScriptStart;
			  OnComment(strComment);
			  OnEndTag(strTagName);
			  i+=8;
			  break;
			 }
			 i++;
			}
			while((i<nContent)&&(pContent[i]!='>'))
			 i++;
			i++;
			continue;
		   } 

		   if(strTagName.pbData[0]=='/')
		   {
			strTagName.pbData +=1;
			strTagName.cbData -=1;
			OnEndTag(strTagName);
			continue;
		   } 

		   size_t m=strTagName.cbData+1;
		   do
		   {
			while((m<nTag)&&(((char)pTag[m]==' ')||((char)pTag[m]=='\r')||((char)pTag[m]=='\n')||((char)pTag[m]=='\t')))
			 m++; 

			size_t nAttribStart=m; 

			while((m<nTag)&&((char)pTag[m]!='='))
			 m++; 

			size_t nAttribStop=m-1;
			while((nAttribStop>0)&&((pTag[nAttribStop]==' ')||(pTag[nAttribStop]=='\r')||(pTag[nAttribStop]=='\n')||(pTag[nAttribStop]=='\t')))
			  nAttribStop--;
			if(nAttribStop>0)
			 nAttribStop++; 

			SZ_STRING strAttribName;
			strAttribName.pbData =pTag+nAttribStart;
			strAttribName.cbData =nAttribStop-nAttribStart; 

			m++;
			bool bStartWithDQ=false;
			bool bStartWithQ=false;
			bool bStartWithSpace=false;
			if((m<nTag)&&((char)pTag[m]=='\"'))
			{
			 bStartWithDQ=true;
			 m++;
			}else if((m<nTag)&&((char)pTag[m]=='\''))
			{
			 bStartWithQ=true;
			 m++;
			}else
			 bStartWithSpace=true; 

			if(m>=nTag)
			 continue; 

			size_t nValueStart=m; 

			while((m<nTag)
			 &&((char)pTag[m]!='>')
			 &&((char)pTag[m]!='\r')
			 &&((char)pTag[m]!='\n')
			 &&(!bStartWithDQ||((char)pTag[m]!='\"'))
			 &&(!bStartWithQ||((char)pTag[m]!='\''))
			 &&(!bStartWithSpace||((char)pTag[m]!=' ')))
			 m++; 

			SZ_STRING strAttribValue;
			strAttribValue.pbData =pTag+nValueStart;
			strAttribValue.cbData =m-nValueStart; 

			if(strAttribName.cbData !=0)
			 Attribs.push_back(pair<SZ_STRING,SZ_STRING>(strAttribName,strAttribValue)); 

			while((m<nTag)&&((char)pTag[m]!=' '))
			 m++; 

		   }while(m<nTag); 

		   OnStartTag(strTagName,Attribs);
		  }else
		  {
		   while((i<nContent)&&((pContent[i]==' ')||(pContent[i]=='\r')||(pContent[i]=='\n')||(pContent[i]=='\t')))
			i++;
		   size_t nTextBegin=i;
		   while((i<nContent)&&(pContent[i]!='<'))
			i++;
		   size_t nTextEnd=i; 

		   while((nTextEnd>=nTextBegin)&&((pContent[nTextEnd-1]==' ')||(pContent[nTextEnd-1]=='\r')||(pContent[nTextEnd-1]=='\n')||(pContent[nTextEnd-1]=='\t')))
			nTextEnd--;
		   if(nTextEnd<=nTextBegin)
			continue;
		   SZ_STRING strData;
		   strData.pbData =pContent+nTextBegin;
		   strData.cbData =nTextEnd-nTextBegin;
		   OnData(strData);
		  }
		 }
		 return true;
	}

	 //Util api for get a absolute url based on current page
	 void Relativity2AbsoluteURL(string& URL)
	{
		 int pos=-1;
		 pos=URL.rfind("#");
		 if(pos!=string::npos)
		 {
		  URL=URL.substr(0,pos);
		 } 

		 do
		 {
		  pos=URL.find("&amp;");
		  if(pos!=string::npos)
		   URL=URL.substr(0,pos)+"&"+URL.substr(pos+5);
		 }while(pos>=0); 

		 do
		 {
		  pos=URL.find("&gt;");
		  if(pos!=string::npos)
		   URL=URL.substr(0,pos)+">"+URL.substr(pos+4);
		 }while(pos>=0); 

		 do
		 {
		  pos=URL.find("&lt;");
		  if(pos!=string::npos)
		   URL=URL.substr(0,pos)+"<"+URL.substr(pos+4);
		 }while(pos>=0); 

		 if((URL.length()>=1)&&(URL[0]=='/'))
		 {
		  URL=m_szBaseDomain+URL;
		 }else if((URL.length()>=7)&&(strnicmp((char*)URL.c_str(),"http://",7)==0))
		 {
		  return;
		 }else
		 {
		  URL=m_szBaseURL+URL;
		 }
	} 

	 //event when a tag begin, such as '<a href=..' , then strTagName is 'a', Attribs contains 'href'
	 virtual void OnStartTag(const SZ_STRING & strTagName,vector< pair<SZ_STRING,SZ_STRING> > Attribs){};
	 //event when a tag close, such as '</a>', then strTagName is 'a'
	 virtual void OnEndTag(const SZ_STRING & strTagName){};
	 //event when text between tags, such as '<>hello<>', then strData is 'hello'
	 virtual void OnData(const SZ_STRING & strData){};
	 //event when script or comment, such as '<!-- .../-->' or '<script ..> </script>'
	 virtual void OnComment(const SZ_STRING & strComment){};
private:
	 char m_szBaseURL[1024];
	 char m_szBaseDomain[1024];
}; 

#endif
