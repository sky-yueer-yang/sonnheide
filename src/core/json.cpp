#include "sonnheide.hpp"
#include <algorithm>
#include <charconv>
#include <limits>
#include <sstream>
namespace sonn {
Error::Error(std::string c,std::string d):std::runtime_error(c+": "+d),code(std::move(c)){}
namespace {
#include "unicode_tables.inc"
std::vector<uint32_t> decode_utf8(std::string_view s) {
 std::vector<uint32_t> out;
 for(size_t i=0;i<s.size();) {
  uint8_t c=static_cast<uint8_t>(s[i++]); uint32_t v; int n;
  if(c<128){v=c;n=0;} else if(c>=0xc2&&c<=0xdf){v=c&31;n=1;} else if(c>=0xe0&&c<=0xef){v=c&15;n=2;} else if(c>=0xf0&&c<=0xf4){v=c&7;n=3;} else throw Error("INVALID_UTF8","invalid leading byte");
  if(i+static_cast<size_t>(n)>s.size()) throw Error("INVALID_UTF8","truncated scalar");
  for(int j=0;j<n;j++){uint8_t d=static_cast<uint8_t>(s[i++]);if((d&0xc0)!=0x80)throw Error("INVALID_UTF8","continuation");v=(v<<6)|(d&63);}
  if((n==1&&v<128)||(n==2&&v<2048)||(n==3&&v<65536)||v>0x10ffff||(v>=0xd800&&v<=0xdfff))throw Error("INVALID_UTF8","non scalar or overlong"); out.push_back(v);
 }
 return out;
}
void append_utf8(std::string& s,uint32_t c){if(c<128)s.push_back(char(c));else if(c<2048){s.push_back(char(0xc0|(c>>6)));s.push_back(char(0x80|(c&63)));}else if(c<65536){s.push_back(char(0xe0|(c>>12)));s.push_back(char(0x80|((c>>6)&63)));s.push_back(char(0x80|(c&63)));}else{s.push_back(char(0xf0|(c>>18)));s.push_back(char(0x80|((c>>12)&63)));s.push_back(char(0x80|((c>>6)&63)));s.push_back(char(0x80|(c&63)));}}
uint8_t ccc(uint32_t c){auto i=std::lower_bound(std::begin(combining_classes),std::end(combining_classes),c,[](auto r,auto x){return r.cp<x;});return i!=std::end(combining_classes)&&i->cp==c?i->ccc:0;}
void decompose(uint32_t c,std::vector<uint32_t>& out){
 if(c>=0xac00&&c<0xac00+11172){uint32_t s=c-0xac00;out.push_back(0x1100+s/588);out.push_back(0x1161+(s%588)/28);if(s%28)out.push_back(0x11a7+s%28);return;}
 auto i=std::lower_bound(std::begin(decompositions),std::end(decompositions),c,[](auto r,auto x){return r.cp<x;});if(i!=std::end(decompositions)&&i->cp==c){decompose(i->a,out);if(i->b)decompose(i->b,out);}else out.push_back(c);
}
uint32_t compose(uint32_t a,uint32_t b){
 if(a>=0x1100&&a<0x1113&&b>=0x1161&&b<0x1176)return 0xac00+((a-0x1100)*21+b-0x1161)*28;
 if(a>=0xac00&&a<0xac00+11172&&(a-0xac00)%28==0&&b>0x11a7&&b<0x11c3)return a+b-0x11a7;
 auto key=std::pair(a,b);auto i=std::lower_bound(std::begin(compositions),std::end(compositions),key,[](auto r,auto x){return std::pair(r.a,r.b)<x;});return i!=std::end(compositions)&&i->a==a&&i->b==b?i->cp:0;
}
void validate_string(std::string_view s){
 if(s.size()>65536)throw Error("JSON_LIMIT","string bytes");auto raw=decode_utf8(s);std::vector<uint32_t>d;for(auto c:raw)decompose(c,d);
 for(size_t i=1;i<d.size();i++){auto k=ccc(d[i]);if(!k)continue;size_t j=i;while(j&&ccc(d[j-1])>k){std::swap(d[j],d[j-1]);--j;}}
 std::vector<uint32_t> n;size_t starter=0;uint8_t last=0;for(auto c:d){uint8_t cls=ccc(c);uint32_t v=n.empty()?0:compose(n[starter],c);if(v&&(last<cls||last==0)){n[starter]=v;}else{if(cls==0)starter=n.size();n.push_back(c);last=cls;}}
 if(raw!=n)throw Error("NON_NFC","string must already be Unicode NFC");
}
class Parser {
 std::string_view s;size_t pos=0;
 [[noreturn]]void fail(std::string_view reason){throw Error("INVALID_JSON",std::string(reason)+" at "+std::to_string(pos));}
 void whitespace(){while(pos<s.size()&&(s[pos]==' '||s[pos]=='\n'||s[pos]=='\r'||s[pos]=='\t'))++pos;}
 bool take(char c){whitespace();if(pos<s.size()&&s[pos]==c){++pos;return true;}return false;}
 uint32_t u4(){if(pos+4>s.size())fail("truncated unicode escape");uint32_t c=0;for(int i=0;i<4;++i){char x=s[pos++];int v=x>='0'&&x<='9'?x-'0':x>='a'&&x<='f'?x-'a'+10:x>='A'&&x<='F'?x-'A'+10:-1;if(v<0)fail("unicode escape");c=(c<<4)|uint32_t(v);}return c;}
 std::string str(){if(!take('"'))fail("string");std::string out;while(pos<s.size()){uint8_t c=static_cast<uint8_t>(s[pos++]);if(c=='"'){validate_string(out);return out;}if(c<32)fail("control in string");if(c!='\\'){out.push_back(char(c));continue;}if(pos==s.size())fail("truncated escape");char e=s[pos++];switch(e){case '"':out+='"';break;case '\\':out+='\\';break;case '/':out+='/';break;case 'b':out+='\b';break;case 'f':out+='\f';break;case 'n':out+='\n';break;case 'r':out+='\r';break;case 't':out+='\t';break;case 'u':{auto v=u4();if(v>=0xd800&&v<=0xdbff){if(pos+2>s.size()||s[pos++]!='\\'||s[pos++]!='u')fail("surrogate pair");auto b=u4();if(b<0xdc00||b>0xdfff)fail("surrogate pair");v=0x10000+(v-0xd800)*1024+b-0xdc00;}else if(v>=0xdc00&&v<=0xdfff)fail("unpaired surrogate");append_utf8(out,v);break;}default:fail("escape");}if(out.size()>65536)throw Error("JSON_LIMIT","string");}fail("unterminated string");}
 Json value(unsigned depth){if(depth>64)throw Error("JSON_LIMIT","depth");whitespace();if(pos==s.size())fail("missing value");char c=s[pos];if(c=='"')return Json(str());if(c=='{'){++pos;Json::Object o;if(take('}'))return o;do{auto k=str();if(!take(':'))fail("colon");auto v=value(depth+1);if(!o.emplace(std::move(k),std::move(v)).second)throw Error("DUPLICATE_KEY","object key");}while(take(','));if(!take('}'))fail("object close");return o;}if(c=='['){++pos;Json::Array a;if(take(']'))return a;do{if(a.size()>=1048576)throw Error("JSON_LIMIT","array");a.push_back(value(depth+1));}while(take(','));if(!take(']'))fail("array close");return a;}
 for(auto lit:{std::string_view("true"),std::string_view("false"),std::string_view("null")})if(s.substr(pos,lit.size())==lit){pos+=lit.size();if(lit=="null")return Json();return Json(lit=="true");}
 size_t begin=pos;if(c=='-')++pos;if(pos==s.size()||s[pos]<'0'||s[pos]>'9')fail("integer required");if(s[pos]=='0'){++pos;if(pos<s.size()&&s[pos]>='0'&&s[pos]<='9')fail("leading zero");}else while(pos<s.size()&&s[pos]>='0'&&s[pos]<='9')++pos;
 if(pos<s.size()&&(s[pos]=='.'||s[pos]=='e'||s[pos]=='E'))throw Error("JSON_FLOAT","authority JSON permits integers only");int64_t n=0;auto r=std::from_chars(s.data()+begin,s.data()+pos,n);if(r.ec!=std::errc()||r.ptr!=s.data()+pos)throw Error("INTEGER_OVERFLOW","JSON int64");return n;
 }
public:explicit Parser(std::string_view x):s(x){} Json run(){auto v=value(0);whitespace();if(pos!=s.size())fail("trailing content");return v;}
};
void quote(std::string& s,const std::string& v){validate_string(v);s+='"';constexpr char h[]="0123456789abcdef";for(uint8_t c:v){switch(c){case '"':s+="\\\"";break;case '\\':s+="\\\\";break;case '\b':s+="\\b";break;case '\f':s+="\\f";break;case '\n':s+="\\n";break;case '\r':s+="\\r";break;case '\t':s+="\\t";break;default:if(c<32){s+="\\u00";s+=h[c>>4];s+=h[c&15];}else s+=char(c);}}s+='"';}
void emit(std::string& s,const Json& j,unsigned depth){if(depth>64)throw Error("JSON_LIMIT","depth");std::visit([&](const auto& v){using T=std::decay_t<decltype(v)>;if constexpr(std::is_same_v<T,std::nullptr_t>)s+="null";else if constexpr(std::is_same_v<T,bool>)s+=v?"true":"false";else if constexpr(std::is_same_v<T,int64_t>)s+=std::to_string(v);else if constexpr(std::is_same_v<T,std::string>)quote(s,v);else if constexpr(std::is_same_v<T,Json::Array>){s+='[';bool first=true;for(auto& x:v){if(!first)s+=',';first=false;emit(s,x,depth+1);}s+=']';}else{s+='{';bool first=true;for(auto& [k,x]:v){if(!first)s+=',';first=false;quote(s,k);s+=':';emit(s,x,depth+1);}s+='}';}},j.value);}
}
const Json& Json::at(std::string_view k)const{auto& o=object();auto i=o.find(k);if(i==o.end())throw Error("JSON_FIELD","missing "+std::string(k));return i->second;}
const Json::Array& Json::array()const{if(auto p=std::get_if<Array>(&value))return *p;throw Error("JSON_TYPE","array");}
const Json::Object& Json::object()const{if(auto p=std::get_if<Object>(&value))return *p;throw Error("JSON_TYPE","object");}
const std::string& Json::string()const{if(auto p=std::get_if<std::string>(&value))return *p;throw Error("JSON_TYPE","string");}
int64_t Json::integer()const{if(auto p=std::get_if<int64_t>(&value))return *p;throw Error("JSON_TYPE","integer");}
bool Json::boolean()const{if(auto p=std::get_if<bool>(&value))return *p;throw Error("JSON_TYPE","boolean");}
Json parse_json(std::string_view s){if(s.size()>134217728)throw Error("JSON_LIMIT","segment bytes");return Parser(s).run();}
std::string canonical_json(const Json& j){std::string s;emit(s,j,0);return s;}
} // sonn
