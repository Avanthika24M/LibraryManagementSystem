// Library Management System - library.cpp
// C++20 implementation; no external database or testing dependencies.

#include "library.hpp"
#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
namespace lms {
    namespace {
    // Check whether an active account has staff-level permissions.
        bool privileged(const User& u) {
            return u.active&&(u.role=="ADMIN"||u.role=="STAFF");
        }
    // Check whether an active account has administrator permissions.
        bool admin(const User& u) {
            return u.active&&u.role=="ADMIN";
        }
    // Reject empty, oversized, or multiline record fields.
        bool validText(const std::string& s) {
            return !s.empty()&&s.size()<=120&&s.find('\n')==std::string::npos;
        }
    // Convert a validated date into a day count.
        int days(const std::string& date) {
            int y=0,m=0,d=0;
            char x=0,z=0;
            std::istringstream in(date);
            in>>y>>x>>m>>z>>d;
            std::tm tm {
            };
            tm.tm_year=y-1900;
            tm.tm_mon=m-1;
            tm.tm_mday=d;
            tm.tm_hour=12;
            auto t=std::mktime(&tm);
            return static_cast<int>(t/86400);
        }
    }
    // Validate a date in YYYY-MM-DD format.
    bool Library::validDate(const std::string& date) {
        if(date.size()!=10||date[4]!='-'||date[7]!='-')return false;
        for(size_t i=0;i<date.size();++i)if(i!=4&&i!=7&&(date[i]<'0'||date[i]>'9'))return false;
        int y=std::stoi(date.substr(0,4)),m=std::stoi(date.substr(5,2)),d=std::stoi(date.substr(8,2));
        if(y<2000||y>2100||m<1||m>12||d<1||d>31)return false;
        std::tm tm {
        };
        tm.tm_year=y-1900;
        tm.tm_mon=m-1;
        tm.tm_mday=d;
        tm.tm_hour=12;
        if(std::mktime(&tm)==-1)return false;
        return tm.tm_year==y-1900&&tm.tm_mon==m-1&&tm.tm_mday==d;
    }
    // Calculate the late fee at five units per overdue day.
    int Library::overdueFine(const std::string& due,const std::string& returned) {
        if(!validDate(due)||!validDate(returned))return -1;
        return std::max(0,days(returned)-days(due))*5;
    }
    // Create the initial administrator account on first launch.
    bool Library::createAdmin(const std::string& username,const std::string& password) {
        if(!users.empty()||!validText(username)||password.size()<10)return false;
        User u;
        u.id=nextUser++;
        u.username=username;
        u.salt=newSalt();
        u.passwordHash=hashPassword(password,u.salt);
        u.role="ADMIN";
        users[u.id]=u;
        return true;
    }
    // Verify credentials against the saved password hash.
    const User* Library::login(const std::string& username,const std::string& password)const {
        for(const auto& [id,u]:users) {
            (void)id;
            if(u.username==username&&u.active) {
                auto candidate=hashPassword(password,u.salt);
                unsigned diff=0;
                for(size_t i=0;i<candidate.size()&&i<u.passwordHash.size();++i)diff|=static_cast<unsigned>(candidate[i]^u.passwordHash[i]);
                if(diff==0&&candidate.size()==u.passwordHash.size())return &u;
            }
        }
        return nullptr;
    }
    // Create an account after checking administrator permissions.
    bool Library::addUser(const User& actor,const std::string& username,const std::string& password,const std::string& role) {
        if(!admin(actor)||!validText(username)||password.size()<10||password.size()>128||(role!="ADMIN"&&role!="STAFF"&&role!="MEMBER"))return false;
        for(const auto& [id,u]:users) {
            (void)id;
            if(u.username==username)return false;
        }
        User u;
        u.id=nextUser++;
        u.username=username;
        u.salt=newSalt();
        u.passwordHash=hashPassword(password,u.salt);
        u.role=role;
        users[u.id]=u;
        return true;
    }
    // Enable or disable a user account.
    bool Library::setUserActive(const User& actor,int userId,bool active) {
        if(!admin(actor)||actor.id==userId||!users.count(userId))return false;
        users[userId].active=active;
        return true;
    }
    // Add a new book and initialize its available inventory.
    bool Library::addBook(const User& actor,const Book& b) {
        if(!privileged(actor)||b.id<=0||books.count(b.id)||!validText(b.title)||!validText(b.author)||!validText(b.category)||b.total<0||b.total>100000)return false;
        Book copy=b;
        copy.available=b.total;
        books[b.id]=copy;
        return true;
    }
    // Update book details without invalidating outstanding loans.
    bool Library::updateBook(const User& actor,const Book& b) {
        auto it=books.find(b.id);
        if(!privileged(actor)||it==books.end()||!validText(b.title)||!validText(b.author)||!validText(b.category)||b.total<0||b.total>100000)return false;
        int borrowed=it->second.total-it->second.available;
        if(b.total<borrowed)return false;
        Book copy=b;
        copy.available=b.total-borrowed;
        it->second=copy;
        return true;
    }
    // Delete a book only when no copies are borrowed or requested.
    bool Library::deleteBook(const User& actor,int bookId) {
        auto it=books.find(bookId);
        if(!privileged(actor)||it==books.end()||it->second.total!=it->second.available)return false;
        for(const auto& [id,r]:requests) {
            (void)id;
            if(r.bookId==bookId&&r.status=="PENDING")return false;
        }
        books.erase(it);
        return true;
    }
    // Record a member request for an available book.
    bool Library::requestBook(const User& actor,int bookId) {
        if(!actor.active||actor.role!="MEMBER"||!books.count(bookId)||books.at(bookId).available<=0)return false;
        for(const auto& [id,r]:requests) {
            (void)id;
            if(r.bookId==bookId&&r.memberId==actor.id&&r.status=="PENDING")return false;
        }
        Request r;
        r.id=nextRequest++;
        r.bookId=bookId;
        r.memberId=actor.id;
        requests[r.id]=r;
        return true;
    }
    // Approve a pending request and create a corresponding loan.
    bool Library::approveRequest(const User& actor,int requestId,const std::string& issued,const std::string& due) {
        auto it=requests.find(requestId);
        if(!privileged(actor)||it==requests.end()||it->second.status!="PENDING"||!validDate(issued)||!validDate(due)||due<issued)return false;
        auto b=books.find(it->second.bookId);
        auto m=users.find(it->second.memberId);
        if(b==books.end()||b->second.available<=0||m==users.end()||!m->second.active)return false;
        Loan loan;
        loan.id=nextLoan++;
        loan.bookId=b->first;
        loan.memberId=m->first;
        loan.issued=issued;
        loan.due=due;
        loans[loan.id]=loan;
        --b->second.available;
        it->second.status="APPROVED";
        return true;
    }
    // Reject a pending borrowing request.
    bool Library::rejectRequest(const User& actor,int requestId) {
        auto it=requests.find(requestId);
        if(!privileged(actor)||it==requests.end()||it->second.status!="PENDING")return false;
        it->second.status="REJECTED";
        return true;
    }
    // Close a loan, restore inventory, and calculate any late fee.
    bool Library::returnBook(const User& actor,int loanId,const std::string& returned,int& fine) {
        auto it=loans.find(loanId);
        if(!privileged(actor)||it==loans.end()||!it->second.returned.empty()||!validDate(returned)||returned<it->second.issued)return false;
        auto b=books.find(it->second.bookId);
        if(b==books.end())return false;
        fine=overdueFine(it->second.due,returned);
        it->second.returned=returned;
        ++b->second.available;
        return true;
    }
}
