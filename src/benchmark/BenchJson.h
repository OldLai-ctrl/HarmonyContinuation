#pragma once
#include <charconv>
#include <cmath>
#include <iomanip>
#include <locale>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace harmony::benchmark::json {
struct Value {
    enum class Type { Null, Boolean, Number, String, Array, Object } type{Type::Null};
    bool boolean{};
    double number{};
    std::string string;
    std::vector<Value> array;
    std::map<std::string,Value> object;
    Value()=default;
    Value(bool b):type(Type::Boolean),boolean(b){}
    Value(double n):type(Type::Number),number(n){}
    Value(int n):Value(static_cast<double>(n)){}
    Value(std::string s):type(Type::String),string(std::move(s)){}
    Value(const char* s):Value(std::string(s)){}
    static Value list(){Value v;v.type=Type::Array;return v;}
    static Value map(){Value v;v.type=Type::Object;return v;}
    const Value& at(std::string_view key) const {
        if(type!=Type::Object)throw std::runtime_error("expected JSON object");
        const auto it=object.find(std::string(key));
        if(it==object.end())throw std::runtime_error("missing JSON field: "+std::string(key));
        return it->second;
    }
    const Value* find(std::string_view key) const {
        if(type!=Type::Object)return nullptr;
        const auto it=object.find(std::string(key));return it==object.end()?nullptr:&it->second;
    }
    const std::string& text() const {
        if(type!=Type::String)throw std::runtime_error("expected JSON string");return string;
    }
    double finite() const {
        if(type!=Type::Number||!std::isfinite(number))throw std::runtime_error("expected finite JSON number");
        return number;
    }
    int integer(int low,int high) const {
        const auto n=finite();if(std::floor(n)!=n||n<low||n>high)throw std::runtime_error("JSON integer out of range");
        return static_cast<int>(n);
    }
    const std::vector<Value>& items() const {
        if(type!=Type::Array)throw std::runtime_error("expected JSON array");return array;
    }
};
inline std::string escape(std::string_view input) {
    std::string out="\"";constexpr char hex[]="0123456789abcdef";
    for(const unsigned char c:input) {
        if(c=='\"'||c=='\\'){out+='\\';out+=static_cast<char>(c);}
        else if(c=='\n')out+="\\n";
        else if(c=='\r')out+="\\r";
        else if(c=='\t')out+="\\t";
        else if(c<0x20){out+="\\u00";out+=hex[c>>4];out+=hex[c&15];}
        else out+=static_cast<char>(c);
    }
    return out+'"';
}
inline void append(std::ostream& out,const Value& v) {
    switch(v.type) {
        case Value::Type::Null:out<<"null";break;
        case Value::Type::Boolean:out<<(v.boolean?"true":"false");break;
        case Value::Type::Number:
            if(!std::isfinite(v.number))throw std::runtime_error("nonfinite JSON output");
            out<<std::setprecision(17)<<v.number;break;
        case Value::Type::String:out<<escape(v.string);break;
        case Value::Type::Array: {
            out<<'[';bool first=true;for(const auto& item:v.array){if(!first)out<<',';first=false;append(out,item);}out<<']';break;
        }
        case Value::Type::Object: {
            out<<'{';bool first=true;for(const auto& [key,item]:v.object){if(!first)out<<',';first=false;
                out<<escape(key)<<':';append(out,item);}out<<'}';break;
        }
    }
}
inline std::string dump(const Value& value) {
    std::ostringstream out;out.imbue(std::locale::classic());append(out,value);return out.str();
}
class Parser {
public:
    explicit Parser(std::string_view input):input_(input){if(input.size()>4*1024*1024)throw std::runtime_error("JSON exceeds 4 MiB");}
    Value parse(){auto value=read(0);space();if(at_!=input_.size())throw std::runtime_error("trailing JSON data");return value;}
private:
    std::string_view input_;std::size_t at_{};
    void space(){while(at_<input_.size()&&(input_[at_]==' '||input_[at_]=='\n'||input_[at_]=='\r'||input_[at_]=='\t'))++at_;}
    bool take(char c){space();if(at_<input_.size()&&input_[at_]==c){++at_;return true;}return false;}
    void need(char c){if(!take(c))throw std::runtime_error("invalid JSON punctuation");}
    std::string string(){
        need('"');std::string out;
        while(at_<input_.size()){
            const unsigned char c=static_cast<unsigned char>(input_[at_++]);
            if(c=='"')return out;
            if(c<0x20)throw std::runtime_error("invalid JSON control character");
            if(c!='\\'){out+=static_cast<char>(c);continue;}
            if(at_==input_.size())break;
            const char e=input_[at_++];
            if(e=='"'||e=='\\'||e=='/')out+=e;
            else if(e=='n')out+='\n';else if(e=='r')out+='\r';else if(e=='t')out+='\t';
            else if(e=='u'){
                if(at_+4>input_.size()||input_.substr(at_,2)!="00")throw std::runtime_error("unsupported JSON unicode escape");
                int n{};for(int i=2;i<4;++i){const char h=input_[at_+i];n*=16;
                    if(h>='0'&&h<='9')n+=h-'0';else if(h>='a'&&h<='f')n+=h-'a'+10;
                    else if(h>='A'&&h<='F')n+=h-'A'+10;else throw std::runtime_error("invalid JSON unicode escape");}
                out+=static_cast<char>(n);at_+=4;
            }else throw std::runtime_error("unsupported JSON escape");
        }
        throw std::runtime_error("unterminated JSON string");
    }
    Value read(int depth){
        if(depth>20)throw std::runtime_error("JSON nesting too deep");
        space();if(at_==input_.size())throw std::runtime_error("truncated JSON");
        if(take('{')){
            auto v=Value::map();if(take('}'))return v;
            do{const auto key=string();need(':');if(!v.object.emplace(key,read(depth+1)).second)
                    throw std::runtime_error("duplicate JSON key");
                if(v.object.size()>4096)throw std::runtime_error("JSON object too large");
                if(take('}'))return v;need(',');}while(true);
        }
        if(take('[')){
            auto v=Value::list();if(take(']'))return v;
            do{if(v.array.size()>=4096)throw std::runtime_error("JSON array too large");
                v.array.push_back(read(depth+1));if(take(']'))return v;need(',');}while(true);
        }
        if(input_[at_]=='"')return Value(string());
        if(input_.substr(at_,4)=="true"){at_+=4;return Value(true);}
        if(input_.substr(at_,5)=="false"){at_+=5;return Value(false);}
        if(input_.substr(at_,4)=="null"){at_+=4;return {};}
        double number{};const auto start=input_.data()+at_;
        const auto parsed=std::from_chars(start,input_.data()+input_.size(),number);
        if(parsed.ec!=std::errc{}||parsed.ptr==start||!std::isfinite(number))throw std::runtime_error("invalid JSON number");
        at_=static_cast<std::size_t>(parsed.ptr-input_.data());return Value(number);
    }
};
inline Value parse(std::string_view input){return Parser(input).parse();}
} // namespace harmony::benchmark::json
