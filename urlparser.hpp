/*
      file: urlparser.h
      desc: parse a html page, get each url and its link text
      author: chen hua 
      create: 2005-03-16
*/
#ifndef _URL_PARSER_H_
#define _URL_PARSER_H_ 

#include "basehtmlparser.h"
#include <map>
#include <string>

using namespace std; 

class CURLParser: public CBaseHtmlParser
{
	public:
	 CURLParser(){m_bInTagA=false;}
	 bool Parse(const string& URL,const string& Content)
	 {
	 m_URL2Text.clear();
	 return CBaseHtmlParser::Parse(URL,Content);
	 } 

	 
	public: 
	 map<string,string> m_URL2Text;
	 
	private:
	 bool m_bInTagA;
	 string m_strCurURL;
	 void OnStartTag(const SZ_STRING & strTagName,vector< pair<SZ_STRING,SZ_STRING> > Attribs)
	{
	 if((strTagName.cbData==1)&&(strnicmp(strTagName.pbData,"A",strTagName.cbData)==0))
	 {
	  m_bInTagA=true;
	  m_strCurURL.clear();
	  for(size_t i=0;i<Attribs.size();i++)
	  {
	   SZ_STRING x=Attribs[i].first;
	   if((Attribs[i].first.cbData==4)&&(strnicmp(Attribs[i].first.pbData,"href",Attribs[i].first.cbData)==0))
	   {
		m_strCurURL=string(Attribs[i].second.pbData,Attribs[i].second.cbData);
		Relativity2AbsoluteURL(m_strCurURL);
		break;
	   }
	  }
	 }
	} 
	 void OnEndTag(const SZ_STRING & strTagName)
	{
	 if((strTagName.cbData==1)&&(strnicmp(strTagName.pbData,"A",strTagName.cbData)==0))
	 {
	  m_bInTagA=false;
	 }
	 if((strTagName.cbData==2)&&(strnicmp(strTagName.pbData,"td",strTagName.cbData)==0))
	 {
	  m_bInTagA=false;
	 }
	} 
	 void OnData(const SZ_STRING & strData)
	{
	 if(m_bInTagA)
	 {
	  if(!m_strCurURL.empty())
	   m_URL2Text[m_strCurURL]=m_URL2Text[m_strCurURL]+string(strData.pbData,strData.cbData);
	 }
	}
}; 

#endif 
