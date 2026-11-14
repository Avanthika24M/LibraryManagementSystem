// Library Management System - main.cpp
// C++20 implementation; no external database or testing dependencies.

#include "library.hpp"
#include <iostream>
#include <limits>
#include <string>
#include <termios.h>
#include <unistd.h>
#include <cstdio>
using namespace lms;
namespace {
    // Read one line of user input from the console.
    std::string ask(const std::string& label) {
        std::cout << label;
        std::string s;
        std::getline(std::cin,s);
        return s;
    }
    // Read passwords without displaying their characters; show one * per character.
    // Mask password input with asterisks in an interactive Linux terminal.
    std::string askPassword(const std::string& label) {
        std::cout << label<<std::flush;
        if(!isatty(STDIN_FILENO)) {
            std::string password;
            std::getline(std::cin,password);
            return password;
        }
        termios original {
        };
        if(tcgetattr(STDIN_FILENO,&original)!=0) {
            std::string password;
            std::getline(std::cin,password);
            return password;
        }
        termios hidden=original;
        hidden.c_lflag &= static_cast<tcflag_t>(~(ECHO|ICANON));
        if(tcsetattr(STDIN_FILENO,TCSANOW,&hidden)!=0) {
            std::string password;
            std::getline(std::cin,password);
            return password;
        }
        std::string password;
        char ch=0;
        while(read(STDIN_FILENO,&ch,1)==1) {
            if(ch=='\n'||ch=='\r')break;
            if(ch==127||ch=='\b') {
                if(!password.empty()) {
                    password.pop_back();
                    std::cout << "\b \b"<<std::flush;
                }
            }
            else if(ch==3) {
                password.clear();
                break;
            }
            else if(static_cast<unsigned char>(ch)>=32 && password.size()<256) {
                password.push_back(ch);
                std::cout << '*'<<std::flush;
            }
        }
        tcsetattr(STDIN_FILENO,TCSANOW,&original);
        std::cout << '\n';
        return password;
    }
    // Convert a user-entered value into an integer; reject invalid input.
    int number(const std::string& label) {
        try {
            auto s=ask(label);
            size_t n=0;
            int v=std::stoi(s,&n);
            return n==s.size()?v:-1;
        }
        catch(...) {
            return -1;
        }
    }
    // Display the current book inventory.
    void books(const Library& db) {
        for(const auto& [id,b]:db.allBooks())std::cout << id<<" | "<<b.title<<" | "<<b.author<<" | "<<b.category<<" | "<<b.available<<"/"<<b.total<<"\n";
    }
    // Display account details without exposing password hashes.
    void users(const Library& db) {
        for(const auto& [id,u]:db.allUsers())std::cout << id<<" | "<<u.username<<" | "<<u.role<<" | "<<(u.active?"ACTIVE":"DISABLED")<<"\n";
    }
    // Display borrowing requests for staff review.
    void requests(const Library& db) {
        for(const auto& [id,r]:db.allRequests())std::cout << id<<" | book "<<r.bookId<<" | member "<<r.memberId<<" | "<<r.status<<"\n";
    }
    // Display loan history, optionally filtered to one member.
    void loans(const Library& db,int member=0) {
        for(const auto& [id,l]:db.allLoans())if(member==0||l.memberId==member)std::cout << id<<" | book "<<l.bookId<<" | member "<<l.memberId<<" | "<<l.issued<<" -> "<<l.due<<" | returned "<<(l.returned.empty()?"NO":l.returned)<<"\n";
    }
    // Display only the operations available to the signed-in role.
    void menu(const User& user) {
        std::cout << "\nLogged in: "<<user.username<<" ["<<user.role<<"]\n1 List books\n2 Search book\n";
        if(user.role=="ADMIN"||user.role=="STAFF")std::cout << "3 Add book\n4 Update book\n5 Delete book\n6 View requests\n7 Approve request\n8 Reject request\n9 Return book\n10 View loans\n";
        if(user.role=="ADMIN")std::cout << "11 Add user\n12 View users\n13 Enable/disable user\n";
        if(user.role=="MEMBER")std::cout << "3 Request book\n4 My requests\n5 My loans\n";
        std::cout << "0 Logout\n";
    }
}
    // Initialize storage, authenticate users, and dispatch menu actions.
int main() {
    const std::string path="data/library.records";
    Library db;
    if(!db.load(path)) {
        std::cerr << "Cannot load records: invalid or corrupted data file.\n";
        return 1;
    }
    if(!db.initialized()) {
        std::cout << "FIRST RUN: Create administrator (password at least 10 characters).\n";
        std::string firstUser=ask("Admin username: ");
        std::string firstPassword=askPassword("Admin password: ");
        if(!db.createAdmin(firstUser,firstPassword)||!db.save(path)) {
            std::cerr << "Administrator setup failed.\n";
            return 1;
        }
    }
    while(true) {
        std::cout << "\nLIBRARY MANAGEMENT SYSTEM\n";
        std::string name=ask("Username (or EXIT): ");
        if(name=="EXIT")break;
        std::string pass=askPassword("Password: ");
        const User* logged=db.login(name,pass);
        if(!logged) {
            std::cout << "Invalid credentials or disabled account.\n";
            continue;
        }
        int userId=logged->id;
        while(true) {
            auto u=db.allUsers().find(userId);
            if(u==db.allUsers().end()||!u->second.active)break;
            const User& actor=u->second;
            menu(actor);
            int choice=number("Choice: ");
            if(choice==0)break;
            // Each role has its own sequential menu; map member selections to internal actions.
            if(actor.role=="MEMBER" && choice>=3 && choice<=5)choice+=11;
            bool changed=false,success=false;
            if(choice==1)books(db);
            else if(choice==2) {
                int id=number("Book ID: ");
                auto it=db.allBooks().find(id);
                if(it==db.allBooks().end())std::cout << "Not found\n";
                else std::cout << it->second.title<<" | available "<<it->second.available<<"\n";
            }
            else if((actor.role=="ADMIN"||actor.role=="STAFF")&&(choice==3||choice==4)) {
                Book b;
                b.id=number("Book ID: ");
                b.title=ask("Title: ");
                b.author=ask("Author: ");
                b.category=ask("Category: ");
                b.total=number("Total copies: ");
                changed=true;
                success=choice==3?db.addBook(actor,b):db.updateBook(actor,b);
            }
            else if((actor.role=="ADMIN"||actor.role=="STAFF")&&choice==5) {
                changed=true;
                success=db.deleteBook(actor,number("Book ID: "));
            }
            else if((actor.role=="ADMIN"||actor.role=="STAFF")&&choice==6)requests(db);
            else if((actor.role=="ADMIN"||actor.role=="STAFF")&&choice==7) {
                int id=number("Request ID: ");
                std::string issued=ask("Issue date YYYY-MM-DD: "),due=ask("Due date YYYY-MM-DD: ");
                changed=true;
                success=db.approveRequest(actor,id,issued,due);
            }
            else if((actor.role=="ADMIN"||actor.role=="STAFF")&&choice==8) {
                changed=true;
                success=db.rejectRequest(actor,number("Request ID: "));
            }
            else if((actor.role=="ADMIN"||actor.role=="STAFF")&&choice==9) {
                int id=number("Loan ID: ");
                std::string date=ask("Return date YYYY-MM-DD: ");
                int fine=0;
                changed=true;
                success=db.returnBook(actor,id,date,fine);
                if(success)std::cout << "Fine: "<<fine<<"\n";
            }
            else if((actor.role=="ADMIN"||actor.role=="STAFF")&&choice==10)loans(db);
            else if(actor.role=="ADMIN"&&choice==11) {
                std::string name=ask("New username: "),pass=askPassword("Password (10+ characters): "),role=ask("Role ADMIN/STAFF/MEMBER: ");
                changed=true;
                success=db.addUser(actor,name,pass,role);
            }
            else if(actor.role=="ADMIN"&&choice==12)users(db);
            else if(actor.role=="ADMIN"&&choice==13) {
                int id=number("User ID: ");
                int active=number("Enable=1, disable=0: ");
                changed=true;
                success=(active==0||active==1)&&db.setUserActive(actor,id,active==1);
            }
            else if(actor.role=="MEMBER"&&choice==14) {
                changed=true;
                success=db.requestBook(actor,number("Book ID: "));
            }
            else if(actor.role=="MEMBER"&&choice==15) {
                for(const auto& [id,r]:db.allRequests())if(r.memberId==actor.id)std::cout << id<<" | book "<<r.bookId<<" | "<<r.status<<"\n";
            }
            else if(actor.role=="MEMBER"&&choice==16)loans(db,actor.id);
            else std::cout << "Invalid option or permission denied.\n";
            if(changed) {
                std::cout << (success?"Success":"Rejected: invalid input, record state, or permission")<<"\n";
                if(success&&!db.save(path)) {
                    std::cerr << "ERROR: Failed to save changes. Stop to avoid further changes.\n";
                    return 1;
                }
            }
        }
    }
    return 0;
}
