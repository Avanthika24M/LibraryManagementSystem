// Library Management System - storage.cpp
// C++20 implementation; no external database or testing dependencies.

#include "library.hpp"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
namespace lms {
    // Write all records to a temporary file and replace the stored snapshot.
    bool Library::save(const std::string& path)const {
        namespace fs=std::filesystem;
        fs::path target(path),tmp=target;
        tmp+=".tmp";
        std::error_code ec;
        if(!target.parent_path().empty())fs::create_directories(target.parent_path(),ec);
        if(ec)return false;
        std::ofstream out(tmp,std::ios::trunc);
        if(!out)return false;
        out<<"LMS2\n";
        for(const auto& [id,u]:users)out<<"U "<<id<<' '<<std::quoted(u.username)<<' '<<std::quoted(u.salt)<<' '<<std::quoted(u.passwordHash)<<' '<<std::quoted(u.role)<<' '<<u.active<<'\n';
        for(const auto& [id,b]:books)out<<"B "<<id<<' '<<std::quoted(b.title)<<' '<<std::quoted(b.author)<<' '<<std::quoted(b.category)<<' '<<b.total<<' '<<b.available<<'\n';
        for(const auto& [id,r]:requests)out<<"R "<<id<<' '<<r.bookId<<' '<<r.memberId<<' '<<std::quoted(r.status)<<'\n';
        for(const auto& [id,l]:loans)out<<"L "<<id<<' '<<l.bookId<<' '<<l.memberId<<' '<<std::quoted(l.issued)<<' '<<std::quoted(l.due)<<' '<<std::quoted(l.returned)<<'\n';
        out.flush();
        if(!out)return false;
        out.close();
        if(!out)return false;
        fs::rename(tmp,target,ec);
        if(ec) {
            fs::remove(tmp);
            return false;
        }
        return true;
    }
    // Load and validate saved records without modifying live state on failure.
    bool Library::load(const std::string& path) {
        std::ifstream in(path);
        if(!in)return !std::filesystem::exists(path);
        std::string version;
        if(!(in>>version)||version!="LMS2")return false;
        Library parsed;
        std::string kind;
        while(in>>kind) {
            if(kind=="U") {
                User u;
                if(!(in>>u.id>>std::quoted(u.username)>>std::quoted(u.salt)>>std::quoted(u.passwordHash)>>std::quoted(u.role)>>u.active)||u.id<=0||parsed.users.count(u.id))return false;
                parsed.nextUser=std::max(parsed.nextUser,u.id+1);
                parsed.users[u.id]=u;
            }
            else if(kind=="B") {
                Book b;
                if(!(in>>b.id>>std::quoted(b.title)>>std::quoted(b.author)>>std::quoted(b.category)>>b.total>>b.available)||b.id<=0||b.total<0||b.available<0||b.available>b.total||parsed.books.count(b.id))return false;
                parsed.books[b.id]=b;
            }
            else if(kind=="R") {
                Request r;
                if(!(in>>r.id>>r.bookId>>r.memberId>>std::quoted(r.status))||r.id<=0||parsed.requests.count(r.id))return false;
                parsed.nextRequest=std::max(parsed.nextRequest,r.id+1);
                parsed.requests[r.id]=r;
            }
            else if(kind=="L") {
                Loan l;
                if(!(in>>l.id>>l.bookId>>l.memberId>>std::quoted(l.issued)>>std::quoted(l.due)>>std::quoted(l.returned))||l.id<=0||parsed.loans.count(l.id))return false;
                parsed.nextLoan=std::max(parsed.nextLoan,l.id+1);
                parsed.loans[l.id]=l;
            }
            else return false;
        }
        if(!in.eof())return false;
        for(const auto& [id,l]:parsed.loans) {
            (void)id;
            if(!parsed.books.count(l.bookId)||!parsed.users.count(l.memberId))return false;
        }
        for(const auto& [id,r]:parsed.requests) {
            (void)id;
            if(!parsed.books.count(r.bookId)||!parsed.users.count(r.memberId))return false;
        }
        *this=std::move(parsed);
        return true;
    }
}
